#include "drops/drops.hpp"

// // logging (used for backend syncing)
// #include "logs.cpp"

#include "ram.cpp"

// DEBUG (used to help testing)
#ifdef DEBUG
#include "debug.cpp"
#endif

[[eosio::on_notify("eosio.token::transfer")]]
void drops::on_transfer(name from, name to, asset quantity, std::string memo)
{
   if (from == "eosio.ram"_n ) return;
   if (to != get_self()) return;

   // Process the memo field to determine the number of drops to generate
   const vector<string> parts = split(memo, ',');
   check(parts.size() == 2, "invalid memo (ex: \"<drops_amount>,<data>)\"");
   const uint32_t amount = stoi(parts[0]);
   const string data = parts[1];
   return do_generate(from, amount, quantity, data);
}

[[eosio::on_notify("eosio::ramtransfer")]]
void drops::on_ram_transfer(const name from, const name to, const int64_t bytes)
{
   check(false, "not yet implemented");
}

uint64_t drops::hash_data( const string data )
{
   auto hash = sha256(data.c_str(), data.length());
   auto byte_array = hash.extract_as_byte_array();
   uint64_t seed;
   memcpy(&seed, &byte_array, sizeof(uint64_t));
   return seed;
}

void drops::do_generate( const name owner, const uint32_t amount, const asset quantity, const string data )
{
   check(amount > 0, "The amount of drops to generate must be a positive value.");
   check(data.length() >= 32, "Drop generation seed data must be at least 32 characters in length.");

   drop_table drops(get_self(), get_self().value);

   // Calculate amount of RAM needing to be purchased
   const int64_t ram_purchase_amount = amount * get_bytes_per_drop();

   // Purchase the RAM for this transaction using the tokens from the transfer
   buy_ram_bytes(ram_purchase_amount);

   // Iterate over all drops to be created and insert them into the drop table
   for (int i = 0; i < amount; i++) {
      drops.emplace(_self, [&](auto& row) {
         row.seed    = hash_data(to_string(i) + data);
         row.owner   = owner;
         row.created = current_block_time();
      });
   }

   // Calculate the purchase cost via bancor after the purchase to ensure the
   // incoming transfer can cover it
   const asset ram_purchase_cost = eosiosystem::ram_cost_with_fee(ram_purchase_amount, EOS);
   check(quantity.amount >= ram_purchase_cost.amount, "The amount sent does not cover the RAM purchase cost (requires " + ram_purchase_cost.to_string() + ")");

   // Calculate any remaining tokens from the transfer after the RAM purchase
   const asset remainder = quantity - ram_purchase_cost;

   // Return any remaining tokens to the sender
   if (remainder.amount > 0) {
      transfer_to(owner, remainder, "Remaining tokens returned to sender.");
   }
}

[[eosio::action]]
void drops::transfer(const name from, const name to, std::vector<uint64_t> drops_ids, string memo)
{
   require_auth(from);

   drops::drop_table drops(get_self(), get_self().value);

   check_is_paused();
   check(is_account(to), "Account does not exist.");
   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   require_recipient(from);
   require_recipient(to);

   // Change ownership of drops from sender to receiver
   for (const uint64_t drop_id : drops_ids ) {
      auto drops_itr = drops.require_find(drop_id);
      check(drops_itr != drops.end(), "drop_id=" + to_string(drop_id) + " not found");
      check(drops_itr->owner == from, "drop_id=" + to_string(drop_id) + " must belong to sender");
      drops.modify(drops_itr, get_self(), [&](auto& row) {
         row.owner = to;
      });
   }
}

[[eosio::action]]
void drops::destroy( const name owner, const vector<uint64_t> drops_ids, const bool sell_ram, const string memo )
{
   require_auth(owner);
   drops::drop_table drops(get_self(), get_self().value);

   check_is_paused();
   check(drops_ids.size() > 0, "drop_ids is empty");
   check( sell_ram == true, "not yet implemented");

   // release RAM from destroyed drops
   for ( const uint64_t drop_id : drops_ids ) {
      auto drops_itr = drops.find(drop_id);
      check(drops_itr != drops.end(), "drop_id=" + to_string(drop_id) + " not found");
      check(drops_itr->owner == owner, "drop_id=" + to_string(drop_id) + " must belong to owner");
      drops.erase(drops_itr);
   }

   // Calculate RAM sell amount and proceeds
   const int64_t ram_sell_amount = drops_ids.size() * get_bytes_per_drop();
   const asset ram_sell_proceeds = eosiosystem::ram_proceeds_minus_fee(ram_sell_amount, EOS);

   // Return RAM sell proceeds to owner
   eosiosystem::system_contract::sellram_action sellram{"eosio"_n, {get_self(), "active"_n}};
   eosio::token::transfer_action transfer_act{"eosio.token"_n, {get_self(), "active"_n}};

   const string transfer_memo = "Reclaimed RAM value of " + std::to_string(drops_ids.size()) + " drops(s)";
   sellram.send(get_self(), ram_sell_amount );
   transfer_act.send(get_self(), owner, ram_sell_proceeds, transfer_memo);
}

[[eosio::action]]
void drops::pause(bool paused)
{
   require_auth(get_self());

   drops::state_table _state(get_self(), get_self().value);
   auto state = _state.get_or_default();
   state.paused = paused;
   _state.set(state, get_self());
}

int64_t drops::get_bytes_per_drop()
{
   drops::state_table _state(get_self(), get_self().value);
   auto state = _state.get_or_default();
   return state.bytes_per_drop;
}

void drops::check_is_paused()
{
   drops::state_table _state(get_self(), get_self().value);
   auto state = _state.get_or_default();
   check(!state.paused, "Contract is paused.");
}

void drops::buy_ram_bytes(const int64_t bytes )
{
   eosiosystem::system_contract::buyrambytes_action buyrambytes{"eosio"_n, {get_self(), "active"_n}};
   buyrambytes.send(get_self(), get_self(), bytes);
}

void drops::transfer_to(const name to, const asset quantity, const string memo )
{
   eosio::token::transfer_action transfer_act{"eosio.token"_n, {get_self(), "active"_n}};
   transfer_act.send(get_self(), to, quantity, memo);
}

vector<string> drops::split(const string& str, const char delim)
{
   vector<string> strings;
   size_t start;
   size_t end = 0;

   while ((start = str.find_first_not_of(delim, end)) != string::npos) {
      end = str.find(delim, start);
      strings.push_back(str.substr(start, end - start));
   }
   return strings;
}
