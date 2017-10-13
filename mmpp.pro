
# Comment this to enable QT
#CONFIG -= qt

CONFIG(qt) {
    QT       += core gui
    greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
    DEFINES += USE_QT
}

TEMPLATE = app
CONFIG += link_pkgconfig
#CONFIG += object_parallel_to_source
PKGCONFIG += libmicrohttpd libcrypto++

SOURCES += \
    main.cpp \
    library.cpp \
    proof.cpp \
    old/unification.cpp \
    wff.cpp \
    toolbox.cpp \
    test.cpp \
    utils/utils.cpp \
    web/httpd.cpp \
    web/web.cpp \
    platform.cpp \
    web/workset.cpp \
    web/jsonize.cpp \
    z3/z3prover.cpp \
    reader.cpp \
    main_unification.cpp \
    temp.cpp \
    optimize.cpp

CONFIG(qt) {
SOURCES += \
    qt/mainwindow.cpp \
    qt/prooftreemodel.cpp \
    qt/htmldelegate.cpp \
    qt/main_qt.cpp
}

# Experiments with clang
#QMAKE_CC = clang
#QMAKE_CXX = clang++
#QMAKE_LINK = clang++
#QMAKE_LIBS += -ldl -rdynamic -lboost_system -lboost_filesystem -lpthread -fsanitize=undefined

# Experiments with undefined behavior checking
#QMAKE_CFLAGS += -fsanitize=undefined -fno-sanitize-recover=all
#QMAKE_CXXFLAGS += -std=c++17 -march=native -fsanitize=undefined -fno-sanitize-recover=all
#QMAKE_LIBS += -ldl -export-dynamic -rdynamic -lboost_system -lboost_filesystem -lpthread -fsanitize=undefined -fno-sanitize-recover=all

QMAKE_CFLAGS += -std=c11 -march=native -g
QMAKE_CXXFLAGS += -std=c++17 -march=native -g -ftemplate-backtrace-limit=0
QMAKE_LIBS += -ldl -export-dynamic -rdynamic -lboost_system -lboost_filesystem -lboost_serialization -lpthread -lz3

HEADERS += \
    wff.h \
    library.h \
    proof.h \
    old/unification.h \
    toolbox.h \
    parsing/earley.h \
    utils/stringcache.h \
    test.h \
    utils/utils.h \
    web/httpd.h \
    web/web.h \
    libs/json.h \
    platform.h \
    web/workset.h \
    web/jsonize.h \
    z3/z3prover.h \
    parsing/lr.h \
    reader.h \
    parsing/parser.h \
    libs/serialize_tuple.h \
    parsing/unif.h

CONFIG(qt) {
HEADERS += \
    qt/mainwindow.h \
    qt/prooftreemodel.h \
    qt/htmldelegate.h
}

DISTFILES += \
    README

FORMS += \
    qt/mainwindow.ui

create_links.commands = for i in mmpp_verify_one mmpp_verify_all mmpp_test_setmm mmpp_unificator webmmpp qmmpp mmpp_test_z3 mmpp_generalizable_theorems ; do ln -s mmpp \$\$i 2>/dev/null || true ; done
QMAKE_EXTRA_TARGETS += create_links
POST_TARGETDEPS += create_links
