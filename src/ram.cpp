#include <cmath>
#include <eosio.system/eosio.system.hpp>

namespace eosiosystem {

using eosio::asset;

int64_t get_bancor_input(int64_t out_reserve, int64_t inp_reserve, int64_t out)
{
   const double ob = out_reserve;
   const double ib = inp_reserve;

   int64_t inp = (ib * out) / (ob - out);

   if (inp < 0)
      inp = 0;

   return inp;
}

int64_t get_bancor_output(int64_t inp_reserve, int64_t out_reserve, int64_t inp)
{
   const double ib = inp_reserve;
   const double ob = out_reserve;
   const double in = inp;

   int64_t out = int64_t((in * ob) / (ib + in));

   if (out < 0)
      out = 0;

   return out;
}

double round_to(double value, double precision = 1.0, bool up = false)
{
   if (up) {
      return std::ceil(value / precision) * precision;
   } else {
      return std::floor(value / precision) * precision;
   }
}

asset get_fee(const asset quantity)
{
   asset fee  = quantity;
   fee.amount = (fee.amount + 199) / 200; /// .5% fee (round up)
   return fee;
}

int64_t bytes_cost_with_fee(const asset quantity)
{
   name      system_account = "eosio"_n;
   rammarket _rammarket(system_account, system_account.value);

   const asset fee                = get_fee(quantity);
   const asset quantity_after_fee = quantity - fee;

   auto          itr         = _rammarket.find(system_contract::ramcore_symbol.raw());
   const int64_t ram_reserve = itr->base.balance.amount;
   const int64_t eos_reserve = itr->quote.balance.amount;
   const int64_t cost        = get_bancor_output(eos_reserve, ram_reserve, quantity_after_fee.amount);
   return cost;
}

asset ram_cost(uint32_t bytes, symbol core_symbol)
{
   name          system_account = "eosio"_n;
   rammarket     _rammarket(system_account, system_account.value);
   auto          itr         = _rammarket.find(system_contract::ramcore_symbol.raw());
   const int64_t ram_reserve = itr->base.balance.amount;
   const int64_t eos_reserve = itr->quote.balance.amount;
   const int64_t cost        = get_bancor_input(ram_reserve, eos_reserve, bytes);
   return asset{cost, core_symbol};
}

asset ram_cost_with_fee(uint32_t bytes, symbol core_symbol)
{
   const asset   cost          = ram_cost(bytes, core_symbol);
   const int64_t cost_plus_fee = cost.amount / double(0.995);
   return asset{cost_plus_fee, core_symbol};
}

// asset direct_convert(const asset& from, const symbol& to)
asset ram_proceeds_minus_fee(uint32_t bytes, symbol core_symbol)
{
   asset from = asset{bytes, system_contract::ram_symbol};

   symbol    to             = core_symbol;
   name      system_account = "eosio"_n;
   rammarket _rammarket(system_account, system_account.value);
   auto      itr = _rammarket.find(system_contract::ramcore_symbol.raw());

   const auto& sell_symbol  = from.symbol;
   const auto& base_symbol  = itr->base.balance.symbol;
   const auto& quote_symbol = itr->quote.balance.symbol;
   check(sell_symbol != to, "cannot convert to the same symbol");

   asset out(0, to);
   if (sell_symbol == base_symbol && to == quote_symbol) {
      out.amount = get_bancor_output(itr->base.balance.amount, itr->quote.balance.amount, from.amount);
   } else if (sell_symbol == quote_symbol && to == base_symbol) {
      out.amount = get_bancor_output(itr->quote.balance.amount, itr->base.balance.amount, from.amount);
   } else {
      check(false, "invalid conversion");
   }

   out -= get_fee(out);
   return out;
}

} // namespace eosiosystem