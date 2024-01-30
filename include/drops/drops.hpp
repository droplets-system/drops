#pragma once

#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

#include <drops/drops.hpp>
#include <drops/ram.hpp>

using namespace eosio;
using namespace std;

namespace dropssystem {

static constexpr name drops_contract  = "seed.gm"_n;   // location of drops contract
static constexpr name oracle_contract = "oracle.gm"_n; // location of oracle contract

// drops table row bytes costs
static constexpr uint64_t primary_row     = 145;                           // size to create a row
static constexpr uint64_t secondary_index = 144;                           // size of secondary index
static constexpr uint64_t record_size     = primary_row + secondary_index; // total record size

// account table row bytes cost
static constexpr uint64_t accounts_row = 124;

// stat table row bytes cost
static constexpr uint64_t stats_row = 412;

// Additional RAM bytes to purchase (buyrambytes bug)
static constexpr uint64_t purchase_buffer = 1;

static constexpr symbol EOS = symbol{"EOS", 4};

uint128_t combine_ids(const uint64_t& v1, const uint64_t& v2) { return (uint128_t{v1} << 64) | v2; }

class [[eosio::contract("drops")]] drops : public contract
{
public:
   using contract::contract;

   struct [[eosio::table("drop")]] drop_row
   {
      uint64_t          seed;
      name              owner;
      eosio::time_point created;
      bool              bound;
      uint64_t          primary_key() const { return seed; }
      uint128_t         by_owner() const { return ((uint128_t)owner.value << 64) | seed; }
   };

   struct [[eosio::table("state")]] state_row
   {
      uint16_t id;
      bool     enabled;
      uint64_t primary_key() const { return id; }
   };

   struct [[eosio::table("unbind")]] unbind_row
   {
      name                  owner;
      std::vector<uint64_t> drops_ids;
      uint64_t              primary_key() const { return owner.value; }
   };

   typedef eosio::multi_index<
      "drop"_n,
      drop_row,
      eosio::indexed_by<"owner"_n, eosio::const_mem_fun<drop_row, uint128_t, &drop_row::by_owner>>>
                                                      drop_table;
   typedef eosio::multi_index<"state"_n, state_row>   state_table;
   typedef eosio::multi_index<"unbind"_n, unbind_row> unbind_table;

   /*

    Return value structs

   */

   struct generate_return_value
   {
      uint32_t drops;
      asset    cost;
      asset    refund;
      uint64_t total_drops;
   };

   struct destroy_return_value
   {
      uint64_t ram_sold;
      asset    redeemed;
      uint64_t ram_reclaimed;
   };

   struct bind_return_value
   {
      uint64_t ram_sold;
      asset    redeemed;
   };

   /*

    User actions

    */

   [[eosio::on_notify("eosio.token::transfer")]] generate_return_value
   generate(name from, name to, asset quantity, std::string memo);

   [[eosio::action]] generate_return_value mint(name owner, uint32_t amount, std::string data);

   [[eosio::action]] void transfer(name from, name to, std::vector<uint64_t> drops_ids, string memo);

   [[eosio::action]] destroy_return_value destroy(name owner, std::vector<uint64_t> drops_ids, string memo);

   [[eosio::action]] bind_return_value bind(name owner, std::vector<uint64_t> drops_ids);
   [[eosio::action]] void              unbind(name owner, std::vector<uint64_t> drops_ids);
   [[eosio::action]] void              cancelunbind(name owner);

   using generate_action     = eosio::action_wrapper<"generate"_n, &drops::generate>;
   using mint_action         = eosio::action_wrapper<"mint"_n, &drops::mint>;
   using transfer_action     = eosio::action_wrapper<"transfer"_n, &drops::transfer>;
   using destroy_action      = eosio::action_wrapper<"destroy"_n, &drops::destroy>;
   using bind_action         = eosio::action_wrapper<"bind"_n, &drops::bind>;
   using unbind_action       = eosio::action_wrapper<"unbind"_n, &drops::unbind>;
   using cancelunbind_action = eosio::action_wrapper<"cancelunbind"_n, &drops::cancelunbind>;

   /*

    Admin actions

    */

   [[eosio::action]] void enable(bool enabled);
   using enable_action = eosio::action_wrapper<"enable"_n, &drops::enable>;

   [[eosio::action]] void init();
   using init_action = eosio::action_wrapper<"init"_n, &drops::init>;

   // Dummy action that'll help the ABI export the generate_return_value struct
   [[eosio::action]] generate_return_value generatertrn();
   using generatertrn_action = eosio::action_wrapper<"generatertrn"_n, &drops::generatertrn>;

   /*

    Testnet actions

    */

   [[eosio::action]] void wipe();
   using wipe_action = eosio::action_wrapper<"wipe"_n, &drops::wipe>;

   [[eosio::action]] void wipesome();
   using wipesome_action = eosio::action_wrapper<"wipesome"_n, &drops::wipesome>;

   [[eosio::action]] void destroyall();
   using destroyall_action = eosio::action_wrapper<"destroyall"_n, &drops::destroyall>;

private:
   generate_return_value do_generate(name from, name to, asset quantity, std::vector<std::string> parsed);
   generate_return_value do_unbind(name from, name to, asset quantity, std::vector<std::string> parsed);

   void transfer_tokens(name to, asset amount, string memo);
   void transfer_ram(name to, asset amount, string memo);

   void buy_ram_bytes(int64_t bytes);
   void sell_ram_bytes(int64_t bytes);

   drop_row modify_drop_binding(name owner, uint64_t drop_id, bool bound);

   std::vector<std::string> split(const std::string& str, char delim);
};

} // namespace dropssystem
