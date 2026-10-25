CMAKE := cmake -DCMAKE_CXX_COMPILER=g++

build:
	$(CMAKE) -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=address,undefined
	cmake --build build/asan -j
	$(CMAKE) -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=thread
	cmake --build build/tsan -j

build-release:
	$(CMAKE) -S . -B build/release -DCMAKE_BUILD_TYPE=Release
	cmake --build build/release -j

.PHONY: build build-release
