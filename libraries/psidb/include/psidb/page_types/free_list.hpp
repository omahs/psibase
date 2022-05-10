#pragma once

#include <psidb/allocator.hpp>
#include <psidb/page_header.hpp>
#include <psidb/sync/shared_value.hpp>

namespace psidb
{

   struct page_free_list : page_header
   {
      page_id                      _size = 0;
      static constexpr std::size_t capacity =
          (page_size - sizeof(page_header)) / sizeof(page_id) - 1;
      page_id _children[capacity];
   };

   struct pending_gc_list
   {
      page_free_list* _head;
      void            queue_gc(allocator& alloc, page_header* page)
      {
         if (_head == nullptr || _head->_size == page_free_list::capacity)
         {
            auto new_head          = new (alloc.allocate(page_size)) page_free_list;
            new_head->_children[0] = alloc.get_id(_head);
            new_head->_size        = 1;
            _head                  = new_head;
         }
         _head->_children[_head->_size++] = alloc.get_id(page);
      }
      // TODO: single page deallocation can integrate with the allocator's free list
      // to allow this function to run in constant time.
      void free(allocator& alloc)
      {
         while (_head)
         {
            for (std::size_t i = 1; i < _head->_size; ++i)
            {
               alloc.deallocate(alloc.translate_page_address(_head->_children[i]), page_size);
            }
            _head = static_cast<page_free_list*>(alloc.translate_page_address(_head->_children[0]));
         }
      }
   };

   struct gc_manager
   {
      pending_gc_list lists[2] = {};
      shared_value    manager;
      void            queue_gc(allocator& alloc, page_header* page)
      {
         auto val = manager.load_and_lock();
         lists[val].queue_gc(alloc, page);
         // FIXME: RAII
         manager.unlock(val);
      }
      void process_gc(allocator& alloc)
      {
         if (manager.try_wait())
         {
            auto val = manager.load(std::memory_order_relaxed);
            lists[val ^ 1].free(alloc);
            manager.async_store(val ^ 1);
         }
      }
   };

}  // namespace psidb
