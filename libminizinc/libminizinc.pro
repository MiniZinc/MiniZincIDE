TEMPLATE = lib

CONFIG += c++11 staticlib

TARGET = minizinc

HEADERS += \
  ../libminizinc_src/include/minizinc/ast.hh
  ../libminizinc_src/include/minizinc/ast.hpp
  ../libminizinc_src/include/minizinc/astexception.hh
  ../libminizinc_src/include/minizinc/astiterator.hh
  ../libminizinc_src/include/minizinc/aststring.hh
  ../libminizinc_src/include/minizinc/astvec.hh
  ../libminizinc_src/include/minizinc/builtins.hh
  ../libminizinc_src/include/minizinc/cli.hh
  ../libminizinc_src/include/minizinc/config.hh.in
  ../libminizinc_src/include/minizinc/copy.hh
  ../libminizinc_src/include/minizinc/eval_par.hh
  ../libminizinc_src/include/minizinc/exception.hh
  ../libminizinc_src/include/minizinc/file_utils.hh
  ../libminizinc_src/include/minizinc/flatten.hh
  ../libminizinc_src/include/minizinc/flatten_internal.hh
  ../libminizinc_src/include/minizinc/flattener.hh
  ../libminizinc_src/include/minizinc/gc.hh
  ../libminizinc_src/include/minizinc/hash.hh
  ../libminizinc_src/include/minizinc/htmlprinter.hh
  ../libminizinc_src/include/minizinc/iter.hh
  ../libminizinc_src/include/minizinc/model.hh
  ../libminizinc_src/include/minizinc/optimize.hh
  ../libminizinc_src/include/minizinc/optimize_constraints.hh
  ../libminizinc_src/include/minizinc/options.hh
  ../libminizinc_src/include/minizinc/parser.hh
  ../libminizinc_src/include/minizinc/prettyprinter.hh
  ../libminizinc_src/include/minizinc/solver.hh
  ../libminizinc_src/include/minizinc/solver_instance.hh
  ../libminizinc_src/include/minizinc/solver_instance_base.hh
  ../libminizinc_src/include/minizinc/statistics.hh
  ../libminizinc_src/include/minizinc/stl_map_set.hh
  ../libminizinc_src/include/minizinc/timer.hh
  ../libminizinc_src/include/minizinc/type.hh
  ../libminizinc_src/include/minizinc/typecheck.hh
  ../libminizinc_src/include/minizinc/values.hh
  ../libminizinc_src/include/minizinc/stl_map_set.hh
  ../libminizinc_src/include/minizinc/thirdparty/SafeInt3.hpp
  ../libminizinc_src/lib/cached/minizinc/parser.tab.hh

SOURCES += \
  ../libminizinc_src/lib/ast.cpp \
  ../libminizinc_src/lib/astexception.cpp \
  ../libminizinc_src/lib/aststring.cpp \
  ../libminizinc_src/lib/astvec.cpp \
  ../libminizinc_src/lib/builtins.cpp \
  ../libminizinc_src/lib/cli.cpp \
  ../libminizinc_src/lib/copy.cpp \
  ../libminizinc_src/lib/eval_par.cpp \
  ../libminizinc_src/lib/file_utils.cpp \
  ../libminizinc_src/lib/gc.cpp \
  ../libminizinc_src/lib/htmlprinter.cpp \
  ../libminizinc_src/lib/model.cpp \
  ../libminizinc_src/lib/prettyprinter.cpp \
  ../libminizinc_src/lib/solver.cpp \
  ../libminizinc_src/lib/solver_instance.cpp \
  ../libminizinc_src/lib/solver_instance_base.cpp \
  ../libminizinc_src/lib/typecheck.cpp \
  ../libminizinc_src/lib/flatten.cpp \
  ../libminizinc_src/lib/flattener.cpp \
  ../libminizinc_src/lib/MIPdomains.cpp \
  ../libminizinc_src/lib/optimize.cpp \
  ../libminizinc_src/lib/options.cpp \
  ../libminizinc_src/lib/optimize_constraints.cpp \
  ../libminizinc_src/lib/solns2out_class.cpp \
  ../libminizinc_src/lib/statistics.cpp \
  ../libminizinc_src/lib/values.cpp \
  ../libminizinc_src/lib/cached/parser.tab.cpp \
  ../libminizinc_src/lib/cached/lexer.yy.cpp
  
INCLUDEPATH += ../libminizinc_src/include ../libminizinc_src/lib/cached .

DEFINES += "MZN_VERSION_MAJOR=2" "MZN_VERSION_MINOR=0" "MZN_VERSION_PATCH=14"

unix {
  DEFINES += HAS_ATTR_THREAD HAS_PIDPATH
}

win32 {
}
