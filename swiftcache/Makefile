.PHONY: build test run clean

build:
	cmake -S . -B build
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

run: build
	./build/SwiftCache

clean:
	rm -rf build
