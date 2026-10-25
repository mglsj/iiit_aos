# Build:  make            (optimised ./mygit)
#         make debug      (-O0 -g with AddressSanitizer + UBSan)
#         make clean
# Run inside the devenv shell (provides GCC 16, OpenSSL and zlib).

CXX      ?= g++
CXXFLAGS ?= -O2
override CPPFLAGS += -Isrc -MMD -MP
CXXSTD   := -std=c++26
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
LDLIBS   := -lcrypto -lz

BUILD_DIR := build/obj
TARGET    := mygit

SOURCES := $(shell find src -name '*.cpp')
OBJECTS := $(SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)


.PHONY: all debug clean

all: $(TARGET)

debug:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-O0 -g -fsanitize=address,undefined" \
	        LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $(CPPFLAGS) $(WARNINGS) -c $< -o $@


clean:
	rm -rf build/obj $(TARGET)

-include $(OBJECTS:.o=.d)
