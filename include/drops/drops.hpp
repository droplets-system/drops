#pragma once

#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio/singleton.hpp>

#include <drops/drops.hpp>
#include <drops/ram.hpp>
#include <drops/utils.hpp>

using namespace eosio;
using namespace std;

namespace dropssystem {

static constexpr symbol EOS = symbol{"EOS", 4};

static const string ERROR_INVALID_MEMO    = "Invalid transfer memo. (ex: \"<amount>,<data>\")";
static const string ERROR_DROP_NOT_FOUND  = "Drop not found.";
static const string ERROR_SYSTEM_DISABLED = "Drops system is disabled.";
static const string ERROR_OPEN_BALANCE    = "Account does not have an open balance.";
static const string MEMO_RAM_TRANSFER     = "Claiming RAM bytes.";

uint128_t combine_ids(const uint64_t& v1, const uint64_t& v2) { return (uint128_t{v1} << 64) | v2; }

class [[eosio::contract("drops")]] drops : public contract
{
public:
   using contract::contract;

   /**
    * ## TABLE `drops`
    *
    * ### params
    *
    * - `{uint64_t} seed` - (primary key) unique seed
    * - `{name} owner` - owner of the drop
    * - `{block_timestamp} created` - creation time
    * - `{bool} bound` - whether the drop is bound to an account
    *
    * ### example
    *
    * ```json
    * {
    *   "seed": 16355392114041409,
    *   "owner": "test.gm",
    *   "created": "2024-01-29T00:00:00.000",
    *   "bound": false
    * }
    * ```
    */
   struct [[eosio::table("drop")]] drop_row
   {
      uint64_t        seed;
      name            owner;
      block_timestamp created;
      bool            bound;
      uint64_t        primary_key() const { return seed; }
      uint128_t       by_owner() const { return ((uint128_t)owner.value << 64) | seed; }
   };

   /**
    * ## TABLE `state`
    *
    * ### params
    *
    * - `{block_timestamp} genesis` - genesis time when the contract was created
    * - `{int64_t} bytes_per_drop` - amount of RAM bytes required per minting drop
    * - `{bool} enabled` - whether the contract is enabled
    *
    * ### example
    *
    * ```json
    * {
    *   "genesis": "2024-01-29T00:00:00",
    *   "bytes_per_drop": 512,
    *   "enabled": true
    * }
    * ```
    */
   struct [[eosio::table("state")]] state_row
   {
      block_timestamp genesis        = current_block_time();
      int64_t         bytes_per_drop = 512; // 144 bytes primary row + 368 bytes secondary row
      bool            enabled        = true;
   };

   /**
    * ## TABLE `balances`
    *
    * ### params
    *
    * - `{name} owner` - (primary key) owner account
    * - `{uint64_t} drops` - total amount of drops owned
    * - `{int64_t} ram_bytes` - total amount of RAM bytes available by the owner
    *
    * ### example
    *
    * ```json
    * {
    *   "owner": "test.gm",
    *   "drops": 69,
    *   "ram_bytes": 2048
    * }
    * ```
    */
   struct [[eosio::table("balances")]] balances_row
   {
      name     owner;
      uint64_t drops;
      int64_t  ram_bytes;

      uint64_t primary_key() const { return owner.value; }
   };

   /**
    * ## TABLE `stat`
    *
    * ### params
    *
    * - `{uint64_t} drops` - total supply of drops
    * - `{int64_t} ram_bytes` - total available RAM bytes held by the contract
    *
    * ### example
    *
    * ```json
    * {
    *   "drops": 88888,
    *   "ram_bytes": 2048
    * }
    * ```
    */
   struct [[eosio::table("stat")]] stat_row
   {
      uint64_t drops;
      int64_t  ram_bytes;
   };

   typedef eosio::multi_index<
      "drop"_n,
      drop_row,
      eosio::indexed_by<"owner"_n, eosio::const_mem_fun<drop_row, uint128_t, &drop_row::by_owner>>>
                                                          drop_table;
   typedef eosio::singleton<"state"_n, state_row>         state_table;
   typedef eosio::multi_index<"balances"_n, balances_row> balances_table;
   typedef eosio::singleton<"stat"_n, stat_row>           stat_table;

   // @return
   struct destroy_return_value
   {
      uint64_t unbound_destroyed;
      int64_t  bytes_reclaimed;
   };

   // @user
   [[eosio::on_notify("*::transfer")]] int64_t
   on_transfer(const name from, const name to, const asset quantity, const string memo);

   // @user
   [[eosio::action]] int64_t generate(const name owner, const bool bound, const uint32_t amount, const string data);

   // @user
   [[eosio::action]] void transfer(const name from, const name to, const vector<uint64_t> drops_ids, const string memo);

   // @user
   [[eosio::action]] destroy_return_value
   destroy(const name owner, const vector<uint64_t> drops_ids, const string memo);

   // @user
   [[eosio::action]] int64_t bind(const name owner, const vector<uint64_t> drops_ids);

   // @user
   [[eosio::action]] int64_t unbind(const name owner, const vector<uint64_t> drops_ids);

   // @user
   [[eosio::action]] bool open(const name owner);

   // @user
   [[eosio::action]] int64_t claim(const name owner);

   // @admin
   [[eosio::action]] void enable(bool enabled);

   // @static
   static bool is_enabled(const name code)
   {
      state_table state(code, code.value);
      if (!state.exists())
         return false;
      return state.get().enabled;
   }

   // @static
   static void check_is_enabled(const name code) { check(is_enabled(code), ERROR_SYSTEM_DISABLED); }

   // action wrappers
   using generate_action = eosio::action_wrapper<"generate"_n, &drops::generate>;
   using transfer_action = eosio::action_wrapper<"transfer"_n, &drops::transfer>;
   using destroy_action  = eosio::action_wrapper<"destroy"_n, &drops::destroy>;
   using bind_action     = eosio::action_wrapper<"bind"_n, &drops::bind>;
   using unbind_action   = eosio::action_wrapper<"unbind"_n, &drops::unbind>;
   using enable_action   = eosio::action_wrapper<"enable"_n, &drops::enable>;
   using open_action     = eosio::action_wrapper<"open"_n, &drops::open>;
   using claim_action    = eosio::action_wrapper<"claim"_n, &drops::claim>;

// DEBUG (used to help testing)
#ifdef DEBUG
   [[eosio::action]] void test(const string data);

   // @debug
   [[eosio::action]] void
   cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows);
#endif

private:
   int64_t  get_bytes_per_drop();
   uint64_t hash_data(const string data);

   void transfer_tokens(const name to, const asset quantity, const string memo);
   void transfer_ram(const name to, const int64_t bytes, const string memo);
   void buy_ram_bytes(int64_t bytes);
   void sell_ram_bytes(int64_t bytes);
   void buy_ram(const asset quantity);

   void reduce_ram_bytes(const name owner, const int64_t bytes);
   void add_ram_bytes(const name owner, const int64_t bytes);

   void    check_drop_owner(const drop_row drop, const name owner);
   void    check_drop_bound(const drop_row drop, const bool bound);
   void    modify_owner(const uint64_t drop_id, const name current_owner, const name new_owner);
   void    modify_ram_payer(const uint64_t drop_id, const name owner, const bool bound);
   int64_t emplace_drops(const name owner, const bool bound, const uint32_t amount, const string data);
   bool    destroy_drop(const uint64_t drop_id, const name owner);

// DEBUG (used to help testing)
#ifdef DEBUG
   template <typename T>
   void clear_table(T& table, uint64_t rows_to_clear);
#endif
};

} // namespace dropssystem
