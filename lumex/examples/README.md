# Examples of LumexLib

One directory per module, each with at least two programs: a walkthrough of the
API and a small workflow. They run without hardware and without a network
beyond the loopback interface. With `LUMEX_BUILD_TESTS=ON` the tree compiles
every example and CTest runs it (names `examples.<directory>.<program>`).

## In the installed package

The package carries the **sources** of the examples, not their binaries:

```text
<prefix>/share/examples/
    cmake/                   the helper every directory includes
    <module>/CMakeLists.txt  a project of its own
    <module>/example_*.cpp   the programs (and the headers they share)
```

A directory is there when its module was built. Each one builds against the
installed package:

```sh
cmake -S <prefix>/share/examples/span -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
cmake --build build
./build/LumexSpanExample
```

`LumexLib_DIR` is the directory with `LumexLibConfig.cmake`; with
`-DCMAKE_PREFIX_PATH=<prefix>` the package is found without it. The programs
link the shared libraries of the package, so a Windows build needs the `bin`
directory of the package on `PATH` when it runs them.

Some directories need a package of the consumer:

- `json`, and the export example of `protocol` (when the library was built with
  `LUMEX_PROTOCOL_WITH_JSON`): nlohmann/json 3.x, found with
  `find_package (nlohmann_json 3)`. Without it the export example is skipped
  with a message and `json` stops at the configure.
- `reflection`: the field reflection example builds when nlohmann/json is found.

The compiler of the consumer must be the one of the package (its name carries
the compiler and the C++ standard), as for any use of the libraries.

## In the source tree

`lumex/examples/<module>/CMakeLists.txt` is the same file the package carries:
it starts with its own `cmake_minimum_required` and `project`, includes
`cmake/LumexExampleHelpers.cmake` and declares its programs with
`lumex_example_executable`. In the tree it is added by
`lumex/examples/CMakeLists.txt` under the `LUMEX_BUILD_*` option of the module;
standalone (it is the top-level project) it finds the package with
`find_package (LumexLib)`.

The udev rules in `transport/udev` are not examples: the package installs them
into `share/LumexLib/udev`.
