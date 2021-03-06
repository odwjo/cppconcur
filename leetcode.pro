TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    regexfordotandstar.cpp \
    helloccw.cpp \
    share_data_between_thrds.cpp \
    use_boost_shared_mutex.cpp \
    synchronizing_concurrent.cpp \
    use_futures.cpp \
    using_std_packged_task.cpp \
    functional_programming.cpp \
    use_atomic.cpp \
    test_and_debug.cpp

LIBS += -lpthread\
    -L/usr/lib/x86_64-linux-gnu -lboost_system


HEADERS += \
    cppconcurrent.h \
    functional_programming \
    threadsafe_queue.h \
    threadsafe_lookup_table.h \
    threadsafe_list.h \
    threadsafe_datastructure_nolocks.h \
    hazard_pointer.h \
    concurrent_code.h \
    threadsafe_stack_usemutex.h \
    thread_pool.h

