# Asio (standalone) 1.36.0

- **Upstream:** https://github.com/chriskohlhoff/asio, tag `asio-1-36-0`
- **Archive:** `https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-36-0.tar.gz`, SHA256 `0310a76b27e1854f09f696b30de57dc490b5e1b17faed1eb8c9a2891f956e52b` (computed on download, GitHub publishes no checksum for tag archives)
- **Vendored:** 21.10.2026
- **License:** Boost Software License 1.0 (`LICENSE_1_0.txt`, `COPYING`)

## Taken

`asio/include/asio.hpp`, `asio/include/asio/**` (header-only use), `asio/LICENSE_1_0.txt`, `asio/COPYING`.

## Left out

`asio/src/` (separate compilation, examples, tests, documentation), the autotools files, `include/Makefile.am`, `include/.gitignore`.

## Local changes

None.

## Relation to DChannel

`DChannel/src/3rdparty/asio/include/` is identical to this tree, file by file, once line endings are normalized (627 files compared, 21.10.2026; DChannel lacks only `include/Makefile.am` and `include/.gitignore`). LumexLib must use the same version and the same configuration macros as DChannel (`ASIO_STANDALONE`, `ASIO_NO_DEPRECATED`), because a process can hold both copies (see the LumexLib transport specification, F3).
