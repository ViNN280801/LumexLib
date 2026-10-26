# LumexInterprocess: what processes on one machine share, from C++11 {#lumex_interprocess}

`lumex::core::interprocess` (target `lumex::interprocess`, a compiled library, option `LUMEX_BUILD_INTERPROCESS`; needs `lumex::expected`, `lumex::filesystem`, `lumex::reflection` and `lumex::utility`) holds one tool for now: `lock::interprocess_lock`, an exclusive lock of a file. One lock file per resource (an instrument, a port): the name of the file is the key. Two processes that take the lock of the same file never hold it at the same time, and the system releases the lock when its process ends in any way, a crash and the task manager included.

```cpp
// lumex/examples/interprocess/example_interprocess_lock.cpp (shortened)
#include "lumex/core/interprocess/LumexInterprocess"

namespace lk = lumex::core::interprocess::lock;

auto lock = lk::interprocess_lock::try_acquire (directory / "device_a.lock",
                                                "{\"pid\":1234}");
if (!lock && lock.error ().kind == lk::lock_error::busy)
  {
    // Another process holds it; its payload says which one.
    auto const owner = lk::interprocess_lock::read_payload (
        directory / "device_a.lock");
  }
```

## The class

- `try_acquire (file, payload)` never waits. It creates the directories of `file` and the file itself when they are missing, takes the lock, and cuts the file to `payload` (a short text that names the holder, for example its process ID and a time as JSON). The result is `expected<interprocess_lock, lock_failure_t>`: the lock, or why there is none.
- `read_payload (file)` reads the payload without taking the lock, whether the lock is held or not; at most the first 64 KiB. It is a hint for a message: it may be cut short while the holder writes it, and any program may write into the file.
- `release ()` or the destructor unlock and close the file; the file stays. The object is move-only; a moved-from object holds nothing. `owns_lock ()` tells whether the object still holds its lock.
- An object is not shared between threads. The class keeps no global state.

Failures are values, never exceptions: `lock_failure_t { kind, system_code }`. `kind` is the reflected enum `lock_error` (`to_string ()` gives `"busy"`, `"access_denied"`, `"io_error"`; `lumex::core::fmt` formats it by name):

| `lock_error` | When | `system_code` |
| --- | --- | --- |
| `busy` | another open file holds the lock: another process, or another `interprocess_lock` of this process on the same file | `ERROR_LOCK_VIOLATION`, `ERROR_SHARING_VIOLATION`; `EWOULDBLOCK` |
| `access_denied` | the user may not create or open the directory or the file | `ERROR_ACCESS_DENIED`; `EACCES`, `EPERM`, `EROFS` |
| `io_error` | anything else: no such file (`read_payload`), a full disk, a path the system refuses, a directory in place of the file | the number of the system |

The number of the system travels for the log; its text does not (on Windows it is in the ANSI code page).

## How it works

| | Windows | POSIX |
| --- | --- | --- |
| Open | `CreateFileW`, sharing read and write, `OPEN_ALWAYS` | `open (O_CREAT \| O_RDWR \| O_CLOEXEC, 0666)` |
| Lock | `LockFileEx (LOCKFILE_EXCLUSIVE_LOCK \| LOCKFILE_FAIL_IMMEDIATELY)` on one byte at offset `0x7FFFFFFF00000000` | `flock (LOCK_EX \| LOCK_NB)` |
| Payload | written from the start, `SetEndOfFile` after it | `pwrite` from the start, `ftruncate` after it |
| Release | `UnlockFileEx`, `CloseHandle` | `flock (LOCK_UN)`, `close` |

- The locked byte lies far behind the data: a range locked through one handle cannot be read through another, and `read_payload` must read while the lock is held.
- `flock`, not `fcntl`: a `fcntl` record lock belongs to the process and is released when the process closes any descriptor of the file (a log reader, a backup tool in the same process). A `flock` lock belongs to the open file.
- The file is never deleted. Deleting a file that another process has open would let two processes lock two different files of the same name.
- The lock is advisory: it protects against programs that take the same lock, not against a program that just opens the file.
- On Windows the system releases the lock of a process that ended without unlocking soon, but not always at once: a program that starts right after such an end may retry a few times (the tests retry for up to 5 s).
- Paths are UTF-8 (`lumex::core::filesystem::fs::path`) and go to the wide functions of Windows, so a data directory under a profile with a Cyrillic name works. From C++17 a `std::filesystem::path` converts.

## Building

The library is compiled as C++11 and exports the class through `LUMEX_INTERPROCESS_API` (`lumex/LumexExport.hpp`, keyed on `LumexCore_interprocess_EXPORTS`). Each system has its own source file (`lock/LumexInterprocessLockWindows.cpp`, `lock/LumexInterprocessLockPosix.cpp`); the module's `CMakeLists.txt` compiles the one of the target, and each file stops with `#error` on the wrong system.

## Tests and examples

- `lumex/tests/core/interprocess/`: `ctest -R '^interprocess\.'`, suites at C++11, 14, 17, 20 and 23. Besides the cases of one process (another object of the same process is another owner), a second process holds the lock and is killed (`TerminateProcess`, `SIGKILL`), and death tests end a holder with `_Exit`; one of them checks that the child locked the very file the parent reads.
- `lumex/examples/interprocess/`: `LumexInterprocessExampleLock` (the whole class in one process) and `LumexInterprocessExampleLockHold <seconds>` (run it in two consoles).
