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
   if (owner != get_self())
      require_recipient(owner);
}

void drops::log_drops(const name owner, const int64_t amount, const int64_t before_drops, const int64_t drops)
{
   drops::logdrops_action logdrops_act{get_self(), {get_self(), "active"_n}};
   logdrops_act.send(owner, amount, before_drops, drops);
}

void drops::logdrops(const name owner, const int64_t amount, const int64_t before_drops, const int64_t drops)
{
   require_auth(get_self());
   if (owner != get_self()) {
      require_recipient(owner);
   }
}

[[eosio::action]] void drops::logdestroy(const name                    owner,
                                         const vector<uint64_t>        drops_ids,
                                         const vector<block_timestamp> created,
                                         const int64_t                 destroyed,
                                         const int64_t                 unbound_destroyed,
                                         const int64_t                 bytes_reclaimed,
                                         optional<string>              memo,
                                         optional<name>                to_notify)
{
   require_auth(get_self());
   if (owner != get_self()) {
      require_recipient(owner);
   }
   if (to_notify) {
      require_recipient(*to_notify);
   }
}

} // namespace dropssystem