#include "drops/drops.hpp"

#include "ram.cpp"

// DEBUG (used to help testing)
#ifdef DEBUG
#include "debug.cpp"
#endif

namespace dropssystem {

[[eosio::on_notify("*::transfer")]] drops::generate_return_value
drops::on_transfer(name from, name to, asset quantity, std::string memo)
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
   std::vector<std::string> parsed = split(memo, ',');
   if (parsed[0] == "unbind") {
      check(parsed.size() == 1, "Memo data must only contain 1 value of 'unbind'.");
      return do_unbind(from, to, quantity, parsed);
   } else {
      check(parsed.size() == 2, "Memo data must contain 2 values, seperated by a "
                                "comma using format: <drops_amount>,<drops_data>");
      return do_generate(from, to, quantity, parsed);
   }
}

drops::generate_return_value drops::do_generate(name from, name to, asset quantity, std::vector<std::string> parsed)
{
   check_is_enabled();

   // Ensure amount is a positive value
   uint64_t amount = stoi(parsed[0]);
   check(amount > 0, "The amount of drops to generate must be a positive value.");

   // Ensure string length
   string data = parsed[1];
   check(data.length() >= 32, "Drop generation seed data must be at least 32 characters in length.");

   // Calculate amount of RAM needing to be purchased
   // NOTE: Additional RAM is being purchased to account for the buyrambytes bug
   // SEE: https://github.com/EOSIO/eosio.system/issues/30
   int64_t ram_purchase_amount = amount * get_bytes_per_drop();

   // Purchase the RAM for this transaction using the tokens from the transfer
   buy_ram_bytes(ram_purchase_amount);

   // Iterate over all drops to be created and insert them into the drop table
   drop_table drops(_self, _self.value);
   for (int i = 0; i < amount; i++) {
      string   value      = std::to_string(i) + data;
      auto     hash       = sha256(value.c_str(), value.length());
      auto     byte_array = hash.extract_as_byte_array();
      uint64_t seed;
      memcpy(&seed, &byte_array, sizeof(uint64_t));
      drops.emplace(_self, [&](auto& row) {
         row.seed    = seed;
         row.owner   = from;
         row.bound   = false;
         row.created = current_block_time();
      });
   }

   // Calculate the purchase cost via bancor after the purchase to ensure the
   // incoming transfer can cover it
   asset ram_purchase_cost = eosiosystem::ramcostwithfee(ram_purchase_amount, EOS);
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

drops::generate_return_value drops::do_unbind(name from, name to, asset quantity, std::vector<std::string> parsed)
{
   check_is_enabled();

   // Find the unbind request of the owner
   unbind_table unbinds(_self, _self.value);
   auto         unbinds_itr = unbinds.find(from.value);
   check(unbinds_itr != unbinds.end(), "No unbind request found for account.");

   // Calculate amount of RAM needing to be purchased
   // NOTE: Additional RAM is being purchased to account for the buyrambytes bug
   // SEE: https://github.com/EOSIO/eosio.system/issues/30
   int64_t ram_purchase_amount = unbinds_itr->drops_ids.size() * get_bytes_per_drop();

   // Purchase the RAM for this transaction using the tokens from the transfer
   buy_ram_bytes(ram_purchase_amount);

   // Recreate all selected drops with new bound value (false)
   for (auto it = begin(unbinds_itr->drops_ids); it != end(unbinds_itr->drops_ids); ++it) {
      modify_drop_binding(from, *it, false);
   }

   // Calculate the purchase cost via bancor after the purchase to ensure the
   // incoming transfer can cover it
   asset ram_purchase_cost = eosiosystem::ramcostwithfee(ram_purchase_amount, EOS);
   check(quantity.amount >= ram_purchase_cost.amount,
         "The amount sent does not cover the RAM purchase cost (requires " + ram_purchase_cost.to_string() + ")");

   // Calculate any remaining tokens from the transfer after the RAM purchase
   int64_t remainder = quantity.amount - ram_purchase_cost.amount;

   // Return any remaining tokens to the sender
   if (remainder > 0) {
      transfer_tokens(from, asset{remainder, EOS}, "");
   }

   // Destroy the unbind request now that its complete
   unbinds.erase(unbinds_itr);

   return {
      0,                     // drops bought
      ram_purchase_cost,     // cost
      asset{remainder, EOS}, // refund
      0,                     // total drops
   };
}

[[eosio::action]] drops::generate_return_value drops::mint(name owner, uint32_t amount, std::string data)
{
   require_auth(owner);
   check_is_enabled();

   // Ensure amount is a positive value
   check(amount > 0, "The amount of drops to generate must be a positive value.");

   // Ensure string length
   check(data.length() > 32, "Drop data must be at least 32 characters in length.");

   // Iterate over all drops to be created and insert them into the drops table
   drop_table drops(_self, _self.value);
   for (uint32_t i = 0; i < amount; i++) {
      string   value      = std::to_string(i) + data;
      auto     hash       = sha256(value.c_str(), value.length());
      auto     byte_array = hash.extract_as_byte_array();
      uint64_t seed;
      memcpy(&seed, &byte_array, sizeof(uint64_t));
      drops.emplace(owner, [&](auto& row) {
         row.seed    = seed;
         row.owner   = owner;
         row.bound   = true;
         row.created = current_time_point();
      });
   }

   return {
      (uint32_t)amount, // drops minted
      asset{0, EOS},    // cost
      asset{0, EOS},    // refund
      amount,           // total drops
   };
}

[[eosio::action]] void drops::transfer(name from, name to, std::vector<uint64_t> drops_ids, string memo)
{
   require_auth(from);
   check_is_enabled();

   check(is_account(to), "Account does not exist.");
   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   require_recipient(from);
   require_recipient(to);

   // Iterate over all drops selected to be transferred
   drops::drop_table drops(_self, _self.value);
   for (auto it = begin(drops_ids); it != end(drops_ids); ++it) {
      auto drops_itr = drops.find(*it);
      check(drops_itr != drops.end(), "Drop " + std::to_string(drops_itr->seed) + " not found");
      check(drops_itr->bound == false,
            "Drop " + std::to_string(drops_itr->seed) + " is bound and cannot be transferred");
      check(drops_itr->owner == from, "Account does not own drop" + std::to_string(drops_itr->seed));
      // Perform the transfer
      drops.modify(drops_itr, _self, [&](auto& row) { row.owner = to; });
   }
}

void drops::buy_ram_bytes(int64_t bytes)
{
   eosiosystem::system_contract::buyrambytes_action buyrambytes{"eosio"_n, {_self, "active"_n}};
   buyrambytes.send(_self, _self, bytes);
}

void drops::sell_ram_bytes(int64_t bytes)
{
   eosiosystem::system_contract::sellram_action sellram{"eosio"_n, {_self, "active"_n}};
   sellram.send(_self, bytes);
}

void drops::transfer_tokens(name to, asset amount, string memo)
{
   token::transfer_action transfer_act{"eosio.token"_n, {{_self, "active"_n}}};
   transfer_act.send(_self, to, amount, memo);
}

void drops::transfer_ram(name to, asset amount, string memo) { check(false, "transfer_ram not implemented"); }

drops::drop_row drops::modify_drop_binding(name owner, uint64_t drop_id, bool bound)
{
   drops::drop_table drops(_self, _self.value);

   auto drops_itr = drops.find(drop_id);

   check(drops_itr != drops.end(), "Drop " + std::to_string(drops_itr->seed) + " not found");
   check(drops_itr->owner == owner, "Drop " + std::to_string(drops_itr->seed) + " does not belong to account.");
   check(drops_itr->bound == bound,
         "Drop " + std::to_string(drops_itr->seed) + " bound value is already " + std::to_string(bound) + ".");

   // Determine the payer with bound = owner, unbound = contract
   name ram_payer = bound ? owner : _self;

   // Copy the existing row
   drops::drop_row drop = *drops_itr;

   // Destroy the existing row
   drops.erase(drops_itr);

   // Recreate identical drop with new bound value and payer
   drops.emplace(owner, [&](auto& row) {
      row.seed    = drop.seed;
      row.owner   = drop.owner;
      row.bound   = bound;
      row.created = drop.created;
   });

   return drop;
}

[[eosio::action]] drops::bind_return_value drops::bind(name owner, std::vector<uint64_t> drops_ids)
{
   require_auth(owner);
   check_is_enabled();

   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   drops::drop_table drops(_self, _self.value);
   for (auto it = begin(drops_ids); it != end(drops_ids); ++it) {
      modify_drop_binding(owner, *it, true);
   }

   // Calculate RAM sell amount and reclaim value
   uint64_t ram_sell_amount   = drops_ids.size() * get_bytes_per_drop();
   asset    ram_sell_proceeds = eosiosystem::ramproceedstminusfee(ram_sell_amount, EOS);

   if (ram_sell_amount > 0) {
      // Sell the excess RAM no longer used by the contract
      sell_ram_bytes(ram_sell_amount);

      // Transfer proceeds to the owner
      transfer_tokens(owner, ram_sell_proceeds,
                      "Reclaimed RAM value of " + std::to_string(drops_ids.size()) + " drops(s)");
   }

   return {
      ram_sell_amount,  // ram the contract sold
      ram_sell_proceeds // token value of ram sold
   };
}

[[eosio::action]] void drops::unbind(name owner, std::vector<uint64_t> drops_ids)
{
   require_auth(owner);
   check_is_enabled();

   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   // Save the unbind request and await for token transfer with matching memo data
   unbind_table unbinds(_self, _self.value);
   unbinds.emplace(owner, [&](auto& row) {
      row.owner     = owner;
      row.drops_ids = drops_ids;
   });
}

[[eosio::action]] void drops::cancelunbind(name owner)
{
   require_auth(owner);
   check_is_enabled();

   // Remove the unbind request of the owner
   unbind_table unbinds(_self, _self.value);
   auto         unbinds_itr = unbinds.find(owner.value);
   check(unbinds_itr != unbinds.end(), "No unbind request found for account.");
   unbinds.erase(unbinds_itr);
}

[[eosio::action]] drops::destroy_return_value drops::destroy(name owner, std::vector<uint64_t> drops_ids, string memo)
{
   require_auth(owner);
   check_is_enabled();

   check(drops_ids.size() > 0, "No drops were provided to destroy.");

   drops::drop_table drops(_self, _self.value);

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
   uint64_t ram_sell_amount   = (drops_ids.size() - bound_destroyed) * record_size;
   asset    ram_sell_proceeds = eosiosystem::ramproceedstminusfee(ram_sell_amount, EOS);
   if (ram_sell_amount > 0) {
      sell_ram_bytes(ram_sell_amount);
      transfer_tokens(owner, ram_sell_proceeds,
                      "Reclaimed RAM value of " + std::to_string(drops_ids.size()) + " drops(s)");
   }

   // Calculate how much of their own RAM the account reclaimed
   uint64_t ram_reclaimed = bound_destroyed * record_size;

   return {
      ram_sell_amount,   // ram sold
      ram_sell_proceeds, // redeemed ram value
      ram_reclaimed      // ram released from owner
   };
}

[[eosio::action]] void drops::enable(bool enabled)
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

std::vector<std::string> drops::split(const std::string& str, char delim)
{
   std::vector<std::string> strings;
   size_t                   start;
   size_t                   end = 0;
   while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
      end = str.find(delim, start);
      strings.push_back(str.substr(start, end - start));
   }
   return strings;
}

} // namespace dropssystem
