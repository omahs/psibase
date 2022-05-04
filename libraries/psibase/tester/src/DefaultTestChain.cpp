#include <psibase/DefaultTestChain.hpp>

#include <utility>
#include <vector>

#include <contracts/system/account_sys.hpp>
#include <contracts/system/auth_ec_sys.hpp>
#include <contracts/system/auth_fake_sys.hpp>
#include <contracts/system/proxy_sys.hpp>
#include <contracts/system/rpc_account_sys.hpp>
#include <contracts/system/transaction_sys.hpp>
#include <contracts/system/verify_ec_sys.hpp>

using namespace psibase;

DefaultTestChain::DefaultTestChain(
    const std::vector<std::pair<AccountNumber, const char*>>& additionalContracts /* = {{}} */,
    const char*                                               snapshot /* = nullptr */,
    uint64_t                                                  state_size /* = 1024 * 1024 * 1024 */)
    : test_chain(snapshot, state_size)
{
   start_block();
   installSystemContracts();
   createSysContractAccounts();

   for (const auto& c : additionalContracts)
   {
      add_contract(c.first, c.second);
   }
}

void DefaultTestChain::installSystemContracts(bool show /* = false */)
{
   auto trace = pushTransaction(make_transaction(  //
       {                                           //
        Action{
            .sender   = AccountNumber{"foo"},  // ignored
            .contract = AccountNumber{"bar"},  // ignored
            .method   = {},
            .rawData  = psio::convert_to_frac(GenesisActionData{
                 .contracts =  // g.a.d--^ is config file for gen
                {
                     {
                         .contract     = system_contract::transaction_sys::contract,
                         .authContract = system_contract::auth_fake_sys::contract,
                         .flags        = system_contract::transaction_sys::contract_flags,
                         .code         = read_whole_file("transaction_sys.wasm"),
                    },
                     {
                         .contract     = system_contract::account_sys::contract,
                         .authContract = system_contract::auth_fake_sys::contract,
                         .flags        = system_contract::account_sys::contract_flags,
                         .code         = read_whole_file("account_sys.wasm"),
                    },
                     {
                         .contract     = proxyContractNum,
                         .authContract = system_contract::auth_fake_sys::contract,
                         .flags        = 0,
                         .code         = read_whole_file("proxy_sys.wasm"),
                    },
                     {
                         .contract     = system_contract::auth_fake_sys::contract,
                         .authContract = system_contract::auth_fake_sys::contract,
                         .flags        = 0,
                         .code         = read_whole_file("auth_fake_sys.wasm"),
                    },
                     {
                         .contract     = system_contract::auth_ec_sys::contract,
                         .authContract = system_contract::auth_fake_sys::contract,
                         .flags        = 0,
                         .code         = read_whole_file("auth_ec_sys.wasm"),
                    },
                     {
                         .contract     = system_contract::verify_ec_sys::contract,
                         .authContract = system_contract::auth_fake_sys::contract,
                         .flags        = 0,
                         .code         = read_whole_file("verify_ec_sys.wasm"),
                    },
                },
            }),
        }}));

   check(psibase::show(show, trace) == "", "Failed to install genesis contracts");
}

void DefaultTestChain::createSysContractAccounts(bool show /* = false */)
{
   transactor<system_contract::account_sys> asys{system_contract::transaction_sys::contract,
                                                 system_contract::account_sys::contract};

   auto trace = pushTransaction(make_transaction(  //
       {asys.startup(std::vector<AccountNumber>{
           system_contract::transaction_sys::contract,
           system_contract::account_sys::contract,
           proxyContractNum,
           system_contract::auth_fake_sys::contract,
           system_contract::auth_ec_sys::contract,
           system_contract::verify_ec_sys::contract,
       })}));

   check(psibase::show(show, trace) == "", "Failed to create system contract accounts");
}

AccountNumber DefaultTestChain::add_account(
    AccountNumber acc,
    AccountNumber authContract /* = AccountNumber("auth-fake-sys") */,
    bool          show /* = false */)
{
   transactor<system_contract::account_sys> asys(system_contract::transaction_sys::contract,
                                                 system_contract::account_sys::contract);

   auto trace = pushTransaction(  //
       make_transaction({asys.newAccount(acc, authContract, true)}));

   check(psibase::show(show, trace) == "", "Failed to add account");

   return acc;
}

AccountNumber DefaultTestChain::add_account(
    const char*   acc,
    AccountNumber authContract /* = AccountNumber("auth-fake-sys")*/,
    bool          show /* = false */)
{
   return add_account(AccountNumber(acc), authContract, show);
}

AccountNumber DefaultTestChain::add_ec_account(AccountNumber    name,
                                               const PublicKey& public_key,
                                               bool             show /* = false */)
{
   auto trace = pushTransaction(  //
       make_transaction(          //
           {{
               .sender   = system_contract::transaction_sys::contract,
               .contract = system_contract::auth_ec_sys::contract,
               .rawData  = psio::convert_to_frac(system_contract::auth_ec_sys::action{
                   system_contract::auth_ec_sys::create_account{
                        .name       = name,
                        .public_key = public_key,
                   }}),
           }}));

   check(psibase::show(show, trace) == "", "Failed to add ec account");
   auto& at = get_top_action(trace, 0);
   return psio::convert_from_frac<AccountNumber>(at.rawRetval);
}  // add_ec_account()

AccountNumber DefaultTestChain::add_ec_account(const char*      name,
                                               const PublicKey& public_key,
                                               bool             show /* = false */)
{
   return add_ec_account(AccountNumber(name), public_key, show);
}

AccountNumber DefaultTestChain::add_contract(AccountNumber acc,
                                             const char*   filename,
                                             bool          show /* = false */)
{
   add_account(acc, AccountNumber("auth-fake-sys"), show);
   transactor<system_contract::transaction_sys> tsys{acc,
                                                     system_contract::transaction_sys::contract};

   auto trace =
       pushTransaction(make_transaction({{tsys.setCode(acc, 0, 0, read_whole_file(filename))}}));

   check(psibase::show(show, trace) == "", "Failed to create contract");

   return acc;
}  // add_contract()

AccountNumber DefaultTestChain::add_contract(const char* acc,
                                             const char* filename,
                                             bool        show /* = false */)
{
   return add_contract(AccountNumber(acc), filename, show);
}
