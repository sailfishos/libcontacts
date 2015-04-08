TEMPLATE = subdirs
SUBDIRS = src tests
OTHER_FILES += rpm/libcontacts-qt5.spec

tests.depends = src
