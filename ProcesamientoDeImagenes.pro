# Qt 5 GUI: qmake && make   ->  ./procimg-gui
TEMPLATE = app
TARGET   = procimg-gui
QT      += widgets
CONFIG  += c++11 link_pkgconfig
PKGCONFIG += libpng

HEADERS  = addressbook.h finddialog.h
SOURCES  = gui_main.cpp addressbook.cpp finddialog.cpp
