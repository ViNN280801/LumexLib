# Relacy Race Detector (vendored)

Source: https://github.com/dvyukov/relacy, commit 2c87447b56b90cbccb5129c93a4e82eda5e7868e (2026-09-03).
License: the BSD-style license of the file `LICENSE` in this directory (Copyright (c) 2008-2013, Dmitry S. Vyukov).

Only the header directory `relacy/` and `LICENSE` are kept; the upstream tests, examples and build files are not.
The sources are unmodified. Relacy is a tool for LumexLib's own tests (the memory-order models of the split-count atomic
smart pointers, `lumex/tests/core/atomic/smart_ptr/split_count/relacy/`): it is built only with `-DLUMEX_BUILD_RELACY=ON`
and is never installed or packaged. `-DLUMEX_RELACY_DIR=<path>` points the build at another checkout instead.
