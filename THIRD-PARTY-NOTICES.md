# Third-party notices

LumexLib is released under the MIT license in [LICENSE](LICENSE). It contains code derived from, or ships the sources of, the projects below. Their notices are reproduced or referenced here as their licenses require.

## pugixml

The XML module (`lumex/xml/`, the `lumex::xml` target) is derived from pugixml (https://github.com/zeux/pugixml): the DOM classes, the parser and writers, and the XPath implementation with its allocator. The code that was moved out of the XML module into other modules is derived from pugixml as well: the byte-order helpers `byte_swap` (below C++20) and `is_little_endian` in `lumex/core/utility/bit/`. The XML code is compiled into the LumexXml library, so this notice goes with every copy of LumexLib. It also goes with every program that uses the moved code, including one that does not link `lumex::xml`: those are header-only inline functions and become part of that program.

```
MIT License

Copyright (c) 2006-2026 Arseny Kapoulkine

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## JSON for Modern C++ (nlohmann/json) 3.12.0

Vendored as a single header in `3rdparty/nlohmann/json.hpp` (MIT, Copyright (c) 2013-2025 Niels Lohmann); the notice is in the header itself. It is used by field reflection, the JSON logger configuration and `LumexSettingsJSON`, and is not installed with LumexLib: a consumer supplies its own copy.

## GoogleTest 1.12.1 and 1.18.0

Vendored sources in `3rdparty/googletest-1.12.1/` and `3rdparty/googletest-1.18.0/` (BSD 3-Clause, Copyright 2008, Google Inc.); the license text is in the `LICENSE` file of each directory. They build LumexLib's own tests only and are never installed.
