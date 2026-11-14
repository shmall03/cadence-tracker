QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = cadence
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/theme.h \
    src/translator.h \
    src/vault.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/vault.cpp

RESOURCES += src/resources.qrc
