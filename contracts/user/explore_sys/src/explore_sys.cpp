#include "contracts/user/explore_sys.hpp"

#include <contracts/system/proxy_sys.hpp>
#include <psibase/KVGraphQLConnection.hpp>
#include <psibase/dispatch.hpp>
#include <psibase/serveGraphQL.hpp>

using table_num = uint16_t;

static constexpr table_num web_content_table = 1;

inline auto webContentKey(psibase::AccountNumber thisContract, std::string_view path)
{
   return std::tuple{thisContract, web_content_table, path};
}
struct WebContentRow
{
   std::string       path        = {};
   std::string       contentType = {};
   std::vector<char> content     = {};

   auto key(psibase::AccountNumber thisContract) { return webContentKey(thisContract, path); }
};
PSIO_REFLECT(WebContentRow, path, contentType, content)

struct Block
{
   std::shared_ptr<psibase::Block> block;

   const auto& header() const { return block->header; }
   const auto& transactions() const { return block->transactions; }
};
PSIO_REFLECT(Block, method(header), method(transactions))

using BlockEdge       = psio::Edge<Block>;
using BlockConnection = psio::Connection<Block>;
PSIO_REFLECT_GQL_EDGE(BlockEdge)
PSIO_REFLECT_GQL_CONNECTION(BlockConnection)

struct QueryRoot
{
   // TODO: avoid reading full blocks
   auto blocks(const std::optional<psibase::BlockNum>& gt,
               const std::optional<psibase::BlockNum>& ge,
               const std::optional<psibase::BlockNum>& lt,
               const std::optional<psibase::BlockNum>& le,
               std::optional<uint32_t>                 first,
               std::optional<uint32_t>                 last,
               const std::optional<std::string>&       before,
               const std::optional<std::string>&       after) const
   {
      return psibase::makeKVConnection<BlockConnection, psibase::Block>(
          gt, ge, lt, le, first, last, before, after, psibase::DbId::blockLog, psibase::BlockNum(0),
          ~psibase::BlockNum(0), 0,
          [](auto& block) {  //
             return block.header.blockNum;
          },
          [](auto& p) {  //
             return Block{p};
          });
   }  // blocks()
};
PSIO_REFLECT(  //
    QueryRoot,
    method(blocks, gt, ge, lt, le, first, last, before, after))

namespace system_contract
{
   std::optional<psibase::RpcReplyData> explore_sys::serveSys(psibase::RpcRequestData request)
   {
      if (auto result = psibase::serveGraphQL(request, [] { return QueryRoot{}; }))
         return result;

      if (request.method == "GET")
      {
         if (auto content = kvGet<WebContentRow>(webContentKey(getReceiver(), request.target)))
         {
            return psibase::RpcReplyData{
                .contentType = content->contentType,
                .body        = content->content,
            };
         }
      }

      return std::nullopt;
   }  // explore_sys::proxy_sys

   void explore_sys::storeSys(psio::const_view<std::string>       path,
                              psio::const_view<std::string>       contentType,
                              psio::const_view<std::vector<char>> content)
   {
      psibase::check(getSender() == getReceiver(), "wrong sender");

      // TODO
      auto              size = content.size();
      std::vector<char> c(size);
      for (size_t i = 0; i < size; ++i)
         c[i] = content[i];

      // TODO: avoid copies before pack
      WebContentRow row{
          .path        = path,
          .contentType = contentType,
          .content     = std::move(c),
      };
      kvPut(row.key(getReceiver()), row);
   }

}  // namespace system_contract

PSIBASE_DISPATCH(system_contract::explore_sys)
