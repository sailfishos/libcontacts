include(../../config.pri)
include(../common.pri)
TARGET = tst_seasideimport

SOURCES += tst_seasideimport.cpp

equals(QT_MAJOR_VERSION, 4): LIBS += ../../src/libcontactcache.so
equals(QT_MAJOR_VERSION, 5): LIBS += ../../src/libcontactcache-qt5.so
