#pragma once

#include <psidb/cursor.hpp>
#include <psidb/page_manager.hpp>

namespace psidb
{
   class transaction
   {
     public:
      transaction(page_manager* db, const checkpoint& ck, version_type clone_version)
          : _db(db), _checkpoint(ck), _clone_version(clone_version)
      {
      }
      cursor get_cursor() { return {_db, _checkpoint}; }

      // when a transaction is destroyed
      // - if is committed or aborted do nothing
      // - if it is the head, abort
      // - if it is not the head, fold it into the next transaction

      // move constructor is allowed
      // move assignement is equivalent to destroying the lhs and then moving

      void insert(std::string_view key, std::string_view value)
      {
         cursor c = get_cursor();
         insert(c, key, value);
      }
      void insert(cursor& c, std::string_view key, std::string_view value)
      {
         c.insert(*this, key, value);
      }
      void erase(std::string_view key)
      {
         cursor c = get_cursor();
         erase(c, key);
      }
      void erase(cursor& c, std::string_view key)
      {
         c.lower_bound(key);
         if (c.valid() && c.get_key() == key)
         {
            erase(c);
         }
      }
      void erase(cursor& c) { c.erase(*this); }

      void commit() { _db->commit_transaction(_checkpoint, std::move(obsolete_nodes)); }
      // A transaction can only be aborted if it is the current head.
      void abort()
      {
         // iterate through new_nodes and revert them.
         for (node_ptr node : modified_nodes)
         {
            // relaxed load is safe because anything that wrote them
            // has already synchronized with us.  The last write was
            // either in this transaction or in a later transaction
            // that was aborted.
            // TODO: are we certain that this page is in memory?
            page_header* page = _db->translate_page_address(node->load(std::memory_order_relaxed));
            auto         prev = page->prev.load(std::memory_order_relaxed);
            // We don't need to synchronize with readers before
            // the next gc cycle.
            node->store(prev, std::memory_order_relaxed);
            // pages cannot be freed until it is certain
            // that no read transaction is accessing them.
            _db->queue_gc(page);
         }
         // TODO: check all roots
         page_header* root = _db->root(_checkpoint, 0);
         if (root->version == _db->get_version(_checkpoint))
         {
            _db->queue_gc(root);
         }
         _db->abort_transaction(std::move(_checkpoint));
         // Clean up state
         _db = nullptr;
         modified_nodes.clear();
      }

      void on_delete(page_header* p) { obsolete_nodes.push_back(p); }

      void on_copy(node_ptr ptr)
      {
         obsolete_nodes.push_back(_db->translate_page_address(
             _db->translate_page_address(ptr->load(std::memory_order_relaxed))->prev));
         modified_nodes.push_back(ptr);
      }

      // Nodes at or before this version will always be
      // cloned if any child is modified
      version_type always_clone_version() const { return _clone_version; }

     private:
      page_manager* _db;
      checkpoint    _checkpoint;
      version_type  _clone_version;
      // list of pages obsoleted by this transaction
      std::vector<page_header*> obsolete_nodes;
      // list of pages created by this transaction
      std::vector<node_ptr> modified_nodes;
   };

}  // namespace psidb
