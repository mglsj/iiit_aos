# `make` builds tracker/tracker and client/client with only g++ and make.
# The cmake targets build into build*/ for clangd and sanitizer runs.

# the code needs GCC 16 for reflection and contracts, and cmake would
# otherwise take whatever c++ is first on PATH, which may be clang
CMAKE := cmake -DCMAKE_CXX_COMPILER=g++

all: tracker client

tracker client:
	$(MAKE) -C $@

cmake:
	$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build -j

asan:
	$(CMAKE) -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_ASAN=ON
	cmake --build build-asan -j

tsan:
	$(CMAKE) -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TSAN=ON
	cmake --build build-tsan -j

test: all
	scripts/test_sync.sh

clean:
	$(MAKE) -C tracker clean
	$(MAKE) -C client clean
	rm -rf build build-asan build-tsan

.PHONY: all tracker client cmake asan tsan test clean
