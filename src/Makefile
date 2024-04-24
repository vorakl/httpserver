#
# Copyright (c) 2023 vorakl
# SPDX-License-Identifier: Apache-2.0
#

IDIR=include
ODIR=obj
BDIR=bin
CXX=$(shell which g++)
CFLAGS=-std=c++17 -O2 -Wall -I$(IDIR)
LIBS=

_DEPS=common.hpp httpserver.hpp
DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ=common.o httpserver.o main.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

.MAIN: build

build: $(BDIR)/httpserver

$(BDIR)/httpserver: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

$(ODIR)/%.o: %.cpp $(DEPS)
	$(CXX) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~ $(BDIR)/httpserver

.PHONY: help
help:
	@echo ''
	@echo 'Usage: make [build|clean]'
	@echo '   make build        build httpserver into bin/'
	@echo '   make clean        clean up all temporary files'


