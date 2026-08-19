#!/usr/bin/env bash
# Build and run the IDE test suite against the fetched compiler and solvers.
#
# Env: ROOT, MAKE
set -eux

ROOT="${ROOT:-$PWD}"
cd "$ROOT"

export PATH="$ROOT/minizinc/bin:$PATH"
export MZN_SOLVER_PATH="$ROOT/vendor/gecode_gist/share/minizinc/solvers/:$ROOT/vendor/chuffed/share/minizinc/solvers"
minizinc --solvers

mkdir -p test
cd test
qmake -makefile "$ROOT/tests/tests.pro"
"${MAKE:-make}" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
"${MAKE:-make}" check
