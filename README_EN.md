# CJASTHelper

Cangjie Abstract Syntax Tree Helper, providing extended capabilities for manipulating the Cangjie Abstract Syntax Tree, such as printing source code (after desugaring), etc.

> Note:
>
> This tool is developed based on the open-source Cangjie project and is currently in the experimental stage.

## Build

### Dependency Download

[Cangjie Open Source Repository](https://gitcode.com/Cangjie/cangjie_compiler)

Refer to the [Cangjie SDK Build Guide](https://gitcode.com/Cangjie/cangjie_build) to build the required components as needed.

### Environment Variable Configuration

This tool depends on the header files and build artifacts from the Cangjie open-source repository. Assume the local Cangjie source directory is `${xxx}/cangjie_compiler/` and the built SDK directory is `${yyy}/cangjie/`.

```bash
# Configure the Cangjie source path
export CANGJIE_SRC_HOME=${xxx}/cangjie_compiler
# Configure the Cangjie binary environment (CANGJIE_HOME, LD_LIBRARY_PATH environment variables)
source ${yyy}/cangjie/envsetup.sh
```

### Build Commands

After setting up the dependencies and environment, use `build.sh` to build this tool.

```bash
# Assume the source path of this tool is CJASTHelper_SRC
cd ${CJASTHelper_SRC}
# Build the debug version of the tool
bash build.sh -t Debug -b
# Build the release version of the tool
bash build.sh -t Release -b
```

## Features

### Print Source Code

The `--dump-source` option is provided to support printing the source code after a specific stage, outputting it to a specified directory. Supported parameter values: `parser`, `desugared-parser`, `sema`, `desugared-sema`.

- `parser`: Prints the source code after syntax parsing.
- `desugared-parser`: Prints the source code after syntax parsing and desugaring.
- `sema`: Prints the source code after semantic analysis.
- `desugared-sema`: Prints the source code after semantic analysis and desugaring.

```bash
# Assume the built tool is located at ${CJASTHelper_SRC}/build/bin/cjah, the source code is at ${zzz}/main.cj, and the output directory is ${OUT_DIR}.
${CJASTHelper_SRC}/build/bin/cjah --dump-source=desugared-sema ${zzz}/main.cj --output-dir ${OUT_DIR}
```

> Note:
>
> This tool requires the binary environment built by Cangjie. Please ensure you have executed `source ${yyy}/cangjie/envsetup.sh`.

### Configure Whether to Print Desugared Code

The `--dump-desugared` option is provided to configure whether to print desugared code. Supported parameter values: (default) `true`, `false`.

- `true`: Print desugared code.
- `false`: Do not print desugared code, attempt to restore the original user code before desugaring.

```bash
# Assume the built tool is located at ${CJASTHelper_SRC}/build/bin/cjah, the source code is at ${zzz}/main.cj, and the output directory is ${OUT_DIR}.
${CJASTHelper_SRC}/build/bin/cjah --dump-source=desugared-sema --dump-desugar=true --filter-decls=class,func ${zzz}/main.cj --output-dir ${OUT_DIR}
```

> Note:
> 
> This option is not fully implemented yet!

### Configure Declaration Filters

The `--filter-decls` option is provided to configure the list of top-level declaration types to print. Multiple values are supported. Supported parameter values: `func`, `class`, `interface`, `struct`, `enum`, `var`.

- By default, no filtering is applied, and all declarations are printed.
- To configure multiple values, e.g., `--filter-decls=class,func`, only top-level classes and functions will be printed.

```bash
# Assume the built tool is located at ${CJASTHelper_SRC}/build/bin/cjah, the source code is at ${zzz}/main.cj, and the output directory is ${OUT_DIR}.
${CJASTHelper_SRC}/build/bin/cjah --dump-source=desugared-sema --filter-decls=class,func ${zzz}/main.cj --output-dir ${OUT_DIR}
```

## Test Cases

### Dependency Download

[gtest Dependency Download](https://github.com/google/googletest/archive/tags/v1.17.0.zip)

> Note:
>
> After downloading, extract the source code to the `third_party/googletest-v1.17.0` directory in this project.

### Build

```bash
# Use the -g option to enable building tests that depend on Google Test
bash build.sh -g -t Release -b
```

> Note:
>
> After a successful build, the test executable `build/bin/cjah_test` will be generated.

### Execution

```bash
# Run all test cases
./build/bin/cjah_test

# Run CI test cases
./build/bin/cjah_test --gtest_filter="CJAHTest.Integration*"
```