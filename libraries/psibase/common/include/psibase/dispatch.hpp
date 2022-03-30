#pragma once
#include <psio/fracpack.hpp>
#include <psibase/intrinsic.hpp>

namespace psibase {

   namespace detail {
      struct FRACPACK raw_variant_view {
         uint8_t  action_idx;
         uint32_t size;
         char     data[];
      };

      /*
      struct FRACPACK action_view final {
          uint64_t       sender;
          uint64_t       receiver;
          uint64_t       action;
          vector<char>   param;
      };
      */
   };

   template<typename R, typename T, typename MemberPtr, typename... Args>
   void call_method( T& contract, MemberPtr method, Args&&... args ) {
       psio::shared_view_ptr<R> p((contract.*method)( std::forward<decltype(args)>(args)... ));
       raw::set_retval( p.data(), p.size() );
   }

   /**
    *  This method is called when a contract receives a call and will
    *  and will call the proper method on Contact assuming Contract has
    *  used the PSIO_REFLECT_INTERFACE macro.
    */
   template<typename Contract>
   void dispatch( account_num sender, account_num receiver) {
      Contract contract;
      contract.psibase::contract::dispatch_set_sender_receiver( sender, receiver );
      /*
      uint32_t action_size = raw::get_current_action();
      auto data = get_result(action_size);
      */
      action act = get_current_action(); /// action view...
      check( act.raw_data.size() >= 5, "action data too short" );
      auto& rvv = *reinterpret_cast<const detail::raw_variant_view*>(act.raw_data.data());
      check( act.raw_data.size()-5 == rvv.size, "invalid size encoded" );

      bool called = psio::reflect<Contract>::get( rvv.action_idx, [&]( auto member_func ) {
         using result_type = decltype( psio::result_of(member_func) );
         using param_tuple = decltype( psio::tuple_remove_view(psio::args_as_tuple(member_func)) );

         psibase::check( psio::fracvalidate<param_tuple>( rvv.data, rvv.data+rvv.size ).valid, "invalid argument encoding" );
         psio::const_view<param_tuple>  param_view( rvv.data );

         param_view->call( [&]( auto... args ){
            if constexpr( std::is_same_v<void, result_type> ) {
                (contract.*member_func)( std::forward<decltype(args)>(args)... );
            } else {
                call_method<result_type,Contract,decltype(member_func),decltype(args)...>( contract, member_func, std::forward<decltype(args)>(args)... );
            }
         }); // param_view::call
      }); // reflect::get
      check( called, "unknown contract action" );
   } // dispatch

}

#define PSIBASE_DISPATCH( CONTRACT ) \
   extern "C" void called(psibase::account_num receiver, psibase::account_num sender ) \
   {  \
      psibase::dispatch<CONTRACT>( sender, receiver ); \
   } \
   extern "C" void __wasm_call_ctors();  \
   extern "C" void start(psibase::account_num this_contract) { __wasm_call_ctors(); }  \
