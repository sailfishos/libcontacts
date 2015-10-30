TEMPLATE = subdirs
SUBDIRS = src tests translations
OTHER_FILES += rpm/libcontacts-qt5.spec

tests.depends = src
