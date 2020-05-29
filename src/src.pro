include(../config.pri)

TEMPLATE = lib
CONFIG += qt hide_symbols
CONFIG += create_pc create_prl no_install_prl

# 'contacts' is too generic for the target name - use 'contactcache'
TARGET = $${PACKAGENAME}
target.path = $$[QT_INSTALL_LIBS]
INSTALLS += target

# This should be passed on qmake command line
isEmpty(PKGCONFIG_LIB) {
    PKGCONFIG_LIB = lib
    message("PKGCONFIG_LIB is unset, assuming $$PKGCONFIG_LIB")
}

# version for generated pkgconfig files is defined in the spec file
QMAKE_PKGCONFIG_INCDIR = $$PREFIX/include/$${PACKAGENAME}
QMAKE_PKGCONFIG_LIBDIR = $$PREFIX/$${PKGCONFIG_LIB}
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

CONFIG += link_pkgconfig
packagesExist(mlite5) {
    PKGCONFIG += mlite5
    DEFINES += HAS_MLITE
} else {
    warning("mlite not available. Some functionality may not work as expected.")
}
PKGCONFIG += mlocale5 mce qtcontacts-sqlite-qt5-extensions
LIBS += -lphonenumber

DEFINES += CONTACTCACHE_BUILD

# We need access to QtContacts private headers
QT += contacts-private

# We need the moc output for ContactManagerEngine from sqlite-extensions
extensionsIncludePath = $$system(pkg-config --cflags-only-I qtcontacts-sqlite-qt5-extensions)
VPATH += $$replace(extensionsIncludePath, -I, )
HEADERS += contactmanagerengine.h

SOURCES += \
    $$PWD/cacheconfiguration.cpp \
    $$PWD/seasidecache.cpp \
    $$PWD/seasideexport.cpp \
    $$PWD/seasideimport.cpp \
    $$PWD/seasidecontactbuilder.cpp \
    $$PWD/seasidepropertyhandler.cpp

HEADERS += \
    $$PWD/cacheconfiguration.h \
    $$PWD/contactcacheexport.h \
    $$PWD/seasidecache.h \
    $$PWD/seasideexport.h \
    $$PWD/seasideimport.h \
    $$PWD/seasidecontactbuilder.h \
    $$PWD/synchronizelists.h \
    $$PWD/seasidepropertyhandler.h

headers.files = \
    $$PWD/cacheconfiguration.h \
    $$PWD/contactcacheexport.h \
    $$PWD/seasidecache.h \
    $$PWD/seasideexport.h \
    $$PWD/seasideimport.h \
    $$PWD/seasidecontactbuilder.h \
    $$PWD/synchronizelists.h \
    $$PWD/seasidepropertyhandler.h
headers.path = $$PREFIX/include/$$TARGET
INSTALLS += headers
