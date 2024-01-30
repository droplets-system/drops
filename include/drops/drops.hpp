#pragma once

#include <eosio/singleton.hpp>
#include <eosio.system/eosio.system.hpp>
#include <eosio/crypto.hpp>
#include <eosio.system/exchange_state.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace eosio;
using namespace std;

static constexpr symbol EOS = symbol{"EOS", 4};

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
     *
     * ### example
     *
     * ```json
     * {
     *   "seed": 16355392114041409,
     *   "owner": "test.gm",
     *   "created": "2024-01-29T00:00:00"
     * }
     * ```
     */
    struct [[eosio::table("drop")]] drop_row
    {
        uint64_t          seed;
        name              owner;
        block_timestamp   created;

        uint64_t          primary_key() const { return seed; }
        uint128_t         by_owner() const { return ((uint128_t)owner.value << 64) | seed; }
    };
    typedef eosio::multi_index<"drop"_n, drop_row,
        eosio::indexed_by<"owner"_n, eosio::const_mem_fun<drop_row, uint128_t, &drop_row::by_owner>>
    > drop_table;

    /**
     * ## TABLE `state`
     *
     * ### params
     *
     * - `{block_timestamp} genesis` - genesis time when the contract was created
     * - `{int64_t} bytes_per_drop` - amount of RAM bytes required per minting drop
     * - `{bool} paused` - whether the contract is paused
     *
     * ### example
     *
     * ```json
     * {
     *   "genesis": "2024-01-29T00:00:00",
     *   "bytes_per_drop": 512,
     *   "paused": false
     * }
     * ```
     */
    struct [[eosio::table("state")]] state_row
    {
        block_timestamp   genesis = current_block_time();
        int64_t           bytes_per_drop = 512; // 144 bytes primary row + 368 bytes secondary row
        bool              paused = false;
    };
    typedef eosio::singleton<"state"_n, state_row> state_table;

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name from, const name to, const asset quantity, const string memo);

    [[eosio::on_notify("eosio::ramtransfer")]]
    void on_ram_transfer(const name from, const name to, const int64_t bytes);

    [[eosio::action]]
    void transfer( const name from, const name to, const vector<uint64_t> drops_ids, const string memo );

    [[eosio::action]]
    void destroy( const name owner, const vector<uint64_t> drops_ids, const bool ram_transfer, const string memo );

    [[eosio::action]]
    void pause( bool paused );

    // action wrappers
    using transfer_action = eosio::action_wrapper<"transfer"_n, &drops::transfer>;
    using destroy_action = eosio::action_wrapper<"destroy"_n, &drops::destroy>;
    using pause_action = eosio::action_wrapper<"pause"_n, &drops::pause>;

    // DEBUG (used to help testing)
    #ifdef DEBUG
    [[eosio::action]]
    void test( const string data );

    // @debug
    [[eosio::action]]
    void cleartable( const name table_name, const optional<name> scope, const optional<uint64_t> max_rows );
    #endif

private:
    void do_generate( const name owner, const uint32_t amount, const asset quantity, const string seed );
    void check_is_paused();
    void buy_ram_bytes(const int64_t bytes );
    void transfer_to(const name to, const asset quantity, const string memo );
    uint64_t hash_data( const string data );
    int64_t get_bytes_per_drop();

    // DEBUG (used to help testing)
    #ifdef DEBUG
    template <typename T>
    void clear_table( T& table, uint64_t rows_to_clear );
    #endif
};
