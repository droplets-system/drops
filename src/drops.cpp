#include "drops/drops.hpp"

namespace dropssystem {

[[eosio::on_notify("eosio.token::transfer")]] drops::generate_return_value
drops::generate(name from, name to, asset quantity, std::string memo)
{
   if (from == "eosio.ram"_n || to != _self || from == _self || memo == "bypass") {
      return {(uint32_t)0, asset{0, EOS}, asset{0, EOS}, (uint64_t)0};
   }

   require_auth(from);
   check(to == _self, "Tokens must be sent to this contract.");
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
   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

   // Ensure amount is a positive value
   uint64_t amount = stoi(parsed[0]);
   check(amount > 0, "The amount of drops to generate must be a positive value.");

   // Ensure string length
   string data = parsed[1];
   check(data.length() > 32, "Drop generation seed data must be at least 32 characters in length.");

   // Calculate amount of RAM needing to be purchased
   // NOTE: Additional RAM is being purchased to account for the buyrambytes bug
   // SEE: https://github.com/EOSIO/eosio.system/issues/30
   uint64_t ram_purchase_amount = amount * (record_size + purchase_buffer);

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
         row.created = current_time_point();
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
      transfer_tokens(from, asset{remainder, EOS}, "")
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
   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

   // Find the unbind request of the owner
   unbind_table unbinds(_self, _self.value);
   auto         unbinds_itr = unbinds.find(from.value);
   check(unbinds_itr != unbinds.end(), "No unbind request found for account.");

   // Calculate amount of RAM needing to be purchased
   // NOTE: Additional RAM is being purchased to account for the buyrambytes bug
   // SEE: https://github.com/EOSIO/eosio.system/issues/30
   uint64_t ram_purchase_amount = unbinds_itr->drops_ids.size() * (record_size + purchase_buffer);

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

   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

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

[[eosio::action]] drops::generate_return_value drops::generatertrn() {}

[[eosio::action]] void drops::transfer(name from, name to, std::vector<uint64_t> drops_ids, string memo)
{
   require_auth(from);

   check(is_account(to), "Account does not exist.");
   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   require_recipient(from);
   require_recipient(to);

   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

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
}

[[eosio::action]] drops::bind_return_value drops::bind(name owner, std::vector<uint64_t> drops_ids)
{
   require_auth(owner);

   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

   // Recreate all selected drops with new bound value (bound = true)
   drops::drop_table drops(_self, _self.value);
   for (auto it = begin(drops_ids); it != end(drops_ids); ++it) {
      modify_drop_binding(owner, *it, true);
   }

   // Calculate RAM sell amount and reclaim value
   uint64_t ram_sell_amount   = drops_ids.size() * record_size;
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

   check(drops_ids.size() > 0, "No drops were provided to transfer.");

   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

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

   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

   // Remove the unbind request of the owner
   unbind_table unbinds(_self, _self.value);
   auto         unbinds_itr = unbinds.find(owner.value);
   check(unbinds_itr != unbinds.end(), "No unbind request found for account.");
   unbinds.erase(unbinds_itr);
}

[[eosio::action]] drops::destroy_return_value drops::destroy(name owner, std::vector<uint64_t> drops_ids, string memo)
{
   require_auth(owner);

   // Retrieve contract state
   state_table state(_self, _self.value);
   auto        state_itr = state.find(1);
   check(state_itr->enabled, "Contract is currently disabled.");

   check(drops_ids.size() > 0, "No drops were provided to destroy.");

   drops::drop_table drops(_self, _self.value);

   // The number of bound drops that were destroyed
   int bound_destroyed = 0;

   // Loop to destroy specified drops
   for (auto it = begin(drops_ids); it != end(drops_ids); ++it) {
      auto drops_itr = drops.find(*it);
      check(drops_itr != drops.end(), "Drop not found");
      check(drops_itr->owner == owner, "Account does not own this drops");
      // Destroy the drops
      drops.erase(drops_itr);
      // Count the number of bound drops destroyed
      // This will be subtracted from the amount paid out
      if (drops_itr->bound) {
         bound_destroyed++;
      }
   }

   // Calculate RAM sell amount and proceeds
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

/**
    TESTNET ACTIONS
*/
[[eosio::action]] void drops::destroyall()
{
   require_auth(_self);

   uint64_t            drops_destroyed = 0;
   map<name, uint64_t> drops_destroyed_for;

   drops::drop_table drops(_self, _self.value);
   auto              drops_itr = drops.begin();
   while (drops_itr != drops.end()) {
      drops_destroyed += 1;
      // Keep track of how many drops were destroyed per owner for debug refund
      if (drops_destroyed_for.find(drops_itr->owner) == drops_destroyed_for.end()) {
         drops_destroyed_for[drops_itr->owner] = 1;
      } else {
         drops_destroyed_for[drops_itr->owner] += 1;
      }
      drops_itr = drops.erase(drops_itr);
   }

   // Calculate RAM sell amount
   uint64_t ram_to_sell = drops_destroyed * record_size;
   sell_ram_bytes(ram_to_sell);

   for (auto& iter : drops_destroyed_for) {
      uint64_t ram_sell_amount   = iter.second * record_size;
      asset    ram_sell_proceeds = eosiosystem::ramproceedstminusfee(ram_sell_amount, EOS);

      transfer_tokens(iter.first, ram_sell_proceeds,
                      "Testnet Reset - Reclaimed RAM value of " + std::to_string(iter.second) + " drops(s)");
   }
}

[[eosio::action]] void drops::enable(bool enabled)
{
   require_auth(_self);

   drops::state_table state(_self, _self.value);
   auto               state_itr = state.find(1);
   state.modify(state_itr, _self, [&](auto& row) { row.enabled = enabled; });
}

[[eosio::action]] void drops::init()
{
   require_auth(_self);

   drop_table  drops(_self, _self.value);
   state_table state(_self, _self.value);

   // Give system contract the 0 drops
   drops.emplace(_self, [&](auto& row) {
      row.seed    = 0;
      row.owner   = "eosio"_n;
      row.bound   = true;
      row.created = current_block_time();
   });

   // Give Greymass the "Greymass" drops
   drops.emplace(_self, [&](auto& row) {
      row.seed    = 7338027470446133248;
      row.owner   = "teamgreymass"_n;
      row.bound   = true;
      row.created = current_block_time();
   });

   // Set the current state to epoch 1
   state.emplace(_self, [&](auto& row) {
      row.id      = 1;
      row.enabled = false;
   });
}

[[eosio::action]] void drops::wipe()
{
   require_auth(_self);

   drops::drop_table drops(_self, _self.value);
   auto              drops_itr = drops.begin();
   while (drops_itr != drops.end()) {
      drops_itr = drops.erase(drops_itr);
   }

   drops::state_table state(_self, _self.value);
   auto               state_itr = state.begin();
   while (state_itr != state.end()) {
      state_itr = state.erase(state_itr);
   }
}

[[eosio::action]] void drops::wipesome()
{
   require_auth(_self);
   drops::drop_table drops(_self, _self.value);
   auto              drops_itr = drops.begin();

   int i   = 0;
   int max = 10000;
   while (drops_itr != drops.end()) {
      if (i++ > max) {
         break;
      }
      i++;
      drops_itr = drops.erase(drops_itr);
   }
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
