#include <secp256k1.h>
#include <base_contracts/verify_ec_sys.hpp>
#include <psibase/contract_entry.hpp>
#include <psibase/from_bin.hpp>

using namespace psibase;

// TODO: regenerate wasm with this already initialized
auto context = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);

// TODO: r1
extern "C" [[clang::export_name("verify")]] void verify()
{
   auto act     = get_current_action();
   auto data    = eosio::convert_from_bin<verify_data>(act.raw_data);
   auto pub_key = unpack_all<eosio::public_key>(data.claim.raw_data, "extra data in claim");
   auto sig     = unpack_all<eosio::signature>(data.proof, "extra data in proof");
   auto hash    = data.transaction_hash.extract_as_byte_array();

   auto* k1_pub_key = std::get_if<0>(&pub_key);
   auto* k1_sig     = std::get_if<0>(&sig);
   eosio::check(k1_pub_key && k1_sig, "only k1 currently supported");

   secp256k1_pubkey parsed_pub_key;
   eosio::check(secp256k1_ec_pubkey_parse(context, &parsed_pub_key, k1_pub_key->data(),
                                          k1_pub_key->size()) == 1,
                "pubkey parse failed");

   // TODO: first byte of eosio::signature (recovery param) is ignored; change
   //       the format to not include it?
   secp256k1_ecdsa_signature parsed_sig;
   eosio::check(
       secp256k1_ecdsa_signature_parse_compact(context, &parsed_sig, k1_sig->data() + 1) == 1,
       "signature parse failed");

   // secp256k1_ecdsa_verify requires normalized, but we don't
   secp256k1_ecdsa_signature normalized;
   secp256k1_ecdsa_signature_normalize(context, &normalized, &parsed_sig);

   eosio::check(secp256k1_ecdsa_verify(context, &normalized, hash.data(), &parsed_pub_key) == 1,
                "incorrect signature");
}

extern "C" void called(account_num this_contract, account_num sender)
{
   abort_message("this contract has no actions");
}

extern "C" void __wasm_call_ctors();
extern "C" void start(account_num this_contract)
{
   __wasm_call_ctors();
}
