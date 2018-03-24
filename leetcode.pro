TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    regexfordotandstar.cpp \
    helloccw.cpp \
    share_data_between_thrds.cpp \
    use_boost_shared_mutex.cpp

LIBS += -lpthread\
    -L/usr/lib/x86_64-linux-gnu -lboost_system


HEADERS += \
    cppconcurrent.h

