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

void drops::burn_ram(const int64_t bytes, const string memo)
{
   eosiosystem::system_contract::ramburn_action ramburn{"eosio"_n, {get_self(), "active"_n}};
   ramburn.send(get_self(), bytes, memo);
}

void drops::log_ram_bytes(const name    owner,
                          const int64_t bytes,
                          const int64_t before_ram_bytes,
                          const int64_t ram_bytes)
{
   drops::logrambytes_action logrambytes_act{get_self(), {get_self(), "active"_n}};
   logrambytes_act.send(owner, bytes, before_ram_bytes, ram_bytes);
}

void drops::logrambytes(const name owner, const int64_t bytes, const int64_t before_ram_bytes, const int64_t ram_bytes)
{
   require_auth(get_self());
   notify(owner);
}

void drops::log_drops(const name owner, const int64_t amount, const int64_t before_drops, const int64_t drops)
{
   drops::logdrops_action logdrops_act{get_self(), {get_self(), "active"_n}};
   logdrops_act.send(owner, amount, before_drops, drops);
}

void drops::logdrops(const name owner, const int64_t amount, const int64_t before_drops, const int64_t drops)
{
   require_auth(get_self());
   notify(owner);
}

[[eosio::action]] void drops::logdestroy(const name             owner,
                                         const vector<drop_row> drops,
                                         const int64_t          destroyed,
                                         const int64_t          unbound_destroyed,
                                         const int64_t          bytes_reclaimed,
                                         const optional<string> memo,
                                         const optional<name>   to_notify)
{
   require_auth(get_self());
   notify(owner);
   notify(to_notify);
}

[[eosio::action]] void drops::logburn(const name             owner,
                                      const vector<drop_row> drops,
                                      const int64_t          droplets_burned,
                                      const int64_t          bytes_burned,
                                      const optional<string> memo,
                                      const optional<name>   to_notify)
{
   require_auth(get_self());
   notify(owner);
   notify(to_notify);
}

[[eosio::action]] void drops::loggenerate(const name             owner,
                                          const vector<drop_row> drops,
                                          const int64_t          generated,
                                          const int64_t          bytes_used,
                                          const int64_t          bytes_balance,
                                          const string           data,
                                          const optional<name>   to_notify,
                                          const optional<string> memo)
{
   require_auth(get_self());
   notify(owner);
   notify(to_notify);
}

} // namespace dropssystem