namespace eosiosystem {

asset ram_cost(uint32_t bytes, symbol core_symbol);

asset ram_cost_with_fee(uint32_t bytes, symbol core_symbol);

asset ram_proceeds_minus_fee(uint32_t bytes, symbol core_symbol);

} // namespace eosiosystem