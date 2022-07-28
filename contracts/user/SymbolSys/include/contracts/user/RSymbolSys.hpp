#pragma once
#include "SymbolSys.hpp"

namespace UserContract
{
   class RSymbolSys : public psibase::Contract<RSymbolSys>
   {
     public:
      static constexpr auto contract = psibase::AccountNumber("rpc-sym-sys");

      auto serveSys(psibase::RpcRequestData request) -> std::optional<psibase::RpcReplyData>;
      void storeSys(std::string path, std::string contentType, std::vector<char> content);

     private:
      std::optional<psibase::RpcReplyData> _serveRestEndpoints(psibase::RpcRequestData& request);
   };
   PSIO_REFLECT(RSymbolSys, method(serveSys, request), method(storeSys, path, contentType, content))

}  // namespace UserContract
