CXX      ?= g++
CXXFLAGS ?= -O2
PNG_CFLAGS := $(shell pkg-config --cflags libpng)
PNG_LIBS   := $(shell pkg-config --libs libpng)
QMAKE    ?= qmake
HEADERS  := basic_png.h filter_png.h fourier.h functions_png.h io_png.h morpho_png.h

# Command-line batch (main.cpp): libpng only
procimg: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PNG_CFLAGS) -o $@ main.cpp $(PNG_LIBS)

# Qt 5 interface: built out of tree so qmake's Makefile does not replace this one
gui:
	mkdir -p build && cd build && $(QMAKE) ../ProcesamientoDeImagenes.pro && $(MAKE)

all: procimg gui

tests/test_filters: tests/test_filters.cpp $(HEADERS)
	$(CXX) -std=c++11 $(CXXFLAGS) $(PNG_CFLAGS) -o $@ tests/test_filters.cpp $(PNG_LIBS)

# unit tests write a few files, so they run in tests/run/
test: procimg tests/test_filters
	mkdir -p tests/run
	cd tests/run && ../test_filters
	./tests/run_programs.sh

clean:
	rm -rf procimg build
	rm -f tests/test_filters tests/run/*.png tests/run/*.csv

.PHONY: all gui test clean
