#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <chainbase/chainbase.hpp>
#include <clchain/crypto.hpp>
#include <clchain/eden_chain.hpp>
#include <clchain/graphql.hpp>
#include <eden.hpp>
#include <eosio/to_bin.hpp>

using namespace eosio::literals;

extern "C" void __wasm_call_ctors();
[[clang::export_name("initialize")]] void initialize()
{
   __wasm_call_ctors();
}

[[clang::export_name("allocate_memory")]] void* allocate_memory(uint32_t size)
{
   return malloc(size);
}

[[clang::export_name("free_memory")]] void free_memory(void* p)
{
   free(p);
}

template <typename T>
void dump(const T& ind)
{
   printf("%s\n", eosio::format_json(ind).c_str());
}

namespace boost
{
   BOOST_NORETURN void throw_exception(std::exception const& e)
   {
      eosio::detail::assert_or_throw(e.what());
   }
   BOOST_NORETURN void throw_exception(std::exception const& e, boost::source_location const& loc)
   {
      eosio::detail::assert_or_throw(e.what());
   }
}  // namespace boost

struct by_id;
struct by_pk;

template <typename T, typename... Indexes>
using mic = boost::
    multi_index_container<T, boost::multi_index::indexed_by<Indexes...>, chainbase::allocator<T>>;

template <typename T>
using ordered_by_id = boost::multi_index::ordered_unique<  //
    boost::multi_index::tag<by_id>,
    boost::multi_index::member<T, typename T::id_type, &T::id>>;

template <typename T>
using ordered_by_pk = boost::multi_index::ordered_unique<  //
    boost::multi_index::tag<by_pk>,
    boost::multi_index::const_mem_fun<
        T,
        eosio::remove_cvref_t<typename eosio::member_fn<decltype(&T::pk)>::return_type>,
        &T::pk>>;

auto available_pk(const auto& table, const auto& first)
    -> eosio::remove_cvref_t<decltype(table.begin()->pk())>
{
   auto& idx = table.template get<by_pk>();
   if (idx.empty())
      return first;
   return (--idx.end())->pk() + 1;
}

enum tables
{
   status_table,
   induction_table,
};

struct status
{
   bool initialized = false;
   std::string community;
   eosio::symbol communitySymbol;
   eosio::asset minimumDonation;
   std::vector<eosio::name> initialMembers;
   std::string genesisVideo;
   eden::atomicassets::attribute_map collectionAttributes;
   eosio::asset auctionStartingBid;
   uint32_t auctionDuration;
   std::string memo;
};
EOSIO_REFLECT(status,
              initialized,
              community,
              communitySymbol,
              minimumDonation,
              initialMembers,
              genesisVideo,
              collectionAttributes,
              auctionStartingBid,
              auctionDuration,
              memo)

struct status_object : public chainbase::object<status_table, status_object>
{
   CHAINBASE_DEFAULT_CONSTRUCTOR(status_object)

   id_type id;
   status status;
};
using status_index = mic<status_object, ordered_by_id<status_object>>;

struct induction
{
   uint64_t id = 0;
   eosio::name inviter;
   eosio::name invitee;
   std::vector<eosio::name> witnesses;
   eden::new_member_profile profile;
   std::string video;
};
EOSIO_REFLECT(induction, id, inviter, invitee, witnesses, profile, video)

struct induction_object : public chainbase::object<induction_table, induction_object>
{
   CHAINBASE_DEFAULT_CONSTRUCTOR(induction_object)

   id_type id;
   induction induction;

   uint64_t pk() const { return induction.id; }
};
using induction_index =
    mic<induction_object, ordered_by_id<induction_object>, ordered_by_pk<induction_object>>;

struct database
{
   chainbase::database db;
   chainbase::generic_index<status_index> status;
   chainbase::generic_index<induction_index> inductions;

   database()
   {
      db.add_index(status);
      db.add_index(inductions);
   }
};
database db;

template <typename Tag, typename Table, typename Key, typename F>
void add_or_modify(Table& table, const Key& key, F&& f)
{
   auto& idx = table.template get<Tag>();
   auto it = idx.find(key);
   if (it != idx.end())
      table.modify(*it, [&](auto& obj) { return f(false, obj); });
   else
      table.emplace([&](auto& obj) { return f(true, obj); });
}

template <typename Tag, typename Table, typename Key, typename F>
void add_or_replace(Table& table, const Key& key, F&& f)
{
   auto& idx = table.template get<Tag>();
   auto it = idx.find(key);
   if (it != idx.end())
      table.remove(*it);
   table.emplace(f);
}

template <typename Tag, typename Table, typename Key, typename F>
void modify(Table& table, const Key& key, F&& f)
{
   auto& idx = table.template get<Tag>();
   auto it = idx.find(key);
   eosio::check(it != idx.end(), "missing record");
   table.modify(*it, [&](auto& obj) { return f(obj); });
}

template <typename Tag, typename Table, typename Key>
void remove_if_exists(Table& table, const Key& key)
{
   auto& idx = table.template get<Tag>();
   auto it = idx.find(key);
   if (it != idx.end())
      table.remove(*it);
}

const auto& get_status()
{
   auto& idx = db.status.get<by_id>();
   eosio::check(idx.size() == 1, "missing genesis action");
   return *idx.begin();
}

void add_genesis_member(const status& status, eosio::name member)
{
   db.inductions.emplace([&](auto& obj) {
      obj.induction.id = available_pk(db.inductions, 1);
      obj.induction.inviter = "genesis.eden"_n;  // TODO
      obj.induction.invitee = member;
      for (auto witness : status.initialMembers)
         if (witness != member)
            obj.induction.witnesses.push_back(witness);
   });
}

void genesis(std::string community,
             eosio::symbol community_symbol,
             eosio::asset minimum_donation,
             std::vector<eosio::name> initial_members,
             std::string genesis_video,
             eden::atomicassets::attribute_map collection_attributes,
             eosio::asset auction_starting_bid,
             uint32_t auction_duration,
             std::string memo)
{
   auto& idx = db.status.get<by_id>();
   eosio::check(idx.empty(), "duplicate genesis action");
   db.status.emplace([&](auto& obj) {
      obj.status.community = std::move(community);
      obj.status.communitySymbol = std::move(community_symbol);
      obj.status.minimumDonation = std::move(minimum_donation);
      obj.status.initialMembers = std::move(initial_members);
      obj.status.genesisVideo = std::move(genesis_video);
      obj.status.collectionAttributes = std::move(collection_attributes);
      obj.status.auctionStartingBid = std::move(auction_starting_bid);
      obj.status.auctionDuration = std::move(auction_duration);
      obj.status.memo = std::move(memo);
      for (auto& member : obj.status.initialMembers)
         add_genesis_member(obj.status, member);
   });
}

void addtogenesis(eosio::name new_genesis_member)
{
   auto& status = get_status();
   db.status.modify(get_status(),
                    [&](auto& obj) { obj.status.initialMembers.push_back(new_genesis_member); });
   for (auto& obj : db.inductions)
      db.inductions.modify(
          obj, [&](auto& obj) { obj.induction.witnesses.push_back(new_genesis_member); });
   add_genesis_member(status.status, new_genesis_member);
}

void inductinit(uint64_t id,
                eosio::name inviter,
                eosio::name invitee,
                std::vector<eosio::name> witnesses)
{
   // TODO: expire records
   const auto& status = get_status();
   if (!status.status.initialized)
      db.status.modify(status, [&](auto& obj) { obj.status.initialized = true; });
   add_or_replace<by_pk>(db.inductions, id, [&](auto& obj) {
      obj.induction.id = id;
      obj.induction.inviter = inviter;
      obj.induction.invitee = invitee;
      obj.induction.witnesses = witnesses;
   });
}

void inductprofil(uint64_t id, eden::new_member_profile profile)
{
   modify<by_pk>(db.inductions, id, [&](auto& obj) { obj.induction.profile = profile; });
}

void inductvideo(eosio::name account, uint64_t id, std::string video)
{
   modify<by_pk>(db.inductions, id, [&](auto& obj) { obj.induction.video = video; });
}

void inductcancel(eosio::name account, uint64_t id)
{
   remove_if_exists<by_pk>(db.inductions, id);
}

void inductdonate(eosio::name payer, uint64_t id, eosio::asset quantity) {}

template <typename... Args>
void call(void (*f)(Args...), const std::vector<char>& data)
{
   std::tuple<eosio::remove_cvref_t<Args>...> t;
   eosio::input_stream s(data);
   // TODO: prevent abort, indicate what failed
   eosio::from_bin(t, s);
   std::apply([f](auto&&... args) { f(std::move(args)...); }, t);
}

// TODO: configurable contract accounts
void filter_block(const eden_chain::eosio_block& block)
{
   for (auto& trx : block.transactions)
   {
      for (auto& action : trx.actions)
      {
         if (action.firstReceiver == "genesis.eden"_n)
         {
            if (action.name == "genesis"_n)
               call(genesis, action.hexData.data);
            else if (action.name == "addtogenesis"_n)
               call(addtogenesis, action.hexData.data);
            else if (action.name == "inductinit"_n)
               call(inductinit, action.hexData.data);
            else if (action.name == "inductprofil"_n)
               call(inductprofil, action.hexData.data);
            else if (action.name == "inductvideo"_n)
               call(inductvideo, action.hexData.data);
            else if (action.name == "inductcancel"_n)
               call(inductcancel, action.hexData.data);
            else if (action.name == "inductdonate"_n)
               call(inductdonate, action.hexData.data);
         }
      }
   }
}

eden_chain::block_log block_log;

// TODO: prevent from_json from aborting
[[clang::export_name("add_eosio_blocks_json")]] void add_eosio_blocks_json(const char* json,
                                                                           uint32_t size)
{
   std::string str(json, size);
   eosio::json_token_stream s(str.data());
   std::vector<eden_chain::eosio_block> eosio_blocks;
   eosio::from_json(eosio_blocks, s);
   for (auto& eosio_block : eosio_blocks)
   {
      eden_chain::block eden_block;
      eden_block.eosio_block = std::move(eosio_block);
      auto* prev = block_log.block_before_eosio_num(eden_block.eosio_block.num);
      if (prev)
      {
         eden_block.num = prev->block.num + 1;
         eden_block.previous = prev->id;
      }
      else
         eden_block.num = 1;
      auto bin = eosio::convert_to_bin(eden_block);
      eden_chain::block_with_id bi;
      bi.block = std::move(eden_block);
      bi.id = clchain::sha256(bin.data(), bin.size());
      auto status = block_log.add_block(bi);
      // TODO: notify chainbase of forks & irreversible
      // TODO: filter_block
      // printf("%s block %u %s\n", block_log.status_str[status], bi.block.eosio_block.num,
      //        to_string(bi.block.eosio_block.id).c_str());
   }
   // printf("%d blocks processed, %d blocks now in log\n", (int)eosio_blocks.size(),
   //        (int)block_log.blocks.size());
}

// TODO: remove
[[clang::export_name("scan_blocks")]] void scan_blocks()
{
   printf("scan_blocks...\n");
   for (auto& block : block_log.blocks)
      filter_block(block->block.eosio_block);
   printf("%d blocks processed\n", (int)block_log.blocks.size());
}

struct Query
{
   eden_chain::block_log* blockLog;  // TODO: fix ref support
};
EOSIO_REFLECT(Query, blockLog)

auto schema = clchain::get_gql_schema<Query>();
[[clang::export_name("get_schema_size")]] uint32_t get_schema_size()
{
   return schema.size();
}
[[clang::export_name("get_schema")]] const char* get_schema()
{
   return schema.c_str();
}

std::string result;
[[clang::export_name("exec_query")]] void exec_query(const char* query, uint32_t size)
{
   Query root{&block_log};
   result = clchain::gql_query(root, {query, size});
}
[[clang::export_name("get_result_size")]] uint32_t get_result_size()
{
   return result.size();
}
[[clang::export_name("get_result")]] const char* get_result()
{
   return result.c_str();
}
