#pragma once

#include <psio/fracpack.hpp>
#include <psio/to_json.hpp>

namespace psibase
{
   template <typename S>
   void generateActionJsonTemplate(std::tuple<>*,
                                   bool               needComma,
                                   const char* const* begin,
                                   const char* const* end,
                                   S&                 stream)
   {
   }

   template <typename T, typename... Ts, typename S>
   void generateActionJsonTemplate(std::tuple<T, Ts...>*,
                                   bool               needComma,
                                   const char* const* begin,
                                   const char* const* end,
                                   S&                 stream)
   {
      if (needComma)
         stream.write(',');
      if (begin != end)
         to_json(*begin, stream);
      else
         to_json("error_missing_arg_name", stream);
      stream.write(':');
      T empty = {};
      to_json(empty, stream);
      generateActionJsonTemplate((std::tuple<Ts...>*)nullptr, true,
                                 begin != end ? begin + 1 : begin, end, stream);
   }

   template <typename T>
   std::vector<char> generateActionJsonTemplate()
   {
      std::vector<char>   json;
      psio::vector_stream s{json};
      s.write('{');
      bool needComma = false;
      psio::reflect<T>::for_each(
          [&](const psio::meta& meta, auto member)
          {
             using MemPtr = decltype(member(std::declval<T*>()));
             if constexpr (std::is_member_function_pointer_v<MemPtr>)
             {
                if (needComma)
                   s.write(',');
                needComma = true;
                to_json(meta.name, s);
                s.write(':');
                s.write('{');
                generateActionJsonTemplate(
                    (decltype(psio::tuple_remove_view(
                        psio::args_as_tuple(std::declval<MemPtr>())))*)nullptr,
                    false, meta.param_names.begin(), meta.param_names.end(), s);
                s.write('}');
             }
          });
      s.write('}');
      return json;
   }
}  // namespace psibase
