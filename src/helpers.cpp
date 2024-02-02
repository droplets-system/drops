namespace dropssystem {

void drops::buy_ram(const asset quantity)
{
   eosiosystem::system_contract::buyram_action buyram{"eosio"_n, {get_self(), "active"_n}};
   buyram.send(get_self(), get_self(), quantity);
}

void drops::buy_ram_bytes(const int64_t bytes)
{
   eosiosystem::system_contract::buyrambytes_action buyrambytes{"eosio"_n, {get_self(), "active"_n}};
   buyrambytes.send(get_self(), get_self(), bytes);
}

void drops::sell_ram_bytes(const int64_t bytes)
{
   eosiosystem::system_contract::sellram_action sellram{"eosio"_n, {get_self(), "active"_n}};
   sellram.send(get_self(), bytes);
}

void drops::transfer_tokens(const name to, const asset quantity, const string memo)
{
   token::transfer_action transfer_act{"eosio.token"_n, {{get_self(), "active"_n}}};
   transfer_act.send(get_self(), to, quantity, memo);
}

void drops::transfer_ram(const name to, const int64_t bytes, const string memo)
{
   eosiosystem::system_contract::ramtransfer_action ramtransfer{"eosio"_n, {get_self(), "active"_n}};
   ramtransfer.send(get_self(), to, bytes, memo);
}

} // namespace dropssystem