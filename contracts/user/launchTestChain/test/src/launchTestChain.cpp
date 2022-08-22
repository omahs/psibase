#define CATCH_CONFIG_MAIN

#include <contracts/system/commonErrors.hpp>
#include <psibase/DefaultTestChain.hpp>
#include <psibase/MethodNumber.hpp>
#include <psibase/testUtils.hpp>

#include "contracts/user/RSymbolSys.hpp"
#include "contracts/user/RTokenSys.hpp"
#include "contracts/user/TokenSys.hpp"

using namespace psibase;
using namespace psibase::benchmarking;
using UserContract::TokenSys;
using namespace UserContract::Errors;
using namespace UserContract;

namespace
{
   const psibase::String memo{"memo"};

   const std::vector<std::pair<AccountNumber, const char*>> neededContracts = {
       {TokenSys::contract, "TokenSys.wasm"},
       {NftSys::contract, "NftSys.wasm"},
       {SymbolSys::contract, "SymbolSys.wasm"},
       {RTokenSys::contract, "RTokenSys.wasm"},
       {RSymbolSys::contract, "RSymbolSys.wasm"}};
}  // namespace

SCENARIO("Testing default psibase chain")
{
   DefaultTestChain t(neededContracts, 1'000'000'000ul, 32, 32, 32, 38);

   auto        tokenSysRpc = t.as(RTokenSys::contract).at<RTokenSys>();
   std::string rpcUiDir    = "../contracts/user/TokenSys/ui/";
   tokenSysRpc.storeSys("/ui/index.js", "text/javascript", readWholeFile(rpcUiDir + "index.js"));

   auto        symbolSysRpc   = t.as(RSymbolSys::contract).at<RSymbolSys>();
   std::string symbolRpcUiDir = "../contracts/user/SymbolSys/ui/symbol-sys/dist/";
   symbolSysRpc.storeSys("/index.html", "text/html", readWholeFile(symbolRpcUiDir + "index.html"));
   symbolSysRpc.storeSys("/assets/index.c26d6879.js", "text/javascript",
                         readWholeFile(symbolRpcUiDir + "assets/index.c26d6879.js"));
   symbolSysRpc.storeSys("/vite.svg", "image/svg+xml", readWholeFile(symbolRpcUiDir + "vite.svg"));
   symbolSysRpc.storeSys("/assets/react.35ef61ed.svg", "image/svg+xml",
                         readWholeFile(symbolRpcUiDir + "assets/react.35ef61ed.svg"));
   symbolSysRpc.storeSys("/assets/index.287b59a6.css", "text/css",
                         readWholeFile(symbolRpcUiDir + "assets/index.287b59a6.css"));

   auto alice = t.as(t.add_account("alice"_a));
   auto bob   = t.as(t.add_account("bob"_a));

   // Initialize user contracts
   alice.at<NftSys>().init();
   alice.at<TokenSys>().init();
   alice.at<SymbolSys>().init();

   auto sysIssuer = t.as(SymbolSys::contract).at<TokenSys>();
   auto sysToken  = TokenSys::sysToken;

   // Let sys token be tradeable
   sysIssuer.setTokenConf(sysToken, "untradeable"_m, false);

   // Distribute a few tokens
   auto userBalance = 1'000'000e8;
   sysIssuer.mint(sysToken, userBalance, memo);
   sysIssuer.credit(sysToken, alice, 9'000e8, memo);
   sysIssuer.credit(sysToken, bob, 1'000e8, memo);

   auto create = alice.at<TokenSys>().create(4, 1'000'000e4);
   alice.at<TokenSys>().mint(create.returnVal(), 100e4, memo);

   t.startBlock();

   /****** At t.getPath(): 
      drwxr-xr-x block_log
      drwxr-xr-x state
      drwxr-xr-x subjective
      drwxr-xr-x write_only
      ******/

   // Make a couple block
   t.finishBlock();
   t.startBlock();
   t.finishBlock();
   // Run the chain
   psibase::execute("rm -rf tester_psinode_db");
   psibase::execute("mkdir tester_psinode_db");
   psibase::execute("cp -a " + t.getPath() + "/. tester_psinode_db/");
   psibase::execute(
       "psinode --slow -o psibase.127.0.0.1.sslip.io tester_psinode_db --producer testchain "
       "--prods testchain");
}