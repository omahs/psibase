#pragma once

#include <newchain/blob.hpp>
#include <newchain/block.hpp>

#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <chainbase/chainbase.hpp>

#define SHARED_CTOR_MACRO(x, y, field) , field(a)
#define SHARED_CTOR(NAME, ...)                                                                    \
   NAME() = delete;                                                                               \
   template <typename Constructor, typename Allocator>                                            \
   NAME(Constructor&& c, chainbase::allocator<Allocator> a)                                       \
       : id(0) BOOST_PP_SEQ_FOR_EACH(SHARED_CTOR_MACRO, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
   {                                                                                              \
      c(*this);                                                                                   \
   }

namespace newchain
{
   namespace multi_index = boost::multi_index;

   using chainbase::object;
   using chainbase::shared_string;

   constexpr uint64_t default_chain_db_size = 1024 * 1024 * 1024;

   enum tables
   {
      status_table,
      account_table,
      ram_kv_table,
   };

   struct by_id;
   struct by_kv_key;

   template <typename T, typename... Indexes>
   using mic = chainbase::shared_multi_index_container<T, multi_index::indexed_by<Indexes...>>;

   template <typename T>
   using ordered_by_id = multi_index::ordered_unique<  //
       multi_index::tag<by_id>,
       multi_index::key<&T::id>>;

   struct status_object : public object<status_table, status_object>
   {
      CHAINBASE_DEFAULT_CONSTRUCTOR(status_object)

      id_type                   id;
      std::optional<block_info> head;
      bool                      busy = false;
   };
   using status_index = mic<status_object, ordered_by_id<status_object>>;

   // TODO: ram, disk, cpu, net: limits and usage
   // TODO: sequence numbers
   // TODO: vm_type, vm_version, move code to another table indexed by hash
   struct account_object : public object<account_table, account_object>
   {
      SHARED_CTOR(account_object, code)

      id_type       id;
      account_num   auth_contract;
      shared_string code;
      bool          privileged;  // TODO: consider finer granularity
   };
   using account_index = mic<account_object, ordered_by_id<account_object>>;

   // TODO: payer?
   struct ram_kv_object : public object<ram_kv_table, ram_kv_object>
   {
      SHARED_CTOR(ram_kv_object, key, value)

      id_type       id;
      account_num   contract;
      shared_string key;
      shared_string value;
   };
   using ram_kv_index =
       mic<ram_kv_object,
           ordered_by_id<ram_kv_object>,
           multi_index::ordered_unique<
               multi_index::tag<by_kv_key>,
               multi_index::composite_key<
                   ram_kv_object,
                   multi_index::member<ram_kv_object, account_num, &ram_kv_object::contract>,
                   multi_index::member<ram_kv_object, shared_string, &ram_kv_object::key>>,
               multi_index::composite_key_compare<std::less<>, blob_less>>>;

   struct database
   {
      chainbase::database db;

      database(const boost::filesystem::path&            dir,
               chainbase::database::open_flags           write = chainbase::database::read_write,
               uint64_t                                  shared_file_size = default_chain_db_size,
               bool                                      allow_dirty      = false,
               chainbase::pinnable_mapped_file::map_mode db_map_mode =
                   chainbase::pinnable_mapped_file::map_mode::mapped)
          : db{dir, write, shared_file_size, allow_dirty, db_map_mode}
      {
         db.add_index<status_index>();
         db.add_index<account_index>();
         db.add_index<ram_kv_index>();
      }
   };

}  // namespace newchain

CHAINBASE_SET_INDEX_TYPE(newchain::status_object, newchain::status_index)
CHAINBASE_SET_INDEX_TYPE(newchain::account_object, newchain::account_index)
CHAINBASE_SET_INDEX_TYPE(newchain::ram_kv_object, newchain::ram_kv_index)
