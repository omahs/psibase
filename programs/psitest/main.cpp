#include <boost/filesystem/operations.hpp>
#include <debug_eos_vm/debug_eos_vm.hpp>
#include <psibase/ActionContext.hpp>
#include <psibase/NativeFunctions.hpp>
#include <psibase/Prover.hpp>
#include <psibase/Watchdog.hpp>
#include <psibase/log.hpp>
#include <psibase/version.hpp>
#include <psio/to_bin.hpp>
#include <psio/to_json.hpp>

#include <stdio.h>
#include <bitset>
#include <chrono>
#include <optional>

#include <sys/stat.h>

using namespace std::literals;

using eosio::vm::span;
using psibase::WatchdogManager;
using psio::convert_to_bin;

struct callbacks;
using rhf_t = eosio::vm::registered_host_functions<callbacks>;

using cl_flags_t = std::bitset<16>;
enum cl_flags
{
   timing
};

void backtrace();

struct vm_options
{
   static constexpr bool          enable_simd          = true;
   static constexpr bool          enable_bulk_memory   = true;
   static constexpr std::uint32_t max_func_local_bytes = 1024 * 1024;
   static constexpr std::uint32_t max_stack_bytes      = 1024 * 1024;
};

#ifdef __x86_64__
template <>
void eosio::vm::machine_code_writer<eosio::vm::jit_execution_context<callbacks, true>,
                                    true>::on_unreachable()
{
   backtrace();
   eosio::vm::throw_<wasm_interpreter_exception>("unreachable");
}
#endif

using backend_t = eosio::vm::backend<  //
    rhf_t,
#ifdef __x86_64__
    eosio::vm::jit_profile,
#else
    eosio::vm::interpreter,
#endif
    vm_options,
    debug_eos_vm::debug_instr_map>;

inline constexpr int max_backtrace_frames = 512;

inline constexpr int      block_interval_ms   = 500;
inline constexpr int      block_interval_us   = block_interval_ms * 1000;
inline constexpr uint32_t billed_cpu_time_use = 2000;

inline constexpr int32_t polyfill_root_dir_fd = 3;

inline constexpr uint16_t wasi_errno_badf  = 8;
inline constexpr uint16_t wasi_errno_fault = 21;
inline constexpr uint16_t wasi_errno_inval = 28;
inline constexpr uint16_t wasi_errno_io    = 29;
inline constexpr uint16_t wasi_errno_noent = 44;

inline constexpr uint8_t wasi_filetype_character_device = 2;
inline constexpr uint8_t wasi_filetype_directory        = 3;
inline constexpr uint8_t wasi_filetype_regular_file     = 4;

inline constexpr uint64_t wasi_rights_fd_read  = 2;
inline constexpr uint64_t wasi_rights_fd_write = 64;

inline constexpr uint16_t wasi_oflags_creat     = 1;
inline constexpr uint16_t wasi_oflags_directory = 2;
inline constexpr uint16_t wasi_oflags_excl      = 4;
inline constexpr uint16_t wasi_oflags_trunc     = 8;

inline constexpr uint16_t wasi_fdflags_append   = 1;
inline constexpr uint16_t wasi_fdflags_dsync    = 2;
inline constexpr uint16_t wasi_fdflags_nonblock = 4;
inline constexpr uint16_t wasi_fdflags_rsync    = 8;
inline constexpr uint16_t wasi_fdflags_sync     = 1;

struct assert_exception : std::exception
{
   std::string msg;

   assert_exception(std::string&& msg) : msg(std::move(msg)) {}

   const char* what() const noexcept override { return msg.c_str(); }
};

struct NullProver : psibase::Prover
{
   std::vector<char> prove(std::span<const char>, const psibase::Claim&) const { return {}; }
   bool              remove(const psibase::Claim&) { return false; }
   void              get(std::vector<psibase::Claim>&) const {}
   void              get(std::vector<psibase::ClaimKey>&) const {}
};

struct file;
struct test_chain;

struct state
{
   const char*                              wasm;
   dwarf::info&                             dwarf_info;
   eosio::vm::wasm_allocator&               wa;
   backend_t&                               backend;
   std::vector<std::string>                 args;
   cl_flags_t                               additional_args;
   std::vector<file>                        files;
   std::vector<std::unique_ptr<test_chain>> chains;
   std::shared_ptr<WatchdogManager>         watchdogManager = std::make_shared<WatchdogManager>();
   std::optional<uint32_t>                  selected_chain_index;
   std::vector<char>                        result_key;
   std::vector<char>                        result_value;
};

template <typename T>
using wasm_ptr = eosio::vm::argument_proxy<T*>;

struct test_chain;

struct test_chain_ref
{
   test_chain* chain = {};

   test_chain_ref() = default;
   test_chain_ref(test_chain&);
   test_chain_ref(const test_chain_ref&);
   ~test_chain_ref();

   test_chain_ref& operator=(const test_chain_ref&);
};

struct test_chain
{
   ::state&                                     state;
   std::set<test_chain_ref*>                    refs;
   boost::filesystem::path                      dir;
   psibase::SharedDatabase                      db;
   psibase::WriterPtr                           writer;
   std::unique_ptr<psibase::SystemContext>      sys;
   std::shared_ptr<const psibase::Revision>     revisionAtBlockStart;
   std::unique_ptr<psibase::BlockContext>       blockContext;
   std::unique_ptr<psibase::TransactionTrace>   nativeFunctionsTrace;
   std::unique_ptr<psibase::TransactionContext> nativeFunctionsTransactionContext;
   std::unique_ptr<psibase::ActionContext>      nativeFunctionsActionContext;
   std::unique_ptr<psibase::NativeFunctions>    nativeFunctions;
   std::string                                  name = "testchain";
   psibase::loggers::common_logger              logger;

   std::chrono::system_clock::time_point getTimestamp()
   {
      if (blockContext)
      {
         return std::chrono::system_clock::time_point{
             std::chrono::seconds{blockContext->current.header.time.seconds}};
      }
      else
      {
         return std::chrono::system_clock::now();
      }
   }
   const std::string& getName() { return name; }

   test_chain(::state& state,
              uint64_t hot_bytes,
              uint64_t warm_bytes,
              uint64_t cool_bytes,
              uint64_t cold_bytes)
       : state{state}
   {
      dir    = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      db     = {dir, hot_bytes, warm_bytes, cool_bytes, cold_bytes};
      writer = db.createWriter();
      sys    = std::make_unique<psibase::SystemContext>(
          psibase::SystemContext{db, {128}, {}, state.watchdogManager});
   }

   test_chain(const test_chain&)            = delete;
   test_chain& operator=(const test_chain&) = delete;

   ~test_chain()
   {
      for (auto* ref : refs)
         ref->chain = nullptr;
      nativeFunctions.reset();
      nativeFunctionsActionContext.reset();
      nativeFunctionsTransactionContext.reset();
      blockContext.reset();
      revisionAtBlockStart.reset();
      sys.reset();
      writer = {};
      db     = {};
      boost::filesystem::remove_all(dir);
   }

   // TODO: Support sub-second block times
   void startBlock(std::optional<psibase::TimePointSec> time = std::nullopt)
   {
      // TODO: undo control
      finishBlock();
      revisionAtBlockStart = db.getHead();
      blockContext =
          std::make_unique<psibase::BlockContext>(*sys, revisionAtBlockStart, writer, true);

      auto producer = psibase::AccountNumber("firstproducer");
      auto status =
          blockContext->db.kvGet<psibase::StatusRow>(psibase::StatusRow::db, psibase::statusKey());
      if (status.has_value())
      {
         if (status->nextConsensus)
         {
            std::visit(
                [&](const auto& c)
                {
                   if (!c.producers.empty())
                   {
                      producer = c.producers.front().name;
                   }
                },
                std::get<0>(*status->nextConsensus));
         }
         if (!status->nextConsensus ||
             status->current.commitNum < std::get<1>(*status->nextConsensus))
         {
            std::visit(
                [&](const auto& c)
                {
                   if (!c.producers.empty())
                   {
                      producer = c.producers.front().name;
                   }
                },
                status->consensus);
         }
      }

      blockContext->start(time, producer);
      blockContext->callStartBlock();
   }

   void start_if_needed()
   {
      if (!blockContext)
         startBlock();
   }

   void finishBlock()
   {
      if (blockContext)
      {
         BOOST_LOG_SCOPED_THREAD_TAG("TimeStamp", getTimestamp());
         BOOST_LOG_SCOPED_THREAD_TAG("Host", getName());
         nativeFunctions.reset();
         nativeFunctionsActionContext.reset();
         nativeFunctionsTransactionContext.reset();
         auto [revision, blockId] = blockContext->writeRevision(NullProver{}, psibase::Claim{});
         db.setHead(*writer, revision);
         db.removeRevisions(*writer, blockId);  // temp rule: head is now irreversible
         PSIBASE_LOG_CONTEXT_BLOCK(logger, blockContext->current.header, blockId);
         PSIBASE_LOG(logger, info) << "Produced block";
         blockContext.reset();
         revisionAtBlockStart.reset();
      }
   }

   psibase::NativeFunctions& native()
   {
      static const psibase::SignedTransaction dummyTransaction;
      static const psibase::Action            dummyAction;
      if (!nativeFunctions)
      {
         if (!blockContext)
            throw std::runtime_error("no block context to read database from");
         nativeFunctionsTrace = std::make_unique<psibase::TransactionTrace>();
         nativeFunctionsTrace->actionTraces.resize(1);
         nativeFunctionsTransactionContext = std::make_unique<psibase::TransactionContext>(
             *blockContext, dummyTransaction, *nativeFunctionsTrace, true, false, true);
         nativeFunctionsActionContext = std::make_unique<psibase::ActionContext>(
             psibase::ActionContext{*nativeFunctionsTransactionContext, dummyAction,
                                    nativeFunctionsTrace->actionTraces[0]});
         nativeFunctions = std::make_unique<psibase::NativeFunctions>(
             psibase::NativeFunctions{blockContext->db,
                                      *nativeFunctionsTransactionContext,
                                      true,
                                      false,
                                      true,
                                      {},
                                      &*nativeFunctionsActionContext});
      }
      return *nativeFunctions;
   }
};  // test_chain

test_chain_ref::test_chain_ref(test_chain& chain)
{
   chain.refs.insert(this);
   this->chain = &chain;
}

test_chain_ref::test_chain_ref(const test_chain_ref& src)
{
   chain = src.chain;
   if (chain)
      chain->refs.insert(this);
}

test_chain_ref::~test_chain_ref()
{
   if (chain)
      chain->refs.erase(this);
}

test_chain_ref& test_chain_ref::operator=(const test_chain_ref& src)
{
   if (chain)
      chain->refs.erase(this);
   chain = nullptr;
   if (src.chain)
      src.chain->refs.insert(this);
   chain = src.chain;
   return *this;
}

struct file
{
   FILE* f    = nullptr;
   bool  owns = false;

   file(FILE* f = nullptr, bool owns = true) : f(f), owns(owns) {}

   file(const file&) = delete;
   file(file&& src) { *this = std::move(src); }

   ~file() { close(); }

   file& operator=(const file&) = delete;

   file& operator=(file&& src)
   {
      close();
      this->f    = src.f;
      this->owns = src.owns;
      src.f      = nullptr;
      src.owns   = false;
      return *this;
   }

   void close()
   {
      if (owns && f)
         fclose(f);
      f    = nullptr;
      owns = false;
   }
};

struct callbacks
{
   ::state&          state;
   static callbacks* single;  // TODO: remove

   callbacks(::state& state) : state{state} { single = this; }
   ~callbacks() { single = nullptr; }

   void backtrace()
   {
      fprintf(stderr, "Generating backtrace...\n");
      void* data[max_backtrace_frames];
      int   count =
          state.backend.get_context().backtrace(data, sizeof(data) / sizeof(data[0]), nullptr);
      for (int i = 0; i < count; ++i)
      {
         auto& di          = state.dwarf_info;
         auto  file_offset = state.backend.get_debug().translate(data[i]);
         if (file_offset == 0xffff'ffff)
            continue;
         const auto* loc = di.get_location(file_offset - di.wasm_code_offset);
         const auto* sub = di.get_subprogram(file_offset - di.wasm_code_offset);
         if (loc)
            fprintf(stderr, "%s:%d", di.files[loc->file_index].c_str(), loc->line);
         else
            fprintf(stderr, "<wasm address 0x%08x>", file_offset - di.wasm_code_offset);
         if (sub)
            fprintf(stderr, ": %s", sub->demangled_name.c_str());
         fprintf(stderr, "\n");
      }
   }

   void check_bounds(void* data, size_t size)
   {
      volatile auto check = *((const char*)data + size - 1);
      eosio::vm::ignore_unused_variable_warning(check);
   }

   std::string span_str(span<const char> str) { return {str.data(), str.size()}; }

   void testerAbort()
   {
      backtrace();
      throw std::runtime_error("called testerAbort");
   }

   void testerExit(int32_t)
   {
      backtrace();
      throw std::runtime_error("called testerExit");
   }

   void abortMessage(span<const char> msg)
   {
      backtrace();
      throw ::assert_exception(span_str(msg));
   }

   void writeConsole(span<const char> str) { std::cout.write(str.data(), str.size()); }

   void testerGetArgCounts(wasm_ptr<uint32_t> argc, wasm_ptr<uint32_t> argv_buf_size)
   {
      uint32_t size = 0;
      for (auto& a : state.args)
         size += a.size() + 1;
      *argc          = state.args.size();
      *argv_buf_size = size;
   };

   // uint8_t** argv, uint8_t* argv_buf
   void testerGetArgs(uint32_t argv, uint32_t argv_buf)
   {
      auto* memory   = state.backend.get_context().linear_memory();
      auto  get_argv = [&]() -> uint32_t& { return *reinterpret_cast<uint32_t*>(memory + argv); };
      auto  get_argv_buf = [&]() -> uint8_t&
      { return *reinterpret_cast<uint8_t*>(memory + argv_buf); };

      for (auto& a : state.args)
      {
         get_argv() = argv_buf;
         argv += 4;
         for (auto ch : a)
         {
            get_argv_buf() = ch;
            ++argv_buf;
         }
         get_argv_buf() = 0;
         ++argv_buf;
      }
   };

   int32_t testerClockTimeGet(uint32_t id, uint64_t precision, wasm_ptr<uint64_t> time)
   {
      std::chrono::nanoseconds result;
      if (id == 0)
      {  // CLOCK_REALTIME
         result = std::chrono::system_clock::now().time_since_epoch();
      }
      else if (id == 1)
      {  // CLOCK_MONOTONIC
         result = std::chrono::steady_clock::now().time_since_epoch();
      }
      else
      {
         return wasi_errno_inval;
      }
      *time = result.count();
      return 0;
   }

   file* get_file(int32_t file_index)
   {
      if (file_index < 0 || static_cast<uint32_t>(file_index) >= state.files.size() ||
          !state.files[file_index].f)
         return nullptr;
      return &state.files[file_index];
   }

   uint32_t testerFdstatGet(int32_t            fd,
                            wasm_ptr<uint8_t>  fs_filetype,
                            wasm_ptr<uint16_t> fs_flags,
                            wasm_ptr<uint64_t> fs_rights_base,
                            wasm_ptr<uint64_t> fs_rights_inheriting)
   {
      if (fd == STDIN_FILENO)
      {
         *fs_filetype          = wasi_filetype_character_device;
         *fs_flags             = 0;
         *fs_rights_base       = wasi_rights_fd_read;
         *fs_rights_inheriting = 0;
         return 0;
      }
      if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
      {
         *fs_filetype          = wasi_filetype_character_device;
         *fs_flags             = wasi_fdflags_append;
         *fs_rights_base       = wasi_rights_fd_write;
         *fs_rights_inheriting = 0;
         return 0;
      }
      if (fd == polyfill_root_dir_fd)
      {
         *fs_filetype          = wasi_filetype_directory;
         *fs_flags             = 0;
         *fs_rights_base       = 0;
         *fs_rights_inheriting = wasi_rights_fd_read | wasi_rights_fd_write;
         return 0;
      }
      if (get_file(fd))
      {
         *fs_filetype          = wasi_filetype_regular_file;
         *fs_flags             = 0;
         *fs_rights_base       = wasi_rights_fd_read | wasi_rights_fd_write;
         *fs_rights_inheriting = 0;
         return 0;
      }
      return wasi_errno_badf;
   }

   uint32_t testerFilestatGet(int32_t fd, span<char> statbuf)
   {
      struct wasi_stat
      {
         uint64_t dev, ino, filetype, nlink, size, atim, mtim, ctim;
      } result = {};
      static_assert(sizeof(wasi_stat) == 64);
      if (statbuf.size() < sizeof(result))
         return wasi_errno_fault;
      if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
      {
         result.filetype = wasi_filetype_character_device;
      }
      else if (fd == polyfill_root_dir_fd)
      {
         result.filetype = wasi_filetype_directory;
      }
      else if (auto file = get_file(fd))
      {
         int         fd = ::fileno(file->f);
         struct stat stat;
         if (::fstat(fd, &stat) < 0)
            return wasi_errno_badf;

         auto to_nsec    = [](const timespec& t) { return t.tv_sec * 1000000000ull + t.tv_nsec; };
         result.dev      = stat.st_dev;
         result.ino      = stat.st_ino;
         result.filetype = wasi_filetype_regular_file;
         result.nlink    = stat.st_nlink;
         result.size     = stat.st_size;
         result.atim     = to_nsec(stat.st_atim);
         result.mtim     = to_nsec(stat.st_mtim);
         result.ctim     = to_nsec(stat.st_ctim);
      }
      else
      {
         return wasi_errno_badf;
      }
      std::memcpy(statbuf.data(), &result, sizeof(result));
      return 0;
   }

   uint32_t testerOpenFile(span<const char>  path,
                           uint32_t          oflags,
                           uint64_t          fs_rights_base,
                           uint32_t          fdflags,
                           wasm_ptr<int32_t> opened_fd)
   {
      if (oflags & wasi_oflags_directory)
         return wasi_errno_inval;
      if (fdflags & wasi_fdflags_nonblock)
         return wasi_errno_inval;

      bool read   = fs_rights_base & wasi_rights_fd_read;
      bool write  = fs_rights_base & wasi_rights_fd_write;
      bool create = oflags & wasi_oflags_creat;
      bool excl   = oflags & wasi_oflags_excl;
      bool trunc  = oflags & wasi_oflags_trunc;
      bool append = fdflags & wasi_fdflags_append;

      // TODO: move away from fopen to allow more flexible options
      const char* mode = nullptr;
      if (read && !create && !excl && !trunc && !append)
      {
         if (write)
            mode = "r+";
         else
            mode = "r";
      }
      else if (write && create && trunc)
      {
         if (read)
         {
            if (excl)
               mode = "w+x";
            else
               mode = "w+";
         }
         else
         {
            if (excl)
               mode = "wx";
            else
               mode = "w";
         }
      }
      else if (write && create && append)
      {
         if (read)
         {
            if (excl)
               mode = "a+x";
            else
               mode = "a+";
         }
         else
         {
            if (excl)
               mode = "ax";
            else
               mode = "a";
         }
      }

      if (!mode)
         return wasi_errno_inval;

      file f = fopen(span_str(path).c_str(), mode);
      if (!f.f)
         return wasi_errno_noent;
      state.files.push_back(std::move(f));
      *opened_fd = state.files.size() - 1;
      return 0;
   }

   uint32_t testerCloseFile(int32_t fd)
   {
      auto* file = get_file(fd);
      if (!file)
         return wasi_errno_badf;
      file->close();
      return 0;
   }

   uint32_t testerWriteFile(int32_t fd, span<const char> content)
   {
      auto* file = get_file(fd);
      if (!file)
         return wasi_errno_badf;
      if (!content.size() || fwrite(content.data(), content.size(), 1, file->f) == 1)
         return 0;
      return wasi_errno_io;
   }

   uint32_t testerReadFile(int32_t fd, span<char> content, wasm_ptr<int32_t> result)
   {
      auto* file = get_file(fd);
      if (!file)
         return wasi_errno_badf;
      *result = fread(content.data(), 1, content.size(), file->f);
      return 0;
   }

   int32_t testerExecute(span<const char> command) { return system(span_str(command).c_str()); }

   test_chain& assert_chain(uint32_t chain, bool require_context = true)
   {
      if (chain >= state.chains.size() || !state.chains[chain])
         throw std::runtime_error("chain does not exist or was destroyed");
      auto& result = *state.chains[chain];
      if (require_context && !result.sys)
         throw std::runtime_error("chain was shut down");
      return result;
   }

   uint32_t testerCreateChain(uint64_t hot_bytes,
                              uint64_t warm_bytes,
                              uint64_t cool_bytes,
                              uint64_t cold_bytes)
   {
      state.chains.push_back(
          std::make_unique<test_chain>(state, hot_bytes, warm_bytes, cool_bytes, cold_bytes));
      if (state.chains.size() == 1)
         state.selected_chain_index = 0;
      return state.chains.size() - 1;
   }

   void testerDestroyChain(uint32_t chain)
   {
      assert_chain(chain, false);
      if (state.selected_chain_index && *state.selected_chain_index == chain)
         state.selected_chain_index.reset();
      state.chains[chain].reset();
      while (!state.chains.empty() && !state.chains.back())
      {
         state.chains.pop_back();
      }
   }

   // TODO: may not be useful anymore; remove?
   void testerShutdownChain(uint32_t chain)
   {
      auto& c = assert_chain(chain);
      c.blockContext.reset();
      c.sys.reset();
      c.db = {};
   }

   uint32_t testerGetChainPath(uint32_t chain, span<char> dest)
   {
      auto& c = assert_chain(chain, false);
      memcpy(dest.data(), c.dir.c_str(), std::min(dest.size(), c.dir.size()));
      return c.dir.size();
   }

   // TODO: Support sub-second block times
   void testerStartBlock(uint32_t chain_index, uint32_t time_seconds)
   {
      assert_chain(chain_index)
          .startBlock(time_seconds ? std::optional{psibase::TimePointSec{time_seconds}}
                                   : std::nullopt);
   }

   void testerFinishBlock(uint32_t chain_index) { assert_chain(chain_index).finishBlock(); }

   uint32_t testerPushTransaction(uint32_t chain_index, span<const char> args_packed)
   {
      auto&              chain     = assert_chain(chain_index);
      psio::input_stream s         = {args_packed.data(), args_packed.size()};
      auto               signedTrx = psio::from_frac<psibase::SignedTransaction>(args_packed);

      chain.start_if_needed();
      BOOST_LOG_SCOPED_THREAD_TAG("TimeStamp", chain.getTimestamp());
      BOOST_LOG_SCOPED_THREAD_TAG("Host", chain.getName());
      psibase::TransactionTrace trace;
      auto                      start_time = std::chrono::steady_clock::now();
      try
      {
         psibase::BlockContext proofBC{*chain.sys, chain.revisionAtBlockStart};
         proofBC.start(chain.blockContext->current.header.time);
         psibase::check(!proofBC.isGenesisBlock || signedTrx.proofs.empty(),
                        "Genesis block may not have proofs");
         for (size_t i = 0; i < signedTrx.proofs.size(); ++i)
            proofBC.verifyProof(signedTrx, trace, i, std::nullopt);

         if (!proofBC.needGenesisAction)
         {
            // checkFirstAuth isn't necessary here, but including it catches
            // the case where an auth service modifies its tables while
            // running in read-only mode.
            //
            // We run the check within blockContext to make it easier for
            // tests to chain transactions which modify auth. There's a
            // cost to this since numExecutionMemories may bounce back
            // and forth.
            auto saveTrace = trace;
            chain.blockContext->checkFirstAuth(signedTrx, trace, std::nullopt);
            trace = std::move(saveTrace);
         }

         chain.blockContext->pushTransaction(std::move(signedTrx), trace, std::nullopt);
      }
      catch (const std::exception& e)
      {
         if (!trace.error)
         {
            trace.error = e.what();
         }
      }

      auto us = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - start_time);

      if (state.additional_args[cl_flags::timing])
      {
         std::cout << "psibase transaction took " << us.count() << " us\n";
      }

      // std::cout << eosio::format_json(trace) << "\n";
      state.result_value = psio::convert_to_frac(trace);
      state.result_key.clear();
      psibase::check(state.result_value.size() <= 0xffff'ffffu, "Transaction trace too large");
      return state.result_value.size();
   }

   void testerSelectChainForDb(uint32_t chain_index)
   {
      assert_chain(chain_index);
      state.selected_chain_index = chain_index;
   }

   psibase::NativeFunctions& native()
   {
      if (!state.selected_chain_index)
         throw std::runtime_error("no chain is selected");
      return assert_chain(*state.selected_chain_index).native();
   }

   void setResult(psibase::NativeFunctions& n)
   {
      state.result_key   = std::move(n.result_key);
      state.result_value = std::move(n.result_value);
   }

   uint32_t getResult(eosio::vm::span<char> dest, uint32_t offset)
   {
      if (offset < state.result_value.size() && dest.size())
         memcpy(dest.data(), state.result_value.data() + offset,
                std::min(state.result_value.size() - offset, dest.size()));
      return state.result_value.size();
   }

   uint32_t getKey(eosio::vm::span<char> dest)
   {
      if (!state.result_key.empty())
         memcpy(dest.data(), state.result_key.data(),
                std::min(state.result_key.size(), dest.size()));
      return state.result_key.size();
   }

   uint32_t kvGet(uint32_t db, eosio::vm::span<const char> key)
   {
      auto& n      = native();
      auto  result = n.kvGet(db, key);
      setResult(n);
      return result;
   }

   uint32_t getSequential(uint32_t db, uint64_t indexNumber)
   {
      auto& n      = native();
      auto  result = n.getSequential(db, indexNumber);
      setResult(n);
      return result;
   }

   uint32_t kvGreaterEqual(uint32_t db, eosio::vm::span<const char> key, uint32_t matchKeySize)
   {
      auto& n      = native();
      auto  result = n.kvGreaterEqual(db, key, matchKeySize);
      setResult(n);
      return result;
   }

   uint32_t kvLessThan(uint32_t db, eosio::vm::span<const char> key, uint32_t matchKeySize)
   {
      auto& n      = native();
      auto  result = native().kvLessThan(db, key, matchKeySize);
      setResult(n);
      return result;
   }

   uint32_t kvMax(uint32_t db, eosio::vm::span<const char> key)
   {
      auto& n      = native();
      auto  result = n.kvMax(db, key);
      setResult(n);
      return result;
   }

   uint32_t kvGetTransactionUsage()
   {
      auto& n      = native();
      auto  result = n.kvGetTransactionUsage();
      setResult(n);
      return result;
   }
};  // callbacks

callbacks* callbacks::single = nullptr;  // TODO: remove

void backtrace()
{
   if (callbacks::single)
      callbacks::single->backtrace();
}

void register_callbacks()
{
   rhf_t::add<&callbacks::testerAbort>("env", "testerAbort");
   rhf_t::add<&callbacks::testerExit>("env", "testerExit");
   rhf_t::add<&callbacks::abortMessage>("env", "abortMessage");
   rhf_t::add<&callbacks::writeConsole>("env", "writeConsole");
   rhf_t::add<&callbacks::testerGetArgCounts>("env", "testerGetArgCounts");
   rhf_t::add<&callbacks::testerGetArgs>("env", "testerGetArgs");
   rhf_t::add<&callbacks::testerClockTimeGet>("env", "testerClockTimeGet");
   rhf_t::add<&callbacks::testerFdstatGet>("env", "testerFdstatGet");
   rhf_t::add<&callbacks::testerFilestatGet>("env", "testerFdFilestatGet");
   rhf_t::add<&callbacks::testerOpenFile>("env", "testerOpenFile");
   rhf_t::add<&callbacks::testerCloseFile>("env", "testerCloseFile");
   rhf_t::add<&callbacks::testerWriteFile>("env", "testerWriteFile");
   rhf_t::add<&callbacks::testerReadFile>("env", "testerReadFile");
   rhf_t::add<&callbacks::testerExecute>("env", "testerExecute");
   rhf_t::add<&callbacks::testerCreateChain>("env", "testerCreateChain");
   rhf_t::add<&callbacks::testerDestroyChain>("env", "testerDestroyChain");
   rhf_t::add<&callbacks::testerShutdownChain>("env", "testerShutdownChain");
   rhf_t::add<&callbacks::testerGetChainPath>("env", "testerGetChainPath");
   rhf_t::add<&callbacks::testerStartBlock>("env", "testerStartBlock");
   rhf_t::add<&callbacks::testerFinishBlock>("env", "testerFinishBlock");
   rhf_t::add<&callbacks::testerPushTransaction>("env", "testerPushTransaction");
   rhf_t::add<&callbacks::testerSelectChainForDb>("env", "testerSelectChainForDb");
   rhf_t::add<&callbacks::getResult>("env", "getResult");
   rhf_t::add<&callbacks::getKey>("env", "getKey");
   rhf_t::add<&callbacks::kvGet>("env", "kvGet");
   rhf_t::add<&callbacks::getSequential>("env", "getSequential");
   rhf_t::add<&callbacks::kvGreaterEqual>("env", "kvGreaterEqual");
   rhf_t::add<&callbacks::kvLessThan>("env", "kvLessThan");
   rhf_t::add<&callbacks::kvMax>("env", "kvMax");
   rhf_t::add<&callbacks::kvGetTransactionUsage>("env", "kvGetTransactionUsage");
}

void fill_substitutions(::state& state, const std::map<std::string, std::string>& substitutions)
{
   for (auto& [a, b] : substitutions)
   {
      std::vector<uint8_t> acode;
      std::vector<uint8_t> bcode;
      try
      {
         acode = eosio::vm::read_wasm(a);
      }
      catch (...)
      {
         std::cerr << "skipping substitution: can not read " << a << "\n";
         continue;
      }
      try
      {
         bcode = eosio::vm::read_wasm(b);
      }
      catch (...)
      {
         std::cerr << "skipping substitution: can not read " << b << "\n";
         continue;
      }
      // TODO
      // auto ahash = fc::sha256::hash((const char*)acode.data(), acode.size());
      // auto bhash = fc::sha256::hash((const char*)bcode.data(), bcode.size());
      // state.cache.substitutions[ahash] = bhash;
      // state.cache.codes[bhash] = std::move(bcode);
   }
}

static void run(const char*                               wasm,
                const std::vector<std::string>&           args,
                const std::map<std::string, std::string>& substitutions,
                const cl_flags_t&                         additional_args)
{
   eosio::vm::wasm_allocator wa;
   auto                      code = eosio::vm::read_wasm(wasm);
   backend_t                 backend(code, nullptr);
   psio::input_stream        s{(const char*)code.data(), code.size()};
   //
   auto dwarf_info = dwarf::get_info_from_wasm(s);
   auto reg        = debug_eos_vm::enable_debug(code, backend, dwarf_info, s);

   ::state state{wasm, dwarf_info, wa, backend, args, additional_args};
   fill_substitutions(state, substitutions);
   callbacks cb{state};
   state.files.emplace_back(stdin, false);
   state.files.emplace_back(stdout, false);
   state.files.emplace_back(stderr, false);
   state.files.emplace_back();  // reserve space for fd 3: root dir
   backend.set_wasm_allocator(&wa);

   rhf_t::resolve(backend.get_module());
   backend.initialize(&cb);
   backend(cb, "env", "_start");
}

struct ConsoleLog
{
   std::string type   = "console";
   std::string filter = "Severity > critical";
   std::string format =
       "[{TimeStamp}] [{Severity}] [{Host}]: {Message}{?: {TransactionId}}{?: {BlockId}: "
       "{BlockHeader}}";
};
PSIO_REFLECT(ConsoleLog, type, filter, format);

struct Loggers
{
   ConsoleLog stderr;
};
PSIO_REFLECT(Loggers, stderr);

const char usage[] = "USAGE: psitest [OPTIONS] file.wasm [args for wasm]...\n";
const char help[]  = R"(
OPTIONS:
      -h, --help

            Show this message

      -V, --version

            Print version information

      -v, --verbose

            Show detailed logging

      --log-filter filter

            Set a filter for logging. This can be used to set a more specific
            log filter than -v.

      -s service.wasm debug.wasm
      --subst service.wasm debug.wasm

            Whenever service.wasm needs to run, substitute debug.wasm in its
            place and enable debugging support. This bypasses size limits and
            other constraints on debug.wasm. psibase still enforces
            constraints on service.wasm. (repeatable)

      --timing

            Show how long transactions take in µs.
)";

int main(int argc, char* argv[])
{
   bool                               show_usage   = false;
   bool                               error        = false;
   bool                               show_version = false;
   int                                verbose      = 0;
   Loggers                            logConfig;
   std::map<std::string, std::string> substitutions;
   cl_flags_t                         additional_args;

   int next_arg = 1;
   while (next_arg < argc && argv[next_arg][0] == '-')
   {
      if (!strcmp(argv[next_arg], "-h") || !strcmp(argv[next_arg], "--help"))
         show_usage = true;
      else if (!strcmp(argv[next_arg], "-V") || !strcmp(argv[next_arg], "--version"))
         show_version = true;
      else if (!strcmp(argv[next_arg], "-v") || !strcmp(argv[next_arg], "--verbose"))
      {
         ++verbose;
      }
      else if (!strcmp(argv[next_arg], "--log-filter"))
      {
         ++next_arg;
         if (next_arg >= argc)
         {
            std::cerr << "Missing argument for " << argv[next_arg - 1] << std::endl;
            error = true;
         }
         else
         {
            verbose                 = 0;
            logConfig.stderr.filter = argv[next_arg];
         }
      }
      else if (!strcmp(argv[next_arg], "--timing"))
      {
         additional_args[cl_flags::timing] = true;
      }
      // TODO
      // else if (!strcmp(argv[next_arg], "-s") || !strcmp(argv[next_arg], "--subst"))
      // {
      //    next_arg += 2;
      //    if (next_arg >= argc)
      //    {
      //       std::cerr << argv[next_arg - 2] << " needs 2 args\n";
      //       error = true;
      //    }
      //    else
      //    {
      //       substitutions[argv[next_arg - 1]] = argv[next_arg];
      //    }
      // }
      else
      {
         std::cerr << "unknown option: " << argv[next_arg] << "\n";
         error = true;
      }
      ++next_arg;
   }
   if (next_arg >= argc)
      error = true;
   if (show_version)
   {
      std::cerr << "psitest " << PSIBASE_VERSION_MAJOR << "." << PSIBASE_VERSION_MINOR << "."
                << PSIBASE_VERSION_PATCH << "\n";
      return 0;
   }
   if (show_usage || error)
   {
      std::cerr << usage;
      if (show_usage)
         std::cerr << help;
      return error;
   }
   if (verbose == 1)
   {
      logConfig.stderr.filter = "Severity >= info";
   }
   else if (verbose >= 2)
   {
      logConfig.stderr.filter = "Severity >= debug";
   }
   try
   {
      psibase::loggers::configure(
          psio::convert_from_json<psibase::loggers::Config>(psio::convert_to_json(logConfig)));
      std::vector<std::string> args{argv + next_arg, argv + argc};
      psibase::ExecutionContext::registerHostFunctions();
      register_callbacks();
      run(argv[next_arg], args, substitutions, additional_args);
      return 0;
   }
   catch (::assert_exception& e)
   {
      std::cerr << "tester wasm asserted: " << e.what() << "\n";
   }
   catch (eosio::vm::exception& e)
   {
      std::cerr << "vm::exception: " << e.detail() << "\n";
   }
   catch (std::exception& e)
   {
      std::cerr << "std::exception: " << e.what() << "\n";
   }
   return 1;
}
