CXX      ?= g++
CXXFLAGS ?= -O2
PNG_CFLAGS := $(shell pkg-config --cflags libpng)
PNG_LIBS   := $(shell pkg-config --libs libpng)
QMAKE    ?= qmake

# Command-line batch (main.cpp): libpng only
procimg: main.cpp basic_png.h filter_png.h fourier.h functions_png.h io_png.h morpho_png.h
	$(CXX) $(CXXFLAGS) $(PNG_CFLAGS) -o $@ main.cpp $(PNG_LIBS)

# Qt 5 interface: built out of tree so qmake's Makefile does not replace this one
gui:
	mkdir -p build && cd build && $(QMAKE) ../ProcesamientoDeImagenes.pro && $(MAKE)

all: procimg gui

clean:
	rm -rf procimg build

.PHONY: all gui clean
