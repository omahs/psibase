#include <wasi/api.h>
#include "polyfill.hpp"

extern "C" __wasi_errno_t POLYFILL_NAME(fd_fdstat_set_flags)(__wasi_fd_t fd, __wasi_fdflags_t flags)
    __attribute__((__import_module__("wasi_snapshot_preview1"),
                   __import_name__("fd_fdstat_set_flags")))
{
   abortMessage("__wasi_fd_fdstat_set_flags not implemented");
}
