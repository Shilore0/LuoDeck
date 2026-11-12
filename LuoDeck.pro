QT += core gui widgets network

CONFIG += c++17
TEMPLATE = app
TARGET = LuoDeck

# 语义化版本，写进 exe 的产品元数据；界面显示的版本号在 src/version.h
VERSION = 1.0.0
QMAKE_TARGET_COMPANY = "LuoDeck"
QMAKE_TARGET_PRODUCT = "LuoDeck"
QMAKE_TARGET_DESCRIPTION = "Windows 图标启动面板"
QMAKE_TARGET_COPYRIGHT = "MIT License"

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/canvaswidget.cpp \
    src/itemwidget.cpp \
    src/appitemwidget.cpp \
    src/groupitemwidget.cpp \
    src/grouppopup.cpp \
    src/icontile.cpp \
    src/settingsdialog.cpp \
    src/foldersettingsdialog.cpp \
    src/shellintegration.cpp \
    src/shellselection.cpp \
    src/windowfx.cpp \
    src/iconprovider.cpp \
    src/launcher.cpp \
    src/appconfig.cpp \
    src/theme.cpp \
    src/i18n.cpp \
    src/layoutengine.cpp \
    src/hotkey.cpp \
    src/winapps.cpp \
    src/winappdialog.cpp

HEADERS += \
    src/version.h \
    src/mainwindow.h \
    src/canvaswidget.h \
    src/itemwidget.h \
    src/appitemwidget.h \
    src/groupitemwidget.h \
    src/grouppopup.h \
    src/icontile.h \
    src/settingsdialog.h \
    src/foldersettingsdialog.h \
    src/shellintegration.h \
    src/shellselection.h \
    src/windowfx.h \
    src/iconprovider.h \
    src/launcher.h \
    src/appconfig.h \
    src/theme.h \
    src/i18n.h \
    src/layoutengine.h \
    src/hotkey.h \
    src/metrics.h \
    src/winapps.h \
    src/winappdialog.h

win32 {
    LIBS += -luser32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -lgdi32
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS += /utf-8
    DESTDIR = $$PWD/bin
}

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += /utf-8
    DESTDIR = $$PWD/bin
}
