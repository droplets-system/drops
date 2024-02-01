#include "drops/drops.hpp"

#include "ram.cpp"

// DEBUG (used to help testing)
#ifdef DEBUG
#include "debug.cpp"
#endif

namespace dropssystem {

[[eosio::on_notify("*::transfer")]]
drops::generate_return_value drops::on_transfer(const name from, const name to, const asset quantity, const string memo)
{
   if (from == "eosio.ram"_n ) return {}; // ignore RAM sales
   if (to != get_self()) return {}; // ignore transfers not sent to this contract
   if (from == get_self()) return {}; // ignore transfers sent from this contract

   check_is_enabled();
   check(get_first_receiver() == "eosio.token"_n, "Only the eosio.token contract may send tokens to this contract.");
   check(quantity.amount > 0, "The transaction amount must be a positive value.");
   check(quantity.symbol == EOS, "Only the system token is accepted for transfers.");
   check(!memo.empty(), "A memo is required to send tokens to this contract");

   // Process the memo field to determine the number of drops to generate
   const vector<string> parsed = split(memo, ',');
   if (parsed[0] == "unbind") {
      check(parsed.size() == 1, "Memo data must only contain 1 value of 'unbind'.");
      return do_unbind(from, quantity);
   } else {
      check(parsed.size() == 2, "Memo data must contain 2 values, seperated by a "
                                "comma using format: <drops_amount>,<drops_data>");

      const int64_t amount = to_number(parsed[0]);
      check( amount > 0, "The amount of drops to generate must be a positive value.");
      const string data = parsed[1];
      return do_generate(from, quantity, amount, data);
   }
}

drops::generate_return_value drops::do_generate(const name from, const asset quantity, const uint64_t amount, const string data )
{
   check_is_enabled();

   // Calculate amount of RAM needing to be purchased
   // NOTE: Additional RAM is being purchased to account for the buyrambytes bug
   // SEE: https://github.com/EOSIO/eosio.system/issues/30
   int64_t ram_purchase_amount = amount * get_bytes_per_drop();

   // Purchase the RAM for this transaction using the tokens from the transfer
   buy_ram_bytes(ram_purchase_amount);

   // Iterate over all drops to be created and insert them into the drop table
   emplace_drops(get_self(), from, data, amount);

   // Calculate the purchase cost via bancor after the purchase to ensure the
   // incoming transfer can cover it
   asset ram_purchase_cost = eosiosystem::ram_cost_with_fee(ram_purchase_amount, EOS);
   check(quantity.amount >= ram_purchase_cost.amount,
         "The amount sent does not cover the RAM purchase cost (requires " + ram_purchase_cost.to_string() + ")");

   // Calculate any remaining tokens from the transfer after the RAM purchase
   int64_t remainder = quantity.amount - ram_purchase_cost.amount;

   // Return any remaining tokens to the sender
   if (remainder > 0) {
      transfer_tokens(from, asset{remainder, EOS}, "");
   }

   return {
      (uint32_t)amount,      // drops bought
      ram_purchase_cost,     // cost
      asset{remainder, EOS}, // refund
      amount,                // total drops
   };
}

drops::generate_return_value drops::do_unbind(const name from, const asset quantity )
{
   check_is_enabled();

   // Find the unbind request of the owner
   unbind_table unbinds(get_self(), get_self().value);
   auto & unbind = unbinds.get(from.value, "No unbind request found for account.");

   // Calculate amount of RAM needing to be purchased
   // NOTE: Additional RAM is being purchased to account for the buyrambytes bug
   // SEE: https://github.com/EOSIO/eosio.system/issues/30
   const vector<uint64_t> drops_ids = unbind.drops_ids;
   const int64_t ram_purchase_amount = drops_ids.size() * get_bytes_per_drop();

   // Purchase the RAM for this transaction using the tokens from the transfer
   buy_ram_bytes(ram_purchase_amount);

   // Recreate all selected drops with new bound value (false)
   for (const uint64_t drop_id : drops_ids) {
      check_drop_ownership(drop_id, from);
      check_drop_bound(drop_id, true);
      modify_ram_payer(drop_id, get_self());
   }

   // Calculate the purchase cost via bancor after the purchase to ensure the
   // incoming transfer can cover it
   const asset ram_purchase_cost = eosiosystem::ram_cost_with_fee(ram_purchase_amount, EOS);
   check(quantity.amount >= ram_purchase_cost.amount, "The amount sent does not cover the RAM purchase cost (requires " + ram_purchase_cost.to_string() + ")");

   // Calculate any remaining tokens from the transfer after the RAM purchase
   const int64_t remainder = quantity.amount - ram_purchase_cost.amount;

   // Return any remaining tokens to the sender
   if (remainder > 0) {
      transfer_tokens(from, asset{remainder, EOS}, "");
   }

   // Destroy the unbind request now that its complete
   unbinds.erase(unbind);

   return {
      0,                     // drops bought
      ram_purchase_cost,     // cost
      asset{remainder, EOS}, // refund
      0,                     // total drops
   };
}

[[eosio::action]]
drops::generate_return_value drops::mint(const name owner, const uint32_t amount, const string data)
{
   require_auth(owner);
   check_is_enabled();
   emplace_drops(owner, owner, data, amount);

   return {
      (uint32_t)amount, // drops minted
      asset{0, EOS},    // cost
      asset{0, EOS},    // refund
      amount,           // total drops
   };
}

void drops::emplace_drops( const name ram_payer, const name owner, const string data, const uint64_t amount )
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
      if ( i == 0) {
         check(drops.find(seed) == drops.end(), "Drop " + to_string(seed) + " already exists.");
      }

      drops.emplace(ram_payer, [&](auto& row) {
         row.seed    = seed;
         row.owner   = owner;
         row.bound   = ram_payer == owner;
         row.created = current_block_time();
      });
   }
}

uint64_t drops::hash_data( const string data )
{
   auto hash = sha256(data.c_str(), data.length());
   auto byte_array = hash.extract_as_byte_array();
   uint64_t seed;
   memcpy(&seed, &byte_array, sizeof(uint64_t));
   return seed;
}

[[eosio::action]]
void drops::transfer(const name from, const name to, const vector<uint64_t> drops_ids, const string memo)
{
   require_auth(from);
   check_is_enabled();

   check(is_account(to), "Account does not exist.");
   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   require_recipient(from);
   require_recipient(to);

   // Iterate over all drops selected to be transferred
   drops::drop_table drops(get_self(), get_self().value);
   for ( const uint64_t drop_id : drops_ids ) {
      auto drops_itr = drops.find(drop_id);
      check(drops_itr != drops.end(), "Drop " + to_string(drops_itr->seed) + " not found");
      check(drops_itr->bound == false, "Drop " + to_string(drops_itr->seed) + " is bound and cannot be transferred");
      check(drops_itr->owner == from, "Account does not own drop" + to_string(drops_itr->seed));
      // Perform the transfer
      drops.modify(drops_itr, _self, [&](auto& row) { row.owner = to; });
   }
}

void drops::buy_ram_bytes(const int64_t bytes)
{
   eosiosystem::system_contract::buyrambytes_action buyrambytes{"eosio"_n, {_self, "active"_n}};
   buyrambytes.send(get_self(), _self, bytes);
}

void drops::sell_ram_bytes(const int64_t bytes)
{
   eosiosystem::system_contract::sellram_action sellram{"eosio"_n, {get_self(), "active"_n}};
   sellram.send(get_self(), bytes);
}

void drops::transfer_tokens(const name to, const asset quantity, const string memo)
{
   token::transfer_action transfer_act{"eosio.token"_n, {{get_self(), "active"_n}}};
   transfer_act.send(get_self(), to, quantity, memo);
}

void drops::transfer_ram(const name to, const int64_t bytes, const string memo) {
   check(false, "transfer_ram not implemented");
}

void drops::modify_ram_payer( const uint64_t drop_id, const name ram_payer )
{
   drops::drop_table drops(get_self(), get_self().value);
   auto & drop = drops.get(drop_id, "Drop not found");

   // Modify RAM payer
   drops.modify(drop, ram_payer, [&](auto& row) {
      // Determine the payer with bound = owner, unbound = contract
      const bool bound = ram_payer == row.owner;

      // Ensure the bound value is being modified
      check(row.bound != bound, "Drop bound was not modified");
      row.bound = bound;
   });
}

[[eosio::action]]
drops::bind_return_value drops::bind(const name owner, const vector<uint64_t> drops_ids)
{
   require_auth(owner);
   check_is_enabled();

   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   for ( const uint64_t drop_id : drops_ids ) {
      check_drop_ownership(drop_id, owner);
      check_drop_bound(drop_id, false);
      modify_ram_payer(drop_id, owner);
   }

   // Calculate RAM sell amount and reclaim value
   uint64_t ram_sell_amount   = drops_ids.size() * get_bytes_per_drop();
   asset    ram_sell_proceeds = eosiosystem::ram_proceeds_minus_fee(ram_sell_amount, EOS);

   if (ram_sell_amount > 0) {
      // Sell the excess RAM no longer used by the contract
      sell_ram_bytes(ram_sell_amount);

      // Transfer proceeds to the owner
      transfer_tokens(owner, ram_sell_proceeds, "Reclaimed RAM value of " + to_string(drops_ids.size()) + " drops(s)");
   }

   return {
      ram_sell_amount,  // ram the contract sold
      ram_sell_proceeds // token value of ram sold
   };
}

[[eosio::action]]
void drops::unbind(const name owner, const vector<uint64_t> drops_ids)
{
   require_auth(owner);
   check_is_enabled();

   unbind_table unbinds(get_self(), get_self().value);

   check(drops_ids.size() > 0, "Drops is empty.");

   // check if valid drops to unbind
   for ( const uint64_t drop_id : drops_ids ) {
      check_drop_ownership(drop_id, owner);
      check_drop_bound(drop_id, true);
   }

   // Save the unbind request and await for token transfer with matching memo data
   unbinds.emplace(owner, [&](auto& row) {
      row.owner     = owner;
      row.drops_ids = drops_ids;
   });
}

void drops::check_drop_bound( const uint64_t drop_id, const bool bound )
{
   drop_table drops(get_self(), get_self().value);

   auto drop = drops.get(drop_id, "Drop not found");
   check(drop.bound == bound, "Drop " + to_string(drop_id) + " is not " + (bound ? "bound" : "unbound") );
}

void drops::check_drop_ownership( const uint64_t drop_id, const name owner )
{
   drop_table drops(get_self(), get_self().value);

   auto drop = drops.get(drop_id, "Drop not found.");
   check(drop.owner == owner, "Drop " + to_string(drop_id) + " does not belong to account.");
}

[[eosio::action]]
void drops::cancelunbind(const name owner)
{
   require_auth(owner);
   check_is_enabled();

   unbind_table unbinds(get_self(), get_self().value);

   // Remove the unbind request of the owner
   auto & unbind = unbinds.get(owner.value, "No unbind request found for account.");
   unbinds.erase(unbind);
}

[[eosio::action]]
drops::destroy_return_value drops::destroy(const name owner, const vector<uint64_t> drops_ids, const string memo)
{
   require_auth(owner);
   check_is_enabled();

   check(drops_ids.size() > 0, "No drops were provided to destroy.");

   drops::drop_table drops(get_self(), get_self().value);

   // The number of bound drops that were destroyed
   int bound_destroyed = 0;

   // Loop to destroy specified drops
   for ( const uint64_t drop_id : drops_ids ) {
      auto drops_itr = drops.find(drop_id);
      check(drops_itr != drops.end(), "Drop " + to_string(drop_id) + " not found");
      check(drops_itr->owner == owner, "Drop " + to_string(drop_id) + " does not belong to account.");
      // Destroy the drops
      drops.erase(drops_itr);
      // Count the number of bound drops destroyed
      // This will be subtracted from the amount paid out
      if (drops_itr->bound) {
         bound_destroyed++;
      }
   }

   // Calculate RAM sell amount and proceeds
   const int64_t record_size = get_bytes_per_drop();
   const uint64_t ram_sell_amount = (drops_ids.size() - bound_destroyed) * record_size;
   const asset ram_sell_proceeds = eosiosystem::ram_proceeds_minus_fee(ram_sell_amount, EOS);

   if (ram_sell_amount > 0) {
      sell_ram_bytes(ram_sell_amount);
      transfer_tokens(owner, ram_sell_proceeds, "Reclaimed RAM value of " + to_string(drops_ids.size()) + " drops(s)");
   }

   // Calculate how much of their own RAM the account reclaimed
   uint64_t ram_reclaimed = bound_destroyed * record_size;

   return {
      ram_sell_amount,   // ram sold
      ram_sell_proceeds, // redeemed ram value
      ram_reclaimed      // ram released from owner
   };
}

[[eosio::action]]
void drops::enable(const bool enabled)
{
   require_auth(get_self());

   drops::state_table _state(get_self(), get_self().value);
   auto state = _state.get_or_default();
   state.enabled = enabled;
   _state.set(state, get_self());
}

void drops::check_is_enabled()
{
   drops::state_table _state(get_self(), get_self().value);
   auto state = _state.get_or_default();
   check(state.enabled, "Drops are currently disabled.");
}

int64_t drops::get_bytes_per_drop()
{
   drops::state_table _state(get_self(), get_self().value);
   auto state = _state.get_or_default();
   return state.bytes_per_drop;
}

vector<string> drops::split(const string& str, const char delim)
{
   vector<string>       strings;
   size_t               start;
   size_t               end = 0;
   while ((start = str.find_first_not_of(delim, end)) != string::npos) {
      end = str.find(delim, start);
      strings.push_back(str.substr(start, end - start));
   }
   return strings;
}

int64_t drops::to_number(const string& str) {
    if (str.empty()) return 0;

    char* end;
    const uint64_t num = std::strtoull(str.c_str(), &end, 10);

    // Check if conversion was successful
    check(*end == '\0', "invalid number format or overflow");

    // Check for underflow
    check(num <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()), "number underflow");

    return static_cast<int64_t>(num);
}

} // namespace dropssystem
