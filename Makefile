default: build_local


build/esp32/release/compile_commands.json: 
	mkdir -p build/esp32/release/ && \
	idf.py -C uActor-ESP32 -B build/esp32/release reconfigure

build/$(shell uname)_$(shell uname -m)/debug/compile_commands.json:
			mkdir -p build/$(shell uname)_$(shell uname -m)/debug/ && \
	cd build/$(shell uname)_$(shell uname -m)/debug && \
	cmake -G Ninja ../../../uActor-POSIX -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

build/$(shell uname)_$(shell uname -m)/release/compile_commands.json:
	mkdir -p build/$(shell uname)_$(shell uname -m)/release && \
	cd build/$(shell uname)_$(shell uname -m)/release && \
	cmake -G Ninja ../../../uActor-POSIX -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

build_local: build/$(shell uname)_$(shell uname -m)/release/compile_commands.json
	cd build/$(shell uname)_$(shell uname -m)/release && \
	ninja


build_local_debug: build/$(shell uname)_$(shell uname -m)/debug/compile_commands.json
	cd build/$(shell uname)_$(shell uname -m)/debug && \
	ninja

build_esp32: build/esp32/release/compile_commands.json
	idf.py -C uActor-ESP32 -B build/esp32/release build

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


lint: build/esp32/release/compile_commands.json 
	cpplint --recursive --root=. --filter -legal,-build/c++11,-whitespace/braces,-build/include_order $(CODE_FILES) ${POSIX_CODE_FILES}
	clang-tidy -p build/esp32/release/compile_commands.json ${ESP32_CODE_FILES}
	clang-tidy -p build/$(shell uname)_$(shell uname -m)/debug/compile_commands.json ${GENERIC_CODE_FILES} {}
format:
	clang-format --style=Google -i $(CODE_FILES)
