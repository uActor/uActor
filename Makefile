default: build_local

build_local:
	mkdir -p build_local_release && \
	cd build_local_release && \
	cmake -G Ninja ../main_linux && \
	ninja

build_local_debug:
	mkdir -p build_local_debug && \
	cd build_local_debug && \
	cmake -G Ninja ../main_linux -DCMAKE_BUILD_TYPE=Debug && \
	ninja

test:
	mkdir -p build_local_test && \
	cd build_local_test && \
	cmake -G Ninja ../unit_test && \
	ninja && \
	./uActor_test

CODE_FILES = $(shell find main_linux uActor components/ble_actor main -name '*.*pp')
lint:
	cpplint --recursive --root=. --filter -legal,-build/c++11,-whitespace/braces,-build/include_order $(CODE_FILES)

format:
	clang-format --style=Google -i $(CODE_FILES)

tidy: format lint