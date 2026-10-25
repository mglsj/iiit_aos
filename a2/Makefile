CMAKE := cmake -DCMAKE_C_COMPILER=gcc

build:
	$(CMAKE) -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=address
	cmake --build build/asan -j

build-release:
	$(CMAKE) -S . -B build/release -DCMAKE_BUILD_TYPE=Release
	cmake --build build/release -j

.PHONY: build build-release
