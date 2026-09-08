# Headless glue benchmark: quadtree in script vs C++ QuadTree driven from script vs pure C++.
CXX      ?= g++
CXXFLAGS ?= -O2 -std=gnu++17 -Wall -Wno-unused-parameter
ZEN_ROOT  ?= ../../..
ZEN_BUILD ?= $(ZEN_ROOT)/build_release
ZEN_INC   ?= $(ZEN_ROOT)/libzen/include/zen
ZEN_LIB   ?= $(ZEN_BUILD)/libzen/libzen.a
LUA_INC ?= /usr/local/include
LUA_LIB ?= /usr/local/lib/liblua.a
WREN_DIR ?= /media/projectos/projects/languages/wren-0.4.0
WREN_INC ?= $(WREN_DIR)/src/include
WREN_LIB ?= $(WREN_DIR)/lib/libwren.a

all: glue_cpp glue_zen glue_lua glue_wren

glue_cpp: glue_cpp.cpp qtree.h bench.h
	$(CXX) $(CXXFLAGS) glue_cpp.cpp -o $@
glue_zen: glue_zen.cpp qtree.h bench.h $(ZEN_LIB)
	$(CXX) $(CXXFLAGS) -I$(ZEN_INC) glue_zen.cpp $(ZEN_LIB) -lm -lpthread -ldl -o $@
glue_lua: glue_lua.cpp qtree.h bench.h
	$(CXX) $(CXXFLAGS) -I$(LUA_INC) glue_lua.cpp $(LUA_LIB) -lm -ldl -o $@
glue_wren: glue_wren.cpp qtree.h bench.h
	$(CXX) $(CXXFLAGS) -I$(WREN_INC) glue_wren.cpp $(WREN_LIB) -lm -o $@

clean:
	rm -f glue_cpp glue_zen glue_lua glue_wren
.PHONY: all clean
