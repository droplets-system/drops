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

void drops::log_balances(const name owner, const int64_t drops, const int64_t ram_bytes)
{
   drops::logbalances_action logbalances_act{get_self(), {get_self(), "active"_n}};
   logbalances_act.send(owner, drops, ram_bytes);
}

void drops::logbalances(const name owner, const int64_t drops, const int64_t ram_bytes)
{
   require_auth(get_self());
   require_recipient(owner);
}

void drops::log_stat(const int64_t drops, const int64_t ram_bytes)
{
   drops::logstat_action logstat_act{get_self(), {get_self(), "active"_n}};
   logstat_act.send(drops, ram_bytes);
}

void drops::logstat(const int64_t drops, const int64_t ram_bytes) { require_auth(get_self()); }

} // namespace dropssystem