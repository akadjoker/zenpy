# Cross-language raylib quadtree demo: one binary per scripting language,
# all sharing host.cpp. Same prerequisites as ../bunnymark_raylib.
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

all: qt_zen qt_lua qt_wren

host.o: host.cpp host.h
	$(CXX) $(CXXFLAGS) -I$(RAYLIB_INC) -c host.cpp -o host.o

qt_zen: qt_zen.cpp host.o $(ZEN_LIB)
	$(CXX) $(CXXFLAGS) -I$(ZEN_INC) qt_zen.cpp host.o $(ZEN_LIB) $(RAYLIB_LIBS) -o $@

qt_lua: qt_lua.cpp host.o
	$(CXX) $(CXXFLAGS) -I$(LUA_INC) qt_lua.cpp host.o $(LUA_LIB) $(RAYLIB_LIBS) -o $@

qt_wren: qt_wren.cpp host.o
	$(CXX) $(CXXFLAGS) -I$(WREN_INC) qt_wren.cpp host.o $(WREN_LIB) $(RAYLIB_LIBS) -o $@

clean:
	rm -f host.o qt_zen qt_lua qt_wren

.PHONY: all clean
