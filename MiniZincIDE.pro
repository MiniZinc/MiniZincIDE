TEMPLATE = subdirs

SUBDIRS = MiniZincIDE

libminizinc {
  SUBDIRS += libminizinc
  MiniZincIDE.depends = libminizinc
}
