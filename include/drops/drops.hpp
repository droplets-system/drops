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

// error messages
static const string ERROR_INVALID_MEMO       = "Invalid transfer memo. (ex: \"<receiver>\")";
static const string ERROR_DROP_NOT_FOUND     = "Drop not found.";
static const string ERROR_SYSTEM_DISABLED    = "Drops system is disabled.";
static const string ERROR_OPEN_BALANCE       = "Account does not have an open balance.";
static const string ERROR_ACCOUNT_NOT_EXISTS = "Account does not exist.";
static const string ERROR_NO_DROPS           = "No drops were provided.";

// memo messages
static const string MEMO_RAM_TRANSFER      = "Claiming RAM bytes.";
static const string MEMO_RAM_SOLD_TRANSFER = "Claiming sold RAM bytes.";

// feature flags
static const bool FLAG_FORCE_RECEIVER_TO_BE_SENDER = true;

// not available until system contract supports `ramtransfer`
static const bool FLAG_ENABLE_RAM_TRANSFER_ON_CLAIM = false;

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
    * - `{uint64_t} sequence` - sequence is used as a salt to add an extra layer of complexity and randomness to the
    * hashing process.
    * - `{bool} enabled` - whether the contract is enabled
    *
    * ### example
    *
    * ```json
    * {
    *   "genesis": "2024-01-29T00:00:00",
    *   "bytes_per_drop": 277,
    *   "sequence": 0,
    *   "enabled": true
    * }
    * ```
    */
   struct [[eosio::table("state")]] state_row
   {
      block_timestamp genesis        = current_block_time();
      int64_t         bytes_per_drop = 277; // 133 bytes primary row + 144 bytes secondary row
      uint64_t        sequence       = 0;   // auto-incremented on each drop generation
      bool            enabled        = true;
   };

   /**
    * ## TABLE `balances`
    *
    * ### params
    *
    * - `{name} owner` - (primary key) owner account
    * - `{int64_t} drops` - total amount of drops owned
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
      name    owner;
      int64_t drops;
      int64_t ram_bytes;

      uint64_t primary_key() const { return owner.value; }
   };

   typedef eosio::multi_index<
      "drop"_n,
      drop_row,
      eosio::indexed_by<"owner"_n, eosio::const_mem_fun<drop_row, uint128_t, &drop_row::by_owner>>>
                                                          drop_table;
   typedef eosio::singleton<"state"_n, state_row>         state_table;
   typedef eosio::multi_index<"balances"_n, balances_row> balances_table;

   // @return
   struct generate_return_value
   {
      int64_t bytes_used;
      int64_t bytes_balance;
   };

   // @return
   struct destroy_return_value
   {
      int64_t unbound_destroyed;
      int64_t bytes_reclaimed;
   };

   // @user
   [[eosio::on_notify("*::transfer")]] int64_t
   on_transfer(const name from, const name to, const asset quantity, const string memo);

   // @user
   [[eosio::action]] generate_return_value generate(
      const name owner, const bool bound, const uint32_t amount, const string data, const optional<name> to_notify);

   // @user
   [[eosio::action]] void
   transfer(const name from, const name to, const vector<uint64_t> drops_ids, const optional<string> memo);

   // @user
   [[eosio::action]] destroy_return_value destroy(const name             owner,
                                                  const vector<uint64_t> drops_ids,
                                                  const optional<string> memo,
                                                  const optional<name>   to_notify);

   // @user
   [[eosio::action]] int64_t bind(const name owner, const vector<uint64_t> drops_ids);

   // @user
   [[eosio::action]] int64_t unbind(const name owner, const vector<uint64_t> drops_ids);

   /**
    * ## ACTION `open`
    *
    * - **authority**: `owner`
    *
    * Opens balances table row for owner account.
    * Transaction silent pass if balances already opened.
    * Action must be auth'ed by owner to prove ownership before accepting RAM bytes deposits.
    *
    * ### params
    *
    * - `{name} owner` - owner account to open balances
    *
    * ### example
    *
    * ```bash
    * $ cleos push action core.drops open '["alice"]' -p alice
    * ```
    */
   [[eosio::action]] bool open(const name owner);

   /**
    * ## ACTION `claim`
    *
    * - **authority**: `owner`
    *
    * Returns any available RAM balance on contract balances to owner.
    * Transaction silently passes if RAM bytes is 0.
    * Owner is the recipient of claimable bytes (cannot claim for another account).
    *
    * ### params
    *
    * - `{name} owner` - owner account to claim RAM bytes
    *
    * ### example
    *
    * ```bash
    * $ cleos push action core.drops claim '["alice"]' -p alice
    * ```
    */
   [[eosio::action]] int64_t claim(const name owner);

   // @admin
   [[eosio::action]] void enable(bool enabled);

   // @logging
   [[eosio::action]] void
   logrambytes(const name owner, const int64_t bytes, const int64_t before_ram_bytes, const int64_t ram_bytes);

   // @logging
   [[eosio::action]] void
   logdrops(const name owner, const int64_t amount, const int64_t before_drops, const int64_t drops);

   // @logging
   [[eosio::action]] void logdestroy(const name             owner,
                                     const vector<drop_row> drops,
                                     const int64_t          destroyed,
                                     const int64_t          unbound_destroyed,
                                     const int64_t          bytes_reclaimed,
                                     optional<string>       memo,
                                     optional<name>         to_notify);

   // @logging
   [[eosio::action]] void loggenerate(const name             owner,
                                      const vector<drop_row> drops,
                                      const int64_t          generated,
                                      const int64_t          bytes_used,
                                      const int64_t          bytes_balance,
                                      const string           data,
                                      optional<name>         to_notify);

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

   using logrambytes_action = eosio::action_wrapper<"logrambytes"_n, &drops::logrambytes>;
   using logdrops_action    = eosio::action_wrapper<"logdrops"_n, &drops::logdrops>;
   using logdestroy_action  = eosio::action_wrapper<"logdestroy"_n, &drops::logdestroy>;
   using loggenerate_action = eosio::action_wrapper<"loggenerate"_n, &drops::loggenerate>;

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

   // helpers
   void transfer_tokens(const name to, const asset quantity, const string memo);
   void transfer_ram(const name to, const int64_t bytes, const string memo);
   void buy_ram_bytes(int64_t bytes);
   void sell_ram_bytes(int64_t bytes);
   void buy_ram(const asset quantity);
   void notify(const optional<name> to_notify);

   // ram balances helpers
   int64_t update_ram_bytes(const name owner, const int64_t bytes);
   int64_t add_ram_bytes(const name owner, const int64_t bytes);
   int64_t reduce_ram_bytes(const name owner, const int64_t bytes);
   int64_t modify_ram_bytes(const name owner, const int64_t bytes, const name ram_payer);
   int64_t get_ram_bytes(const name owner);

   // drop balances helpers
   void update_drops(const name from, const name to, const int64_t amount);
   void add_drops(const name owner, const int64_t amount);
   void reduce_drops(const name owner, const int64_t amount);
   void transfer_drops(const name from, const name to, const int64_t amount);

   // modify RAM operations
   void check_drop_owner(const drop_row drop, const name owner);
   void check_drop_bound(const drop_row drop, const bool bound);
   void modify_owner(const uint64_t drop_id, const name current_owner, const name new_owner);
   void modify_ram_payer(const uint64_t drop_id, const name owner, const bool bound);
   bool open_balance(const name owner, const name ram_payer);
   name auth_ram_payer(const name owner);

   // sequence
   uint64_t get_sequence();
   uint64_t set_sequence(const int64_t amount);

   // create and destroy
   generate_return_value emplace_drops(
      const name owner, const bool bound, const uint32_t amount, const string data, const optional<name> to_notify);
   drop_row destroy_drop(const uint64_t drop_id, const name owner);

   // logging
   void log_drops(const name owner, const int64_t amount, const int64_t before_drops, const int64_t drops);
   void log_ram_bytes(const name owner, const int64_t bytes, const int64_t before_ram_bytes, const int64_t ram_bytes);

// DEBUG (used to help testing)
#ifdef DEBUG
   template <typename T>
   void clear_table(T& table, uint64_t rows_to_clear);
#endif
};

} // namespace dropssystem
