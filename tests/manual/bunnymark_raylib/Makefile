# Cross-language raylib bunnymark: one binary per scripting language, all
# sharing host.cpp. Adjust the paths below (or pass them on the make line)
# to where the SDKs live on this machine.
CXX      ?= g++
CXXFLAGS ?= -O2 -std=gnu++17 -Wall -Wno-unused-parameter
RAYLIB_INC  ?= /usr/local/include
RAYLIB_LIBS ?= -L/usr/local/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

ZEN_ROOT  ?= ../../..
ZEN_BUILD ?= $(ZEN_ROOT)/build_release
ZEN_INC   ?= $(ZEN_ROOT)/libzen/include/zen
ZEN_LIB   ?= $(ZEN_BUILD)/libzen/libzen.a

LUA_INC ?= /usr/local/include
LUA_LIB ?= /usr/local/lib/liblua.a

WREN_DIR ?= /media/projectos/projects/languages/wren-0.4.0
WREN_INC ?= $(WREN_DIR)/src/include
WREN_LIB ?= $(WREN_DIR)/lib/libwren.a

PY_CFLAGS ?= $(shell python3-config --includes)
PY_LIBS   ?= $(shell python3-config --ldflags --embed)

all: bunny_zen bunny_lua bunny_wren bunny_py

host.o: host.cpp host.h
	$(CXX) $(CXXFLAGS) -I$(RAYLIB_INC) -c host.cpp -o host.o

bunny_zen: bunny_zen.cpp host.o $(ZEN_LIB)
	$(CXX) $(CXXFLAGS) -I$(ZEN_INC) bunny_zen.cpp host.o $(ZEN_LIB) $(RAYLIB_LIBS) -o $@

bunny_lua: bunny_lua.cpp host.o
	$(CXX) $(CXXFLAGS) -I$(LUA_INC) bunny_lua.cpp host.o $(LUA_LIB) $(RAYLIB_LIBS) -o $@

bunny_wren: bunny_wren.cpp host.o
	$(CXX) $(CXXFLAGS) -I$(WREN_INC) bunny_wren.cpp host.o $(WREN_LIB) $(RAYLIB_LIBS) -o $@

bunny_py: bunny_py.cpp host.o
	$(CXX) $(CXXFLAGS) $(PY_CFLAGS) bunny_py.cpp host.o $(PY_LIBS) $(RAYLIB_LIBS) -o $@

clean:
	rm -f host.o bunny_zen bunny_lua bunny_wren bunny_py

.PHONY: all clean
