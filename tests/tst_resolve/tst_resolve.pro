include(../common.pri)
include(../../config.pri)
TARGET = tst_resolve
QT += contacts-private dbus

PKGCONFIG += mlocale5
LIBS += -lphonenumber

# We need the moc output for ContactManagerEngine from sqlite-extensions
extensionsIncludePath = $$system(pkg-config --cflags-only-I qtcontacts-sqlite-qt5-extensions)
VPATH += $$replace(extensionsIncludePath, -I, )
HEADERS += contactmanagerengine.h

HEADERS += ../../src/seasidecache.h
SOURCES += ../../src/seasidecache.cpp

HEADERS += ../../src/cacheconfiguration.h
SOURCES += ../../src/cacheconfiguration.cpp

SOURCES += tst_resolve.cpp
