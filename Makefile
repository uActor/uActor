default: build_local

build_local:
	mkdir -p build/local/release/default/ && \
	cd build/local/release/default && \
	cmake -G Ninja ../../../../uActor-POSIX && \
	ninja

build_local_debug:
	mkdir -p build/local/release/default/ && \
	cd build/local/release/default && \
	cmake -G Ninja ../../../../uActor-POSIX -DCMAKE_BUILD_TYPE=Debug && \
	ninja

build_esp32:
	mkdir -p build/esp32/release/default/ && \
	idf.py -C uActor-ESP32 -B build/esp32/release/default build

test:
	mkdir -p build/test/ && \
	cd build/test && \
	cmake -G Ninja ../../unit_test && \
	ninja && \
	./uActor_test

GENERIC_CODE_FILES = $(shell find uActor -name '*.*pp')
ESP32_CODE_FILES = $(shell find uActor-ESP32/main uActor-ESP32/components/ble_actor uActor-ESP32/components/epaper_actor uActor-ESP32/components/bmp180_actor uActor-ESP32/components/bme280_actor -name '*.*pp')
POSIX_CODE_FILES = $(shell find uActor-POSIX -name '*.*pp') 
CODE_FILES = $(GENERIC_CODE_FILES) $(ESP32_CODE_FILES) $(POSIX_CODE_FILES)

lint:
	cpplint --recursive --root=. --filter -legal,-build/c++11,-whitespace/braces,-build/include_order $(CODE_FILES)

format:
	clang-format --style=Google -i $(CODE_FILES)

tidy: format lint