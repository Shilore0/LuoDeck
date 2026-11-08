QT += core gui widgets network

CONFIG += c++17
TEMPLATE = app
TARGET = LuoDeck

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/tilemodel.cpp \
    src/tiledelegate.cpp \
    src/iconprovider.cpp \
    src/launcher.cpp \
    src/appconfig.cpp \
    src/theme.cpp \
    src/i18n.cpp \
    src/hotkey.cpp

HEADERS += \
    src/mainwindow.h \
    src/tilemodel.h \
    src/tiledelegate.h \
    src/iconprovider.h \
    src/launcher.h \
    src/appconfig.h \
    src/theme.h \
    src/i18n.h \
    src/hotkey.h

win32 {
    LIBS += -luser32 -lshell32 -lole32
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS += /utf-8
    DESTDIR = $$PWD/bin
}

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += /utf-8
    DESTDIR = $$PWD/bin
}
