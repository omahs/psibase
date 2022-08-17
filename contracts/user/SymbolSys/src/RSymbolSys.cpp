#include <contracts/system/CommonSys.hpp>
#include <contracts/user/RSymbolSys.hpp>
#include <contracts/user/SymbolSys.hpp>
#include <psibase/dispatch.hpp>
#include <psibase/print.hpp>
#include <psibase/serveGraphQL.hpp>
#include <psibase/serveSimpleUI.hpp>
#include <string>
#include <vector>

using namespace UserContract;
using namespace std;
using namespace psibase;

using Tables = psibase::ContractTables<psibase::WebContentTable>;

namespace
{
   auto simplePage = [](std::string str)
   {
      auto d = string("<html><div>" + str + "</div></html>");

      return HttpReply{.contentType = "text/html", .body = std::vector<char>{d.begin(), d.end()}};
   };

}

optional<HttpReply> RSymbolSys::serveSys(HttpRequest request)
{
   if (auto result = at<system_contract::CommonSys>().serveCommon(request).unpack())
      return result;

   if (auto result = servePackAction<SymbolSys>(request))
      return result;

   if (auto result = serveContent(request, Tables{getReceiver()}))
      return result;

   if (auto result = _serveRestEndpoints(request))
      return result;

   // if (auto result = serveGraphQL(request, TokenQuery{}))
   //    return result;
   return nullopt;
}

void RSymbolSys::storeSys(string path, string contentType, vector<char> content)
{
   check(getSender() == getReceiver(), "wrong sender");
   storeContent(move(path), move(contentType), move(content), Tables{getReceiver()});
}

std::optional<HttpReply> RSymbolSys::_serveRestEndpoints(HttpRequest& request)
{
   auto to_json = [](const auto& obj)
   {
      auto json = psio::convert_to_json(obj);
      return HttpReply{
          .contentType = "application/json",
          .body        = {json.begin(), json.end()},
      };
   };

   if (request.method == "GET")
   {
      if (request.target == "/simple" || request.target == "/action_templates")
      {
         if (request.target == "/simple")
         {
            request.target = "/";
         }
         if (auto result = serveSimpleUI<SymbolSys, true>(request))
            return result;
      }
   }

   return std::nullopt;
}

PSIBASE_DISPATCH(UserContract::RSymbolSys)