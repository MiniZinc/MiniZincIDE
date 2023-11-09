<p align="center">
  <a href="https://www.minizinc.org/">
    <img src="https://www.minizinc.org/MiniZn_logo.png" alt="Logo" width="80" height="80">
  </a>

  <h3 align="center">MiniZinc IDE</h3>

  <p align="center">
    Integrated development environment for the high-level constraint modelling language
    <a href="https://www.minizinc.org">MiniZinc</a>.
    <br />
    <br />
    <a href="https://github.com/MiniZinc/libminizinc">MiniZinc Compiler</a>
    ·
    <a href="https://www.minizinc.org/doc-latest/en/minizinc_ide.html">Documentation</a>
    ·
    <a href="https://github.com/MiniZinc/MiniZincIDE/issues">Report Bug</a>
  </p>

  <p align="center">
    <img src="https://www.minizinc.org/doc-2.7.6/en/images/mzn-ide-playground-data.jpg" alt="The MiniZinc IDE">
  </p>
</p>

## Getting started

Packages for Linux, macOS and Windows can be found in the [releases](https://github.com/MiniZinc/MiniZincIDE/releases) or from [the MiniZinc
website](http://www.minizinc.org/software.html).

These packages contain the MiniZinc IDE, the MiniZinc compiler toolchain, as well as several solvers.

For more detailed installation instructions, see the [documentation](https://www.minizinc.org/doc-latest/en/installation.html).

## Building from source

The MiniZinc IDE is a [Qt](https://www.qt.io/) project and requires:
- A recent C++ compiler
- [Qt](https://www.qt.io/) (we target the latest LTS Qt version)
  - We also require the [Qt WebSockets](https://doc.qt.io/qt-6/qtwebsockets-index.html) module
- [Make](https://www.gnu.org/software/make/)

Ensure you clone the repository including submodules:

```sh
git clone --recurse-submodules https://github.com/MiniZinc/MiniZincIDE
cd MiniZincIDE
```

Then either build open the project (`MiniZincIDE.pro`) in Qt Creator and build, or from the command line:

```sh
mkdir build
cd build
qmake -makefile ../MiniZincIDE/MiniZincIDE.pro
make -j4
```

See the [MiniZinc compiler project](https://github.com/MiniZinc/libminizinc) for instructions on how to build the compiler toolchain.

### Running tests

The IDE has a test suite which can be compiled and run with:

```sh
mkdir test
cd test
qmake -makefile ../tests/tests.pro
make -j4
make check
```
