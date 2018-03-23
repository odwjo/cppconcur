TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    regexfordotandstar.cpp \
    helloccw.cpp \
    share_data_between_thrds.cpp

LIBS += -lpthread

HEADERS += \
    cppconcurrent.h
