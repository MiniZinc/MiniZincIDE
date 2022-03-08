TEMPLATE = subdirs

SUBDIRS = \
    MiniZincIDE \
    tests

libminizinc {
  SUBDIRS += libminizinc
  MiniZincIDE.depends = libminizinc
}
