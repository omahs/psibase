#include <secp256k1_preallocated.h>
#include <contracts/system/verify_ec_sys.hpp>
#include <psibase/contract_entry.hpp>
#include <psibase/from_bin.hpp>

using namespace psibase;

char               pre[1024];
secp256k1_context* context = nullptr;

extern "C" void __wasm_call_ctors();

// Called by wasm-ctor-eval during build
extern "C" [[clang::export_name("prestart")]] void prestart()
{
   __wasm_call_ctors();
   check(secp256k1_context_preallocated_size(SECP256K1_CONTEXT_VERIFY) <= sizeof(pre), "?");
   context = secp256k1_context_preallocated_create(&pre, SECP256K1_CONTEXT_VERIFY);
}

// TODO: r1
extern "C" [[clang::export_name("verify")]] void verify()
{
   check(context, "verify_ec_sys not fully built");

   auto act     = get_current_action();
   auto data    = psio::convert_from_frac<verify_data>(act.raw_data);
   auto pub_key = psio::convert_from_frac<PublicKey>(data.claim.raw_data);  // TODO: verify no extra
   auto sig     = psio::convert_from_frac<Signature>(data.proof);           // TODO: verify no extra

   auto* k1_pub_key = std::get_if<0>(&pub_key);
   auto* k1_sig     = std::get_if<0>(&sig);
   eosio::check(k1_pub_key && k1_sig, "only k1 currently supported");

   secp256k1_pubkey parsed_pub_key;
   eosio::check(secp256k1_ec_pubkey_parse(context, &parsed_pub_key, k1_pub_key->data(),
                                          k1_pub_key->size()) == 1,
                "pubkey parse failed");

   secp256k1_ecdsa_signature parsed_sig;
   eosio::check(secp256k1_ecdsa_signature_parse_compact(context, &parsed_sig, k1_sig->data()) == 1,
                "signature parse failed");

   // secp256k1_ecdsa_verify requires normalized, but we don't
   secp256k1_ecdsa_signature normalized;
   secp256k1_ecdsa_signature_normalize(context, &normalized, &parsed_sig);

   eosio::check(
       secp256k1_ecdsa_verify(context, &normalized,
                              reinterpret_cast<const unsigned char*>(data.transaction_hash.data()),
                              &parsed_pub_key) == 1,
       "incorrect signature");
}

extern "C" void called(account_num this_contract, account_num sender)
{
   //abort_message_str("this contract has no actions");
}

// Caution! Don't replace with version in dispatcher!
extern "C" void start(account_num this_contract) {}
