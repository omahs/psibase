#pragma once
#include "SymbolSys.hpp"

namespace UserContract
{
   class RSymbolSys : public psibase::Contract<RSymbolSys>
   {
     public:
      static constexpr auto contract = psibase::AccountNumber("rpc-sym-sys");

      auto serveSys(psibase::HttpRequest request) -> std::optional<psibase::HttpReply>;
      void storeSys(std::string path, std::string contentType, std::vector<char> content);

     private:
      std::optional<psibase::HttpReply> _serveRestEndpoints(psibase::HttpRequest& request);
   };
   PSIO_REFLECT(RSymbolSys, method(serveSys, request), method(storeSys, path, contentType, content))

}  // namespace UserContract
