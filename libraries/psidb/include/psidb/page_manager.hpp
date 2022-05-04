#pragma once

#include <cstddef>
#include <map>
#include <psidb/allocator.hpp>
#include <psidb/checkpoint.hpp>
#include <psidb/node_ptr.hpp>
#include <psidb/page_header.hpp>
#include <psidb/sync/mutex_set.hpp>
#include <psidb/sync/shared_queue.hpp>
#include <stdexcept>
#include <utility>

namespace psidb
{

   static constexpr std::size_t max_memory_pages = 0x10000000;

   class page_manager
   {
     public:
      page_manager(int fd, std::size_t num_pages);
      page_manager(const char* path, std::size_t num_pages);
      ~page_manager();

      bool         is_memory_page(page_id id) { return id < max_memory_pages; }
      page_header* translate_page_address(page_id id)
      {
         return reinterpret_cast<page_header*>(reinterpret_cast<char*>(_allocator.base()) +
                                               id * page_size);
      }
      page_id get_id(const page_header* page)
      {
         return (reinterpret_cast<const char*>(page) -
                 reinterpret_cast<const char*>(_allocator.base())) /
                page_size;
      }
      page_header* get_page(node_ptr ptr)
      {
         auto id = *ptr;
         if (is_memory_page(id))
         {
            return translate_page_address(id);
         }
         else
         {
            return read_page(ptr, id);
         }
      }
      page_header* get_page(node_ptr ptr, version_type version)
      {
         auto id = *ptr;
         if (is_memory_page(id))
         {
            while (true)
            {
               page_header* p = translate_page_address(id);
               if (p->version <= version)
               {
                  return p;
               }
               else
               {
                  id = p->prev;
               }
            }
         }
         else
         {
            return read_page(ptr, id);
         }
      }
      page_header* get_page(page_id& id)
      {
         if (is_memory_page(id))
         {
            return translate_page_address(id);
         }
         else
         {
            return read_page(id);
         }
      }
      void* get_pages(page_id& id, std::size_t count)
      {
         if (is_memory_page(id))
         {
            return translate_page_address(id);
         }
         else
         {
            return read_pages(id, count);
         }
      }
      std::pair<page_header*, page_id> allocate_page()
      {
         page_header* result = static_cast<page_header*>(_allocator.allocate(page_size));
         return {result, get_id(result)};
      }
      void set_root(const checkpoint& c, int db, page_id new_root)
      {
         c._root->table_roots[db] = new_root;
      }
      page_header* root(const checkpoint& c, int db) { return get_page(c._root->table_roots[db]); }
      page_id get_root_id(const checkpoint& c, int db) const { return c._root->table_roots[db]; }
      version_type get_version(const checkpoint& c) { return c._root->version; }
      checkpoint   get_head() { return {last_commit}; }
      checkpoint   start_transaction()
      {
         if (_flush_pending)
         {
            _flush_version = head->version;
            _flush_pending = false;
            _dirty_flag    = switch_dirty_flag(_dirty_flag);
            // TODO: should this be in async_flush?
            if (head == last_commit)
            {
               start_flush(head);
            }
         }
         auto [iter, inserted] = active_checkpoints.emplace(head->version + 1, *head);
         head                  = &iter->second;
         ++head->version;
         return {head};
      }
      void commit_transaction(const checkpoint& c)
      {
         last_commit = c._root;
         if (last_commit->version == _flush_version)
         {
            start_flush(last_commit);
         }
      }
      void async_flush()
      {
         if (head == last_commit)
         {
            if (_flush_version != head->version)
            {
               _flush_version = head->version;
               _flush_pending = false;
               _dirty_flag    = switch_dirty_flag(_dirty_flag);
               start_flush(head);
            }
         }
         else
         {
            _flush_pending = true;
         }
      }
      // Mark a page as dirty.
      void touch_page(page_header* page, version_type version)
      {
         if (version > _flush_version)
         {
            page->set_dirty(_dirty_flag, true);
         }
         else
         {
            page->set_dirty(switch_dirty_flag(_dirty_flag), true);
         }
      }
      enum value_reference_flags : std::uint16_t
      {
         file,
         memory
      };
      struct value_reference
      {
         value_reference_flags flags;
         std::uint16_t         offset;
         std::uint32_t         size;
         union
         {
            const char* data;
            page_id     page;
         };
      };
      value_reference clone_value(std::string_view data)
      {
         if (data.size() > UINT32_MAX)
         {
            throw std::invalid_argument("value too large");
         }
         void* copy = _allocator.allocate(data.size());
         std::memcpy(copy, data.data(), data.size());
         return {value_reference_flags::memory, 0, static_cast<std::uint32_t>(data.size()),
                 static_cast<const char*>(copy)};
      }

     private:
      page_manager(const page_manager&) = delete;

      page_flags switch_dirty_flag(page_flags f)
      {
         if (f == page_flags::dirty0)
         {
            return page_flags::dirty1;
         }
         else if (f == page_flags::dirty1)
         {
            return page_flags::dirty0;
         }
         else
         {
            __builtin_unreachable();
         }
      }

      void    queue_gc(page_header* node);
      page_id allocate_file_page();
      page_id allocate_file_pages(std::size_t count);

      // Manage the in memory page cache
      void*        read_pages(page_id& id, std::size_t count);
      page_header* read_page(page_id& id);
      page_header* read_page(node_ptr ptr, page_id id);
      void         evict_page(node_ptr ptr);

      // Write dirty pages back to the filesystem
      void write_worker();
      void queue_write(page_id dest, page_header* src, std::atomic<int>* refcount);
      void write_pages(page_id           dest,
                       const void*       src,
                       std::size_t       count,
                       std::atomic<int>* refcount);
      void write_page(page_header* page, page_flags dirty_flag, std::atomic<int>* refcount);
      bool write_tree(page_id           page,
                      version_type      version,
                      page_flags        dirty_flag,
                      std::atomic<int>* refcount);
      void start_flush(checkpoint_root* c);

      // Raw file io
      void read_page(void* out, page_id id, std::size_t count = 1);
      void write_page(const void* in, page_id id);

      // Database header ops
      void read_header(database_header* header);
      void read_header();
      void prepare_flush(page_id& root);
      void prepare_header(database_header* header);
      void write_header();

      struct write_queue_element
      {
         std::int64_t dest;
         const void*  src;
         std::size_t  size;
         // completion handler
         std::atomic<int>* counter;
      };

      allocator                         _allocator;
      int                               fd             = -1;
      page_flags                        _dirty_flag    = page_flags::dirty0;
      bool                              _flush_pending = false;
      version_type                      _flush_version = 0;
      shared_queue<write_queue_element> _write_queue;
      // TODO: use a real allocation algorithm
      page_id next_file_page = max_memory_pages;

      checkpoint_root* stable_checkpoint = nullptr;
      checkpoint_root* last_commit       = nullptr;
      checkpoint_root* head              = nullptr;
      // needs reference count
      std::map<version_type, checkpoint_root> active_checkpoints;

      mutex_set<page_id> _page_load_mutex;

      std::thread _write_worker{[this]() { write_worker(); }};
   };

}  // namespace psidb
