# CJASTHelper

Cangjie Abstract Syntax Tree Helper — an AST manipulation and source-conversion tool
built on the open-source Cangjie compiler frontend (`libcangjie-lsp`). It provides
multi-stage source printing (including desugared output), AST dumping, syntax-only
checking, macro expansion control, and a plugin-based analysis pass pipeline.

> Note: This tool is developed based on the open-source Cangjie project and is
> currently in the experimental stage.

## Feature Overview

| Capability | Description | Usage |
|---|---|---|
| Multi-stage source printing | `parse` / `desugared-parse` / `macro` / `sema` / `desugared-sema` | `--dump-source` |
| AST dumping | Dump the AST as text into files | `--dump-ast` |
| Syntax-only checking | File or directory input (directories are checked per package automatically), with merged JSON diagnostics | `--check-syntax` |
| Macro expansion control | Enable/disable macro expansion as needed | `--enable-macro` |
| Declaration filtering | Keep only the specified kinds of top-level declarations | `--filter-decls` |
| Pass plugin pipeline | Desugaring and source reconstruction (incl. Java output) loaded as plugin libraries | `--pass-config` + `passes.json` |
| CJC option passthrough | Unrecognized arguments are forwarded verbatim to the frontend (`--output-dir`, `-p`, ...) | see `cjc -h` |

## Quick Start

### Dependencies

- Cangjie SDK (compile headers + build artifacts). Build it from the
  [Cangjie open-source repository](https://gitcode.com/Cangjie/cangjie_compiler)
  following the [Cangjie SDK build guide](https://gitcode.com/Cangjie/cangjie_build);
- nlohmann/json headers
  ([v3.12.0](https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip));
- CMake, Ninja, clang (Linux/macOS); CMake, Ninja, llvm-mingw
  (Windows: built from [mstorsjo/llvm-mingw 20220906](https://github.com/mstorsjo/llvm-mingw/archive/refs/tags/20220906.tar.gz)
  with `--with-default-msvcrt=msvcrt`).

### Build

Linux / macOS:

```bash
export CANGJIE_SRC_HOME=${xxx}/cangjie_compiler   # Cangjie source tree (provides include)
export JSON_PATH=${third_party}/json              # nlohmann/json dir (optional, defaults to third_party/json)
source ${yyy}/cangjie/envsetup.sh                 # sets CANGJIE_HOME, LD_LIBRARY_PATH
bash build.sh -t Release -b
```

Windows:

```powershell
# Machine-specific paths (SDK, llvm-mingw, ninja, etc.) are configured centrally in
# scripts/win_env.ps1 and can be overridden via environment variables
# Build options are aligned with build.sh (short options work on both platforms)
build.bat -b -t Release
# or: powershell -File build.ps1 -b -t Release
```

Build artifacts are placed under `build/bin/` (`cjah` / `cjah.exe`); use `-i` /
`bash build.sh -t Release -i` to install into `output/`.
See [Build Guide](./doc/build.md) and [Windows Build Guide](./doc/build-windows.md) for details.

### Run

```bash
mkdir -p out
# Print the source after semantic analysis and desugaring
# (the sema/desugared-sema stages need the passthrough --output-type option)
./build/bin/cjah --dump-source=desugared-sema --output-type=dylib main.cj --output-dir out
# Artifact: out/main_source.cj

# Syntax-only check (directory input, merged JSON diagnostics)
./build/bin/cjah --check-syntax=true src/ --diagnostic-format=json
```

Unrecognized arguments are forwarded to the frontend (`-p <pkg-path>`, `--output-dir`,
`-Woff`, ...). See the [Usage Reference](./doc/usage.md) for the full option list.

## Documentation

| Document | Content |
|---|---|
| [doc/build.md](./doc/build.md) | Linux / macOS build guide |
| [doc/build-windows.md](./doc/build-windows.md) | Windows build guide (toolchain, path config, troubleshooting) |
| [doc/usage.md](./doc/usage.md) | Usage reference (options, stages, output conventions, examples) |
| [doc/design.md](./doc/design.md) | Architecture (module layout, library dependencies, stage pipeline) |
| [doc/testing.md](./doc/testing.md) | Test suite build & run |

## Testing

Based on googletest v1.17.0 (`third_party/googletest-v1.17.0` or a prebuilt
`GTEST_RELEASE_PATH`):

```bash
bash build.sh -g -t Release -b                          # Linux/macOS
powershell -File build.ps1 -g -b       # Windows
./build/bin/cjah_test --gtest_filter="*/CJAHTest.CI001/*"   # CI pipeline regression cases
```

See [Testing Guide](./doc/testing.md).

## License

[Apache License 2.0](./LICENSE)