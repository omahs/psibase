#pragma once

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/mp11/algorithm.hpp>
#include <iostream>
#include <memory>
#include <psibase/SignedMessage.hpp>
#include <psibase/log.hpp>
#include <psibase/message_serializer.hpp>
#include <psibase/net_base.hpp>
#include <psio/fracpack.hpp>
#include <queue>
#include <vector>

namespace psibase::net
{
   // message type 0 is reserved to ensure that message signatures
   // are disjoint from block signatures.
   // message types 1-31 are used for routing messages
   // message types 32-63 are used for consensus messages
   struct InitMessage
   {
      static constexpr unsigned type = 1;
      std::uint32_t             version;
      NodeId                    id;
      std::string to_string() const { return "init: version=" + std::to_string(version); }
   };
   PSIO_REFLECT(InitMessage, version, id);
   struct ProducerMessage
   {
      static constexpr unsigned type = 2;
      producer_id               producer;
      std::string               to_string() const { return "producer: " + producer.str(); }
   };
   PSIO_REFLECT(ProducerMessage, producer)

   template <typename T>
   concept has_block_id = requires(T& t) { t.block_id; };

   // This requires all producers to be peers
   template <typename Derived>
   struct direct_routing : message_serializer<Derived>
   {
      auto& peers() { return static_cast<Derived*>(this)->peers(); }
      auto& chain() { return static_cast<Derived*>(this)->chain(); }
      explicit direct_routing(boost::asio::io_context& ctx)
      {
         std::random_device rng;
         nodeId = std::uniform_int_distribution<NodeId>()(rng);
      }
      static const std::uint32_t protocol_version = 0;
      auto                       get_message_impl()
      {
         return boost::mp11::mp_push_back<
             typename std::remove_cvref_t<
                 decltype(static_cast<Derived*>(this)->consensus())>::message_type,
             InitMessage, ProducerMessage>{};
      }
      template <typename Msg, typename F>
      void async_send_block(peer_id id, const Msg& msg, F&& f)
      {
         PSIBASE_LOG(peers().logger(id), debug) << "Sending message: " << msg.to_string();
         peers().async_send(id, this->serialize_message(msg), std::forward<F>(f));
      }
      // Sends a message to each peer in a list
      // each peer will receive the message only once even if it is duplicated in the input list.
      template <typename Msg>
      void async_multicast(std::vector<peer_id>&& dest, const Msg& msg)
      {
         std::sort(dest.begin(), dest.end());
         dest.erase(std::unique(dest.begin(), dest.end()), dest.end());
         auto serialized_message = this->serialize_message(msg);
         for (auto peer : dest)
         {
            PSIBASE_LOG(peers().logger(peer), debug) << "Sending message: " << msg.to_string();
            peers().async_send(peer, serialized_message, [](const std::error_code& ec) {});
         }
      }
      template <typename Msg>
      void multicast_producers(const Msg& msg)
      {
         std::vector<peer_id> producer_peers;
         for (auto [producer, peer] : producers)
         {
            producer_peers.push_back(peer);
         }
         async_multicast(std::move(producer_peers), msg);
      }
      template <typename Msg>
      void multicast(const Msg& msg)
      {
         // TODO: send to non-producers as well
         multicast_producers(msg);
      }
      template <typename Msg>
      void sendto(producer_id prod, const Msg& msg)
      {
         std::vector<peer_id> peers_for_producer;
         for (auto [iter, end] = producers.equal_range(prod); iter != end; ++iter)
         {
            peers_for_producer.push_back(iter->second);
         }
         async_multicast(std::move(peers_for_producer), msg);
      }
      struct connection;
      void connect(peer_id id)
      {
         async_send_block(id, InitMessage{.version = protocol_version, .id = nodeId},
                          [](const std::error_code&) {});
         if (auto producer = static_cast<Derived*>(this)->consensus().producer_name();
             producer != AccountNumber())
         {
            async_send_block(id, ProducerMessage{producer}, [](const std::error_code&) {});
         }
         static_cast<Derived*>(this)->consensus().connect(id);
      }
      void disconnect(peer_id id)
      {
         static_cast<Derived*>(this)->consensus().disconnect(id);
         for (auto iter = producers.begin(), end = producers.end(); iter != end;)
         {
            if (iter->second == id)
            {
               iter = producers.erase(iter);
            }
            else
            {
               ++iter;
            }
         }
      }
      template <template <typename...> class L, typename... T>
      static constexpr bool check_message_uniqueness(L<T...>*)
      {
         std::uint8_t ids[] = {T::type...};
         for (std::size_t i = 0; i < sizeof(ids) - 1; ++i)
         {
            for (std::size_t j = i + 1; j < sizeof(ids); ++j)
            {
               if (ids[i] == ids[j])
               {
                  return false;
               }
            }
         }
         return true;
      }
      template <typename T>
      void try_recv_impl(peer_id peer, psio::input_stream& s)
      {
         try
         {
            using message_type = std::conditional_t<NeedsSignature<T>, SignedMessage<T>, T>;
            return recv(peer, psio::from_frac<message_type>({s.pos, s.end}));
         }
         catch (std::exception& e)
         {
            PSIBASE_LOG(peers().logger(peer), warning) << e.what();
            peers().disconnect(peer);
         }
      }
      template <template <typename...> class L, typename... T>
      void recv_impl(peer_id peer, int key, std::vector<char>&& msg, L<T...>*)
      {
         psio::input_stream s(msg.data() + 1, msg.size() - 1);
         ((key == T::type ? try_recv_impl<T>(peer, s) : (void)0), ...);
      }
      void recv(peer_id peer, std::vector<char>&& msg)
      {
         using message_type = decltype(get_message_impl());
         static_assert(check_message_uniqueness((message_type*)nullptr));
         if (msg.empty())
         {
            PSIBASE_LOG(peers().logger(peer), warning) << "Invalid message";
            peers().disconnect(peer);
            return;
         }
         recv_impl(peer, msg[0], std::move(msg), (message_type*)0);
      }
      void recv(peer_id peer, const InitMessage& msg)
      {
         PSIBASE_LOG(peers().logger(peer), debug) << "Received message: " << msg.to_string();
         if (msg.version != protocol_version)
         {
            peers().disconnect(peer);
            return;
         }
         if (msg.id)
         {
            peers().set_node_id(peer, msg.id);
         }
      }
      void recv(peer_id peer, const ProducerMessage& msg)
      {
         PSIBASE_LOG(peers().logger(peer), debug) << "Received message: " << msg.to_string();
         producers.insert({msg.producer, peer});
      }
      template <typename T>
      void recv(peer_id peer, const SignedMessage<T>& msg)
      {
         std::vector<char> raw;
         raw.reserve(msg.data.size() + 1);
         raw.push_back(T::type);
         raw.insert(raw.end(), msg.data.data(), msg.data.data() + msg.data.size());
         Claim claim = msg.data->signer();
         PSIBASE_LOG(peers().logger(peer), debug) << "Received message: " << msg.to_string();
         if constexpr (has_block_id<T>)
         {
            chain().verify(msg.data->block_id(), {raw.data(), raw.size()}, claim, msg.signature);
         }
         else
         {
            chain().verify({raw.data(), raw.size()}, claim, msg.signature);
         }
         if constexpr (has_recv<SignedMessage<T>, Derived>)
         {
            static_cast<Derived*>(this)->consensus().recv(peer, msg);
         }
         else
         {
            static_cast<Derived*>(this)->consensus().recv(peer, msg.data.unpack());
         }
      }
      void recv(peer_id peer, const auto& msg)
      {
         PSIBASE_LOG(peers().logger(peer), debug) << "Received message: " << msg.to_string();
         static_cast<Derived*>(this)->consensus().recv(peer, msg);
      }
      std::multimap<producer_id, peer_id> producers;
      NodeId                              nodeId = 0;
   };

}  // namespace psibase::net
