#include <psibase/db.hpp>

#include <boost/filesystem/operations.hpp>
#include <mdbx.h++>

namespace psibase
{
   static std::unique_ptr<mdbx::env_managed> construct_env(const boost::filesystem::path& dir,
                                                           unsigned max_maps = 0)
   {
      mdbx::env_managed::create_parameters  cp;
      mdbx::env_managed::operate_parameters op;
      op.max_maps                          = max_maps;
      op.options.nested_write_transactions = true;
      op.options.orphan_read_transactions  = false;

      boost::filesystem::create_directories(dir);
      return std::make_unique<mdbx::env_managed>(dir.native(), cp, op);
   }

   static mdbx::map_handle construct_kv_map(mdbx::env_managed& env, const char* name)
   {
      mdbx::map_handle kv_map;
      auto             trx = env.start_write();
      kv_map               = trx.create_map(name);
      trx.commit();
      return kv_map;
   }

   struct shared_database_impl
   {
      const std::unique_ptr<mdbx::env_managed> state_env;
      const std::unique_ptr<mdbx::env_managed> subjective_env;
      const std::unique_ptr<mdbx::env_managed> write_only_env;
      const std::unique_ptr<mdbx::env_managed> block_log_env;
      const mdbx::map_handle                   contract_map;
      const mdbx::map_handle                   native_constrained_map;
      const mdbx::map_handle                   native_unconstrained_map;
      const mdbx::map_handle                   subjective_map;
      const mdbx::map_handle                   write_only_map;
      const mdbx::map_handle                   event_map;
      const mdbx::map_handle                   ui_event_map;
      const mdbx::map_handle                   block_log_map;

      shared_database_impl(const boost::filesystem::path& dir)
          : state_env{construct_env(dir / "state", 3)},
            subjective_env{construct_env(dir / "subjective")},
            write_only_env{construct_env(dir / "write_only", 3)},
            block_log_env{construct_env(dir / "block_log")},
            contract_map{construct_kv_map(*state_env, "contract")},
            native_constrained_map{construct_kv_map(*state_env, "native_constrained")},
            native_unconstrained_map{construct_kv_map(*state_env, "native_unconstrained")},
            subjective_map{construct_kv_map(*subjective_env, nullptr)},
            write_only_map{construct_kv_map(*write_only_env, "write_only")},
            event_map{construct_kv_map(*write_only_env, "event")},
            ui_event_map{construct_kv_map(*write_only_env, "ui_event")},
            block_log_map{construct_kv_map(*block_log_env, nullptr)}
      {
      }

      mdbx::map_handle get_map(kv_map map)
      {
         if (map == kv_map::contract)
            return contract_map;
         if (map == kv_map::native_constrained)
            return native_constrained_map;
         if (map == kv_map::native_unconstrained)
            return native_unconstrained_map;
         if (map == kv_map::subjective)
            return subjective_map;
         if (map == kv_map::write_only)
            return write_only_map;
         if (map == kv_map::event)
            return write_only_map;
         if (map == kv_map::ui_event)
            return write_only_map;
         if (map == kv_map::block_log)
            return block_log_map;
         throw std::runtime_error("unknown kv_map");
      }
   };

   shared_database::shared_database(const boost::filesystem::path& dir)
       : impl{std::make_shared<shared_database_impl>(dir)}
   {
   }

   struct database_impl
   {
      shared_database                  shared;
      std::vector<mdbx::txn_managed>   transactions;
      std::vector<mdbx::txn_managed>   write_only_transactions;
      std::optional<mdbx::txn_managed> subjective_transaction;
      std::optional<mdbx::txn_managed> block_log_transaction;
      mdbx::cursor_managed             cursor;

      bool transactions_ok()
      {
         // This mismatch can occur if start_*() or commit() throw
         return transactions.size() == write_only_transactions.size() &&
                transactions.empty() == !subjective_transaction.has_value() &&
                transactions.empty() == !block_log_transaction.has_value();
      }

      bool transactions_available() { return transactions_ok() && !transactions.empty(); }

      mdbx::txn_managed& get_trx(kv_map map)
      {
         check(transactions_available(), "no active database transactions");
         if (map == kv_map::subjective)
            return *subjective_transaction;
         else if (map == kv_map::block_log)
            return *block_log_transaction;
         else if (map == kv_map::write_only)
            return write_only_transactions.back();
         else if (map == kv_map::event)
            return write_only_transactions.back();
         else if (map == kv_map::ui_event)
            return write_only_transactions.back();
         else
            return transactions.back();
      }
   };

   database::database(shared_database shared)
       : impl{std::make_unique<database_impl>(database_impl{std::move(shared)})}
   {
   }

   database::~database() {}

   database::session database::start_read()
   {
      check(impl->transactions_ok(), "database transactions mismatch");
      impl->transactions.push_back(impl->shared.impl->state_env->start_read());
      impl->write_only_transactions.push_back(impl->shared.impl->write_only_env->start_read());
      if (!impl->subjective_transaction)
         impl->subjective_transaction.emplace(impl->shared.impl->subjective_env->start_read());
      if (!impl->block_log_transaction)
         impl->block_log_transaction.emplace(impl->shared.impl->block_log_env->start_read());
      return {this};
   }

   database::session database::start_write()
   {
      check(impl->transactions_ok(), "database transactions mismatch");
      if (impl->transactions.empty())
      {
         impl->transactions.push_back(impl->shared.impl->state_env->start_write());
         impl->write_only_transactions.push_back(impl->shared.impl->write_only_env->start_write());
         impl->subjective_transaction.emplace(impl->shared.impl->subjective_env->start_write());
         impl->block_log_transaction.emplace(impl->shared.impl->block_log_env->start_write());
      }
      else
      {
         impl->transactions.push_back(impl->transactions.back().start_nested());
         impl->write_only_transactions.push_back(
             impl->write_only_transactions.back().start_nested());
      }
      return {this};
   }

   // TODO: consider flushing disk after each commit when
   //       transactions.size() falls to 0.
   void database::commit(database::session&)
   {
      check(impl->transactions_available(), "no active database transactions during commit");
      if (impl->transactions.size() == 1)
      {
         // Commit first to prevent consensus state from getting ahead of block
         // log. If the consensus state fails to commit, then replaying the blocks
         // can catch the consensus state back up on restart.
         impl->block_log_transaction->commit();
         impl->block_log_transaction.reset();
      }

      // Commit write-only before consensus state to prevent consensus from
      // getting ahead. If write-only commits, but consensus state fails to
      // commit, then replaying the block on restart will fix up the problem.
      impl->write_only_transactions.back().commit();
      impl->write_only_transactions.pop_back();

      // Consensus state.
      impl->transactions.back().commit();
      impl->transactions.pop_back();

      // It's no big deal if this fails to commit
      if (impl->transactions.empty())
      {
         impl->subjective_transaction->commit();
         impl->subjective_transaction.reset();
      }
   }

   void database::abort(database::session&)
   {
      if (!impl->transactions_available())
         return;
      impl->transactions.pop_back();
      impl->write_only_transactions.pop_back();
      if (impl->transactions.empty() && impl->subjective_transaction.has_value())
      {
         // The subjective database has no undo stack and survives aborts; this
         // helps subjective contracts perform subjective mitigation.
         try
         {
            impl->subjective_transaction->commit();
         }
         catch (...)
         {
            // Ignore failures since this may be called from ~session()
            // and it's not a major problem if subjective data fails to
            // get committed.

            // TODO: log the failure since this may indicate a disk-space issue
            //       or a larger problem
         }
         impl->subjective_transaction.reset();
      }
      if (impl->transactions.empty())
         impl->block_log_transaction.reset();
   }

   void database::kvPutRaw(kv_map map, psio::input_stream key, psio::input_stream value)
   {
      impl->get_trx(map).upsert(impl->shared.impl->get_map(map), {key.pos, key.remaining()},
                                {value.pos, value.remaining()});
   }

   void database::kvRemoveRaw(kv_map map, psio::input_stream key)
   {
      impl->get_trx(map).erase(impl->shared.impl->get_map(map), {key.pos, key.remaining()});
   }

   std::optional<psio::input_stream> database::kvGetRaw(kv_map map, psio::input_stream key)
   {
      mdbx::slice k{key.pos, key.remaining()};
      mdbx::slice v;
      auto stat = ::mdbx_get(impl->get_trx(map), impl->shared.impl->get_map(map).dbi, &k, &v);
      if (stat == MDBX_NOTFOUND)
         return std::nullopt;
      mdbx::error::success_or_throw(stat);
      return psio::input_stream{(const char*)v.data(), v.size()};
   }

   std::optional<database::kv_result> database::kvGreaterEqualRaw(kv_map             map,
                                                                  psio::input_stream key,
                                                                  size_t             match_key_size)
   {
      mdbx::slice k{key.pos, key.remaining()};
      impl->cursor.bind(impl->get_trx(map), impl->shared.impl->get_map(map).dbi);
      auto result = impl->cursor.lower_bound(k, false);
      if (!result)
         return std::nullopt;
      if (result.key.size() < match_key_size || memcmp(result.key.data(), key.pos, match_key_size))
         return std::nullopt;
      return database::kv_result{
          psio::input_stream{(const char*)result.key.data(), result.key.size()},
          psio::input_stream{(const char*)result.value.data(), result.value.size()},
      };
   }

   std::optional<database::kv_result> database::kvLessThanRaw(kv_map             map,
                                                              psio::input_stream key,
                                                              size_t             match_key_size)
   {
      mdbx::slice k{key.pos, key.remaining()};
      impl->cursor.bind(impl->get_trx(map), impl->shared.impl->get_map(map).dbi);
      auto result = impl->cursor.lower_bound(k, false);
      if (!result)
         result = impl->cursor.to_last(false);
      else
         result = impl->cursor.to_previous(false);
      if (!result)
         return std::nullopt;
      if (result.key.size() < match_key_size || memcmp(result.key.data(), key.pos, match_key_size))
         return std::nullopt;
      return database::kv_result{
          psio::input_stream{(const char*)result.key.data(), result.key.size()},
          psio::input_stream{(const char*)result.value.data(), result.value.size()},
      };
   }

   std::optional<database::kv_result> database::kvMaxRaw(kv_map map, psio::input_stream key)
   {
      std::vector<unsigned char> next(reinterpret_cast<const unsigned char*>(key.pos),
                                      reinterpret_cast<const unsigned char*>(key.end));
      while (!next.empty())
      {
         if (++next.back())
            break;
         next.pop_back();
      }
      std::optional<mdbx::cursor::move_result> result;
      if (next.empty())
      {
         result = impl->cursor.to_last(false);
      }
      else
      {
         mdbx::slice k{next.data(), next.size()};
         result = impl->cursor.lower_bound(k, false);
         if (!*result)
            result = impl->cursor.to_last(false);
         else
            result = impl->cursor.to_previous(false);
      }
      if (!*result)
         return std::nullopt;
      if (result->key.size() < key.remaining() ||
          memcmp(result->key.data(), key.pos, key.remaining()))
         return std::nullopt;
      return database::kv_result{
          psio::input_stream{(const char*)result->key.data(), result->key.size()},
          psio::input_stream{(const char*)result->value.data(), result->value.size()},
      };
   }

}  // namespace psibase
