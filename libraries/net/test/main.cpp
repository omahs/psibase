#include <psibase/ExecutionContext.hpp>
#include <psibase/log.hpp>
#include <psibase/mock_timer.hpp>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

using namespace psibase;

struct ConsoleLog
{
   std::string type   = "console";
   std::string filter = "Severity >= warning";
   std::string format = "[{Severity}] [{Host}]: {Message}{?: {BlockId}: {BlockHeader}}";
};
PSIO_REFLECT(ConsoleLog, type, filter, format);

struct Loggers
{
   ConsoleLog stderr;
};
PSIO_REFLECT(Loggers, stderr);

extern std::vector<std::pair<Consensus, Consensus>> transitions;

int main(int argc, const char* const* argv)
{
   Loggers                    logConfig;
   std::optional<std::size_t> dataIndex;

   ExecutionContext::registerHostFunctions();

   Catch::Session session;

   using Catch::clara::Opt;
   auto cli =
       session.cli() |
       Opt(logConfig.stderr.filter,
           "filter")["--log-filter"]("The filter to apply to node loggers") |
       Opt(dataIndex, "index")["--data-index"]("The zero-based index into the test case data");
   session.cli(cli);

   if (int res = session.applyCommandLine(argc, argv))
      return res;

   if (auto seed = session.configData().rngSeed)
      psibase::test::global_random::set_global_seed(seed);

   if (dataIndex)
   {
      if (*dataIndex < transitions.size())
         transitions = {transitions[*dataIndex]};
   }

   loggers::configure(psio::convert_from_json<loggers::Config>(psio::convert_to_json(logConfig)));

   return session.run();
}
