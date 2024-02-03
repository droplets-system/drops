#include "drops/drops.hpp"

#include "helpers.cpp"
#include "ram.cpp"
#include "utils.cpp"

// DEBUG (used to help testing)
#ifdef DEBUG
#include "debug.cpp"
#endif

namespace dropssystem {

// @user
[[eosio::on_notify("*::transfer")]] int64_t
drops::on_transfer(const name from, const name to, const asset quantity, const string memo)
{
   // ignore RAM sales
   // ignore transfers not sent to this contract
   // ignore transfers sent from this contract
   if (from == "eosio.ram"_n || to != get_self() || from == get_self()) {
      return 0;
   }

   check(get_first_receiver() == "eosio.token"_n, "Only the eosio.token contract may send tokens to this contract.");
   check(quantity.symbol == EOS, "Only the system token is accepted for transfers.");
   check(!memo.empty(), ERROR_INVALID_MEMO);
   check_is_enabled(get_self());

   // Process the memo field by comma delimiting the string
   // Memo payload must contain to 2 fields
   const vector<string> parsed = utils::split(memo, ',');
   check(parsed.size() >= 2, ERROR_INVALID_MEMO);
   const string operation = parsed[0];

   // Buy RAM: "buyram,<receiver>"
   // If no receiver, the RAM would be credited to sender
   if (operation == "buyram") {
      const name receiver = utils::parse_name(parsed[1]);
      check(receiver == from, "Receiver must be the same as the sender.");

      // contract purchase bytes and credit to receiver
      const int64_t bytes = eosiosystem::bytes_cost_with_fee(quantity);
      buy_ram(quantity);
      add_ram_bytes(receiver, bytes);
      return bytes;
   }
   check(false, ERROR_INVALID_MEMO);
   return 0;
}

// @user
[[eosio::action]] int64_t drops::generate(const name owner, const bool bound, const uint32_t amount, const string data)
{
   require_auth(owner);
   check_is_enabled(get_self());
   const int64_t bytes = emplace_drops(owner, bound, amount, data);
   return bytes;
}

int64_t drops::emplace_drops(const name owner, const bool bound, const uint32_t amount, const string data)
{
   drop_table drops(get_self(), get_self().value);

   // Ensure amount is a positive value
   check(amount > 0, "The amount of drops to generate must be a positive value.");

   // Ensure string length
   check(data.length() >= 32, "Drop data must be at least 32 characters in length.");

   // Iterate over all drops to be created and insert them into the drops table
   for (int i = 0; i < amount; i++) {
      const uint64_t seed = hash_data(to_string(i) + data);

      // Ensure first drop does not already exist
      // NOTE: subsequent drops are not checked for performance reasons
      if (i == 0) {
         check(drops.find(seed) == drops.end(), "Drop " + to_string(seed) + " already exists.");
      }

      // Determine the payer with bound = owner, unbound = contract
      const name ram_payer = bound ? owner : get_self();
      drops.emplace(ram_payer, [&](auto& row) {
         row.seed    = seed;
         row.owner   = owner;
         row.bound   = bound;
         row.created = current_block_time();
      });
   }

   // generating unbond drops consumes contract RAM bytes to owner
   if ( bound == false ) {
      const int64_t bytes = amount * get_bytes_per_drop();
      reduce_ram_bytes(owner, bytes);
      return bytes;
   }
   // bound drops do not consume contract RAM bytes
   return 0;
}

uint64_t drops::hash_data(const string data)
{
   auto     hash       = sha256(data.c_str(), data.length());
   auto     byte_array = hash.extract_as_byte_array();
   uint64_t seed;
   memcpy(&seed, &byte_array, sizeof(uint64_t));
   return seed;
}

// @user
[[eosio::action]] void
drops::transfer(const name from, const name to, const vector<uint64_t> drops_ids, const string memo)
{
   require_auth(from);
   check_is_enabled(get_self());

   check(is_account(to), "Account does not exist.");
   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   require_recipient(from);
   require_recipient(to);

   // Iterate over all drops selected to be transferred
   for (const uint64_t drop_id : drops_ids) {
      modify_owner(drop_id, from, to);
   }
}

void drops::modify_owner(const uint64_t drop_id, const name current_owner, const name new_owner)
{
   drops::drop_table drops(get_self(), get_self().value);

   // additional checks
   auto& drop = drops.get(drop_id, ERROR_DROP_NOT_FOUND.c_str());
   check_drop_owner(drop, current_owner);
   check_drop_bound(drop, false);

   // Modify owner
   drops.modify(drop, same_payer, [&](auto& row) {
      check(row.owner != new_owner, "Drop owner was not modified");
      row.owner = new_owner;
   });
}

void drops::modify_ram_payer(const uint64_t drop_id, const name owner, const bool bound)
{
   drops::drop_table drops(get_self(), get_self().value);

   auto& drop = drops.get(drop_id, ERROR_DROP_NOT_FOUND.c_str());

   // Determine the payer with bound = owner, unbound = contract
   const name ram_payer = bound ? owner : get_self();
   check_drop_owner(drop, owner);
   check_drop_bound(drop, !bound);

   // Modify RAM payer
   drops.modify(drop, ram_payer, [&](auto& row) {
      // Ensure the bound value is being modified
      check(row.bound != bound, "Drop bound was not modified");
      row.bound = bound;
   });
}

// @user
[[eosio::action]] int64_t drops::bind(const name owner, const vector<uint64_t> drops_ids)
{
   require_auth(owner);
   check_is_enabled(get_self());
   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   // binding drops releases RAM to the owner
   const int64_t bytes = drops_ids.size() * get_bytes_per_drop();
   add_ram_bytes(owner, bytes);

   // Modify the RAM payer for the selected drops
   for (const uint64_t drop_id : drops_ids) {
      modify_ram_payer(drop_id, owner, true);
   }
   return bytes;
}

// @user
[[eosio::action]] int64_t drops::unbind(const name owner, const vector<uint64_t> drops_ids)
{
   require_auth(owner);
   check_is_enabled(get_self());
   check(drops_ids.size() > 0, "Drops is empty.");

   // unbinding drops requires the owner to pay for the RAM
   const int64_t bytes = drops_ids.size() * get_bytes_per_drop();
   reduce_ram_bytes(owner, bytes);

   // Modify RAM payer for the selected drops
   for (const uint64_t drop_id : drops_ids) {
      modify_ram_payer(drop_id, owner, false);
   }
   return bytes;
}

void drops::check_drop_bound(const drop_row drop, const bool bound)
{
   check(drop.bound == bound, "Drop " + to_string(drop.seed) + " is not " + (bound ? "bound" : "unbound"));
}

void drops::check_drop_owner(const drop_row drop, const name owner)
{
   check(drop.owner == owner, "Drop " + to_string(drop.seed) + " does not belong to account.");
}

// @user
[[eosio::action]] drops::destroy_return_value
drops::destroy(const name owner, const vector<uint64_t> drops_ids, const string memo)
{
   require_auth(owner);

   check_is_enabled(get_self());
   check(drops_ids.size() > 0, "No drops were provided to destroy.");

   // The number of bound drops that were destroyed
   uint64_t unbound_destroyed = 0;
   for (const uint64_t drop_id : drops_ids) {
      // Count the number of "bound=false" drops destroyed
      const bool bound = destroy_drop(drop_id, owner);
      if (bound == false) {
         unbound_destroyed++;
      }
   }

   // Calculate how much of their own RAM the account reclaimed
   const int64_t bytes_reclaimed = unbound_destroyed * get_bytes_per_drop();
   if (bytes_reclaimed > 0) {
      add_ram_bytes(owner, bytes_reclaimed);
   }
   return {unbound_destroyed, bytes_reclaimed};
}

bool drops::destroy_drop(const uint64_t drop_id, const name owner)
{
   drops::drop_table drops(get_self(), get_self().value);

   auto& drop = drops.get(drop_id, ERROR_DROP_NOT_FOUND.c_str());
   check_drop_owner(drop, owner);
   const bool bound = drop.bound;

   // Destroy the drops
   drops.erase(drop);

   // return if the drop was bound or not
   return bound;
}

// @user
[[eosio::action]] bool drops::open(const name owner)
{
   require_auth(owner);

   drops::balances_table _balances(get_self(), get_self().value);

   auto balance = _balances.find(owner.value);
   if (balance == _balances.end()) {
      _balances.emplace(owner, [&](auto& row) {
         row.owner     = owner;
         row.drops     = 0;
         row.ram_bytes = 0;
      });
      return true;
   }
   // else: account already has an open balance
   // do not revert transaction for UI/UX
   return false;
}

// @user
[[eosio::action]] int64_t drops::claim(const name owner)
{
   require_auth(owner);

   drops::balances_table _balances(get_self(), get_self().value);

   const int64_t ram_bytes = _balances.get(owner.value, ERROR_OPEN_BALANCE.c_str()).ram_bytes;
   if (ram_bytes > 0) {
      reduce_ram_bytes(owner, ram_bytes);
      transfer_ram(owner, ram_bytes, MEMO_RAM_TRANSFER);
      return ram_bytes;
   }
   // else: account does not have any RAM bytes to claim
   // do not revert transaction for UI/UX
   return 0;
}

void drops::reduce_ram_bytes(const name owner, const int64_t bytes)
{
   drops::balances_table _balances(get_self(), get_self().value);

   auto& balance = _balances.get(owner.value, ERROR_OPEN_BALANCE.c_str());
   _balances.modify(balance, same_payer, [&](auto& row) {
      row.ram_bytes -= bytes;
      check(row.ram_bytes >= 0, "Account does not have enough RAM bytes.");
   });
}
void drops::add_ram_bytes(const name owner, const int64_t bytes)
{
   drops::balances_table _balances(get_self(), get_self().value);

   auto& balance = _balances.get(owner.value, ERROR_OPEN_BALANCE.c_str());
   _balances.modify(balance, same_payer, [&](auto& row) { row.ram_bytes += bytes; });
}

// @admin
[[eosio::action]] void drops::enable(const bool enabled)
{
   require_auth(get_self());

   drops::state_table _state(get_self(), get_self().value);

   auto state    = _state.get_or_default();
   state.enabled = enabled;
   _state.set(state, get_self());
}

int64_t drops::get_bytes_per_drop()
{
   drops::state_table _state(get_self(), get_self().value);
   return _state.get_or_default().bytes_per_drop;
}

} // namespace dropssystem
