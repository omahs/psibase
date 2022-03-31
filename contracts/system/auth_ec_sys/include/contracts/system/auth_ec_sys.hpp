#pragma once

#include <eosio/crypto.hpp>
#include <psibase/intrinsic.hpp>
#include <psibase/native_tables.hpp>

// TODO
namespace eosio
{
   PSIO_REFLECT(webauthn_public_key, key, user_presence, rpid)
}

namespace system_contract::auth_ec_sys
{
   static constexpr psibase::AccountNumber contract = psibase::AccountNumber("auth-ec-sys");

   struct auth_check
   {
      psibase::action             action;
      std::vector<psibase::claim> claims;
   };
   PSIO_REFLECT(auth_check, action, claims)

   struct set_key
   {
      psibase::account_num account;
      psibase::PublicKey   key;
   };
   PSIO_REFLECT(set_key, account, key)

   // TODO: remove. This is just a temporary approach for creating an account with a key.
   struct create_account
   {
      using return_type = psibase::account_num;

      psibase::AccountNumber name       = {};
      psibase::PublicKey     public_key = {};
      bool                   allow_sudo = {};
   };
   PSIO_REFLECT(create_account, name, public_key, allow_sudo)

   using action = std::variant<auth_check, set_key, create_account>;

   template <typename T, typename R = typename T::return_type>
   R call(psibase::account_num sender, T args)
   {
      auto result = psibase::call(psibase::action{
          .sender   = sender,
          .contract = contract,
          .raw_data = psio::convert_to_frac(action{std::move(args)}),
      });
      if constexpr (!std::is_same_v<R, void>)
         return psio::convert_from_frac<R>(result);
   }
}  // namespace system_contract::auth_ec_sys
