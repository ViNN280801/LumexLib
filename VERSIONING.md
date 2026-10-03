# Versioning

A LumexLib version has four components, `MAJOR.MINOR.PATCH.TWEAK`, set in one place: `project(LumexLib VERSION ...)` in the root `CMakeLists.txt`. A release is a commit tagged `vMAJOR.MINOR.PATCH.TWEAK`, with a dated section in [CHANGELOG.md](CHANGELOG.md) and a GitHub release. These rules apply from 2026-10-03; the releases made before them are listed at the end.

## What raises which component

| Component | Raised when a release contains | Examples |
| --- | --- | --- |
| MAJOR | an incompatible change of the API: code written for the previous version may not compile, or may compile with a different meaning | a public function, class or macro removed or renamed; a public signature changed; the documented behavior of a public function changed in a way callers must adapt to |
| MINOR | an incompatible change of the ABI with the API unchanged: consumers have to be rebuilt, not edited | the layout of a public class or struct changed; an exported function of a compiled module removed or changed; an inline function or template that compiled consumers instantiate changed in a way that mixes badly with old object files |
| PATCH | a functional change that keeps the API and the ABI compatible | a new module, class or function; a bug fix; a change of observable behavior or output; a performance improvement |
| TWEAK | no functional change | tests; documentation and comments; examples; build scripts, packaging and CI; refactoring that keeps the behavior |

When a release contains several kinds of change, the highest one decides and the components after it restart at 0: `1.0.2.3` is followed by `1.0.3.0` for a functional change, by `1.1.0.0` for an ABI change.

A number belongs to a release, not to a single change: everything merged before a release goes out under that release's number. Between releases the release's section in `CHANGELOG.md` is marked "в разработке" ("in development"), and `CMakeLists.txt` already carries the number the release will have.

## Binary compatibility

- Linux: the SONAME of every compiled module is `libLumex<Module>.so.MAJOR.MINOR`. A PATCH or TWEAK release keeps it, so a consumer picks up the new library without relinking; a MINOR or MAJOR release changes it.
- Windows: `FILEVERSION` and `PRODUCTVERSION` of every DLL carry all four components.
- Header-only modules have no binary of their own; their changes are classified by the same rules, for the code that includes them.

## Published releases do not change

A pushed tag, its GitHub release and its dated `CHANGELOG.md` section stay as they are. A wrong version number is corrected by the next release and noted in the section of the release that got it; a defective release is noted in its section and superseded by the next one.

## Releases made before these rules

| Release | Raised | By these rules | Why |
| --- | --- | --- | --- |
| `v1.0.0.1` | TWEAK | PATCH | a bug fix (aggregates made only of `std::optional` fields in field reflection) |
| `v1.0.0.2` | TWEAK | PATCH | new API (`success()` and `failure()` in `core/expected`) |
| `v1.0.1.1` | TWEAK | PATCH | changed behavior and performance (stack traces on POSIX without `addr2line`) |

Their numbers stay. The next release, `v1.0.2.0`, follows these rules.
