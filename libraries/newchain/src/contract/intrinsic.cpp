#include <newchain/intrinsic.hpp>

#include <eosio/from_bin.hpp>

namespace newchain
{
   std::vector<char> get_result(uint32_t size)
   {
      std::vector<char> result(size);
      if (size)
         raw::get_result(result.data(), result.size());
      return result;
   }

   std::vector<char> get_result() { return get_result(raw::get_result(nullptr, 0)); }

   action get_current_action()
   {
      auto data = get_result(raw::get_current_action());
      return eosio::convert_from_bin<action>(data);
   }

   std::vector<char> call(const char* action, uint32_t len)
   {
      return get_result(raw::call(action, len));
   }

   std::vector<char> call(eosio::input_stream action)
   {
      return get_result(raw::call(action.pos, action.remaining()));
   }

   std::vector<char> call(const action& action) { return call(eosio::convert_to_bin(action)); }
}  // namespace newchain
