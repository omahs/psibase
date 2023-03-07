#include <triedent/database.hpp>
#include <triedent/debug.hpp>

namespace triedent
{
   database::database(const std::filesystem::path& dir, const config& cfg, access_mode mode)
       : _ring{dir / "data",
               {
                   .max_ids    = cfg.max_objects,
                   .hot_bytes  = 1ull << cfg.hot_pages,
                   .warm_bytes = 1ull << cfg.warm_pages,
                   .cool_bytes = 1ull << cfg.cool_pages,
                   .cold_bytes = 1ull << cfg.cold_pages,
               },
               mode},
         _file{dir / "db", mode},
         _root_release_session{_ring}
   {
      if (_file.size() == 0)
      {
         _file.resize(sizeof(database_memory));
         new (_file.data()) database_memory();
      }

      _dbm = reinterpret_cast<database_memory*>(_file.data());
   }

   database::database(const std::filesystem::path& dir, access_mode mode)
       : database(dir, config{}, mode)
   {
   }

   database::~database() {}

   void database::create(std::filesystem::path dir, config cfg)
   {
      if (std::filesystem::exists(std::filesystem::symlink_status(dir / "db")) ||
          std::filesystem::exists(std::filesystem::symlink_status(dir / "data")))
         throw std::runtime_error("directory already exists: " + dir.generic_string());

      std::filesystem::create_directories(dir / "data");

      (void)database{dir, cfg, access_mode::read_write};
   }

   void database::print_stats(bool detail)
   {
      //_ring.dump(detail);
   }
}  // namespace triedent
