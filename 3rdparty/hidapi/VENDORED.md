# hidapi 0.15.0

- **Upstream:** https://github.com/libusb/hidapi, tag `hidapi-0.15.0`
- **Archive:** `https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.15.0.tar.gz`, SHA256 `5d84dec684c27b97b921d2f3b73218cb773cf4ea915caee317ac8fc73cef8136` (computed on download, GitHub publishes no checksum for tag archives)
- **Vendored:** 21.10.2026
- **License:** hidapi is offered under three licenses (`LICENSE.txt`); LumexLib uses it under the **BSD** license (`LICENSE-bsd.txt`). `LICENSE-gpl3.txt` and `LICENSE-orig.txt` are kept unchanged, as upstream requires.

## Taken

`hidapi/hidapi.h`; `windows/` sources and headers (`hid.c`, `hidapi_descriptor_reconstruct.c/.h`, `hidapi_cfgmgr32.h`, `hidapi_hidclass.h`, `hidapi_hidpi.h`, `hidapi_hidsdi.h`, `hidapi_winapi.h`); `linux/hid.c` (hidraw); `libusb/hid.c`, `hidapi_libusb.h`, `hidapi_thread_pthread.h`; `mac/hid.c`, `hidapi_darwin.h`; `udev/69-hid.rules` (template for the Linux permission rule); `LICENSE*.txt`, `AUTHORS.txt`, `VERSION`.

## Left out

Upstream build systems (CMake, autotools, meson, Visual Studio projects, makefiles), `hidapi.rc`, `android/`, `netbsd/`, `hidtest/`, `testgui/`, `documentation/`, `doxygen/`, `dist/`, `pc/`, `src/`, `subprojects/`, CI files. LumexLib builds the sources with its own CMake (`cmake/LumexHidapi.cmake`).

## Local changes

None. DChannel's copy carries three Windows SDK include patches (`windows/hid.c`, `hidapi_cfgmgr32.h`, `hidapi_hidpi.h`); they are not applied here, because the unpatched Windows backend compiles with MSVC and clang-cl (21.10.2026). If another toolchain of the verification set needs them, they are added here as documented patches.

## Relation to DChannel

DChannel compiles hidapi **0.15.0 sources** (identical to this release, apart from the three patches above) with the **0.14.0 header** (`dchannel/pitfalls.md`, 2026-10-21). This tree is the full 0.15.0 release, so both libraries run the same hidapi code in one process (Linux symbol interposition, transport specification F3). LumexLib never calls `hid_exit`.
