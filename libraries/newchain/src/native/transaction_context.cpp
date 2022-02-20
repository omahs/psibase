#include <newchain/transaction_context.hpp>

#include <eosio/from_bin.hpp>
#include <newchain/action_context.hpp>
#include <newchain/from_bin.hpp>

namespace newchain
{
   transaction_context::transaction_context(newchain::block_context&              block_context,
                                            const signed_transaction&             trx,
                                            const std::vector<eosio::public_key>& recovered_keys,
                                            newchain::transaction_trace&          transaction_trace,
                                            bool                                  enable_undo)
       : block_context{block_context},
         trx{trx},
         recovered_keys{recovered_keys},
         transaction_trace{transaction_trace}
   {
      if (enable_undo)
      {
         nested_kv_trx = block_context.kv_trx.start_nested();
         kv_trx        = &nested_kv_trx;
      }
      else
      {
         kv_trx = &block_context.kv_trx;
      }
   }

   static void exec_genesis_action(transaction_context& self, const action& act);
   static void exec_auth_action(transaction_context& self, const action& act);

   // TODO: limit execution time when not playing a produced block
   // TODO: charge net and cpu
   // TODO: check ref_block_num, ref_block_prefix
   // TODO: check max_net_usage_words, max_cpu_usage_ms
   void transaction_context::exec_all_actions()
   {
      eosio::check(block_context.current.time < trx.expiration, "transaction expired");
      eosio::check(!trx.actions.empty(), "transaction has no actions");

      // Prepare for execution
      auto& db     = block_context.db;
      auto  status = db.get_kv_or_default<status_row>(*kv_trx, status_key());
      block_context.system_context.set_num_memories(status.num_execution_memories);

      for (auto& act : trx.actions)
      {
         if (block_context.need_genesis_action)
         {
            exec_genesis_action(*this, act);
            block_context.need_genesis_action = false;
         }
         else
         {
            exec_auth_action(*this, act);
         }
      }

      // If the transaction adjusted num_execution_memories too big for this node, then attempt
      // to reject the transaction. It is possible for the node to go down in flames instead.
      status = db.get_kv_or_default<status_row>(*kv_trx, status_key());
      block_context.system_context.set_num_memories(status.num_execution_memories);
   }

   static void exec_genesis_action(transaction_context& self, const action& act)
   {
      auto& atrace = self.transaction_trace.action_traces.emplace_back();
      atrace.act   = act;
      try
      {
         auto& db   = self.block_context.db;
         auto  data = unpack_all<genesis_action_data>({act.raw_data.data(), act.raw_data.size()},
                                                     "extra data in genesis payload");
         account_row account{
             .num           = 1,
             .auth_contract = 1,
             .privileged    = true,
         };
         db.set_kv(*self.kv_trx, account.key(), account);
         set_code(db, *self.kv_trx, 1, data.vm_type, data.vm_version,
                  {data.code.data(), data.code.size()});
      }
      catch (const std::exception& e)
      {
         atrace.error = e.what();
         throw;
      }
   }

   static void exec_auth_action(transaction_context& self, const action& act)
   {
      auto& db     = self.block_context.db;
      auto  sender = db.get_kv<account_row>(*self.kv_trx, account_key(act.sender));
      eosio::check(sender.has_value(), "unknown sender account");
      auto& atrace         = self.transaction_trace.action_traces.emplace_back();
      atrace.act           = act;
      atrace.auth_contract = sender->auth_contract;
      action_context ac{self, act, self.transaction_trace.action_traces.back()};
      ac.exec(true);
   }

   void transaction_context::exec_called_action(const action& act, action_trace& atrace)
   {
      atrace.act = act;
      action_context ac{*this, act, atrace};
      ac.exec();
   }

   execution_context& transaction_context::get_execution_context(account_num contract)
   {
      auto it = execution_contexts.find(contract);
      if (it != execution_contexts.end())
         return it->second;
      eosio::check(
          execution_contexts.size() < block_context.system_context.execution_memories.size(),
          "exceeded maximum number of running contracts");
      auto& memory = block_context.system_context.execution_memories[execution_contexts.size()];
      return execution_contexts.insert({contract, execution_context{*this, memory, contract}})
          .first->second;
   }
}  // namespace newchain
