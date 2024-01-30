#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

#include <string>

using namespace eosio;

class [[eosio::contract("eosio")]] system_contract : public contract
{
public:
    using contract::contract;

    /**
     * Buy a specific amount of ram bytes action. Increases receiver's ram in quantity of bytes provided.
     * An inline transfer from receiver to system contract of tokens will be executed.
     *
     * @param payer - the ram buyer,
     * @param receiver - the ram receiver,
     * @param bytes - the quantity of ram to buy specified in bytes.
     */
    [[eosio::action]]
    void buyrambytes( const name& payer, const name& receiver, uint32_t bytes )
    {
        print("noop");
    }

    /**
     * Sell ram action, reduces quota by bytes and then performs an inline transfer of tokens
     * to receiver based upon the average purchase price of the original quota.
     *
     * @param account - the ram seller account,
     * @param bytes - the amount of ram to sell in bytes.
     */
    [[eosio::action]]
    void sellram( const name& account, int64_t bytes )
    {
        print("noop");
    }

    [[eosio::action]]
    void init()
    {
        rammarket _rammarket("eosio"_n, "eosio"_n.value);
        auto itr = _rammarket.find(symbol("RAMCORE", 4).raw());

        if (itr == _rammarket.end()) {
            _rammarket.emplace("eosio"_n, [&](auto& m) {
                m.supply.amount = 100000000000000;
                m.supply.symbol = symbol("RAMCORE", 4);
                m.base.balance.amount = 129542469746;
                m.base.balance.symbol = symbol("RAM", 0);
                m.quote.balance.amount = 147223045946;
                m.quote.balance.symbol = symbol("EOS", 4);
            });
        }
    }

    // action wrappers
    using sellram_action = eosio::action_wrapper<"sellram"_n, &system_contract::sellram>;
    using buyrambytes_action = eosio::action_wrapper<"buyrambytes"_n, &system_contract::buyrambytes>;

    struct [[eosio::table, eosio::contract("eosio.system")]] exchange_state {
        asset    supply;
        struct connector {
            asset balance;
            double weight = .5;
        };
        connector base;
        connector quote;
        uint64_t primary_key()const { return supply.symbol.raw(); }
    };

    typedef eosio::multi_index< "rammarket"_n, exchange_state > rammarket;
};
