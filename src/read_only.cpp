[[eosio::action, eosio::read_only]] asset dropssystem::drops::ramcost(const int64_t bytes)
{
   return eosiosystem::ram_cost_with_fee(bytes, EOS);
}

[[eosio::action, eosio::read_only]] int64_t dropssystem::drops::bytescost(const asset quantity)
{
   return eosiosystem::bytes_cost_with_fee(quantity);
}
