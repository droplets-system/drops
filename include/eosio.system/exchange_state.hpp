#pragma once

#include <eosio/asset.hpp>
#include <eosio/multi_index.hpp>

namespace eosiosystem {

   using eosio::asset;
   using eosio::symbol;

   /**
    * Uses Bancor math to create a 50/50 relay between two asset types.
    *
    * The state of the bancor exchange is entirely contained within this struct.
    * There are no external side effects associated with using this API.
    */
   struct [[eosio::table, eosio::contract("eosio.system")]] exchange_state {
      asset    supply;

      struct connector {
         asset balance;
         double weight = .5;

         EOSLIB_SERIALIZE( connector, (balance)(weight) )
      };

      connector base;
      connector quote;

      uint64_t primary_key()const { return supply.symbol.raw(); }

      asset convert_to_exchange( connector& reserve, const asset& payment );
      asset convert_from_exchange( connector& reserve, const asset& tokens );
      asset convert( const asset& from, const symbol& to );
      asset direct_convert( const asset& from, const symbol& to );


      static int64_t get_bancor_input(int64_t out_reserve, int64_t inp_reserve, int64_t out)
      {
         const double ob = out_reserve;
         const double ib = inp_reserve;

         int64_t inp = (ib * out) / (ob - out);

         if (inp < 0)
            inp = 0;

         return inp;
      }

      static int64_t get_bancor_output(int64_t inp_reserve, int64_t out_reserve, int64_t inp)
      {
         const double ib = inp_reserve;
         const double ob = out_reserve;
         const double in = inp;

         int64_t out = int64_t((in * ob) / (ib + in));

         if (out < 0)
            out = 0;

         return out;
      }

      EOSLIB_SERIALIZE( exchange_state, (supply)(base)(quote) )
   };

   typedef eosio::multi_index< "rammarket"_n, exchange_state > rammarket;
} /// namespace eosiosystem
