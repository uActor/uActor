.PHONY: default build_local build_local_debug build_esp32 lint_cpplint lint_clang-tidy_posix lint_clang-tidy_posix format test

default: build_local

PROJECT_HOME = ${CURDIR}
BUILD_DIRECTORY = build
ESP32_BUILD_DIRECTORY = ${BUILD_DIRECTORY}/esp32
LOCAL_BUILD_DIRECTORY = ${BUILD_DIRECTORY}/$(shell uname)_$(shell uname -m)
TEST_DIRECTORY = ${BUILD_DIRECTORY}/test


GENERIC_CODE_FILES = $(shell find uActor -name '*.*pp')
ESP32_CODE_FILES = $(shell find uActor-ESP32/main uActor-ESP32/components/ble_actor uActor-ESP32/components/epaper_actor uActor-ESP32/components/bmp180_actor uActor-ESP32/components/bme280_actor -name '*.*pp')
POSIX_CODE_FILES = $(shell find uActor-POSIX -name '*.*pp') 
CODE_FILES = $(GENERIC_CODE_FILES) $(ESP32_CODE_FILES) $(POSIX_CODE_FILES)


${ESP32_BUILD_DIRECTORY}/release/compile_commands.json: 
	mkdir -p ${ESP32_BUILD_DIRECTORY}/release/ && \
	idf.py -C uActor-ESP32 -B ${ESP32_BUILD_DIRECTORY}/release reconfigure

${LOCAL_BUILD_DIRECTORY}/debug/compile_commands.json:
	mkdir -p ${LOCAL_BUILD_DIRECTORY}/debug/ && \
	cd ${LOCAL_BUILD_DIRECTORY}/debug && \
	cmake -G Ninja ${PROJECT_HOME}/uActor-POSIX -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

${LOCAL_BUILD_DIRECTORY}/release/compile_commands.json:
	mkdir -p ${LOCAL_BUILD_DIRECTORY}/release && \
	cd ${LOCAL_BUILD_DIRECTORY}/release && \
	cmake -G Ninja ${PROJECT_HOME}/uActor-POSIX -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

build_local: ${LOCAL_BUILD_DIRECTORY}/release/compile_commands.json
	cd ${LOCAL_BUILD_DIRECTORY}/release && \
	ninja


build_local_debug: ${LOCAL_BUILD_DIRECTORY}/debug/compile_commands.json
	cd ${LOCAL_BUILD_DIRECTORY}/debug && \
	ninja

build_esp32: ${ESP32_BUILD_DIRECTORY}/release/compile_commands.json
	idf.py -C uActor-ESP32 -B ${ESP32_BUILD_DIRECTORY}/release build

test:
	mkdir -p ${TEST_DIRECTORY} && \
	cd ${TEST_DIRECTORY} && \
	cmake -G Ninja ${PROJECT_HOME}/unit_test && \
	ninja && \
	./uActor_test

lint_cpplint:
	cpplint --recursive --root=. --filter -legal,-build/c++11,-whitespace/braces,-build/include_order,-readability/nolint $(CODE_FILES) ${POSIX_CODE_FILES}

lint_clang-tidy_esp32: ${ESP32_BUILD_DIRECTORY}/release/compile_commands.json
	clang-tidy -p ${ESP32_BUILD_DIRECTORY}/release/compile_commands.json ${ESP32_CODE_FILES}

lint_clang-tidy_posix: ${LOCAL_BUILD_DIRECTORY}/debug/compile_commands.json
	clang-tidy -p ${LOCAL_BUILD_DIRECTORY}/debug/compile_commands.json ${GENERIC_CODE_FILES} ${POSIX_CODE_FILES}

lint: lint_cpplint lint_clang-tidy_posix


format:
	clang-format --style=Google -i $(CODE_FILES)
