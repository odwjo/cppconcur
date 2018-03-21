TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    regexfordotandstar.cpp \
    helloccw.cpp

LIBS += -lpthread

HEADERS += \
    cppconcurrent.h
