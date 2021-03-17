# μActor: Stateful Content-based In-Network Computing
Platform for running Actors on various Devices (from Microcontrollers to Edge and Cloud nodes based on Linux/Unix) in the Network.
These Actors are deployed by multiple tenants and are able to communicate using Content-based Networking.

## Project Structure

### Code
* `uActor`: Main platform library.
* `uActor-POSIX`: POSIX command-line utility that allows running the platform on a POSIX node (tested on Linux and macOS). Also contains POSIX-specific components and implementation.
* `uActor-ESP32`: ESP32 ESP-IDF project for running the system on an ESP32 microcontroller. Also contains ESP32-specific components.
* `vendor`: Third-party libraries.
* `unit_test`: GTest-based unit tests for the main library.


### Other
* `tools`: Tools that allow interacting with the platform nodes.
  * actorctl.py Upload a collection of deployments to one of the nodes (which will share it with its peers)
* `evaluation`: Configuration scripts and code files that act as examples and allow reproducing the results presented in the paper.

## Building
The POSIX application is built using CMake and Ninja --- other build tools should also work --- and the ESP32 binary is built using a CMake-based esp.idf project.

The repository contains multiple git submodules, which need to be properly initialized.

Please confer to the included Makefile.

## ESP32
The application needs to be configured before building using `idf.py menuconfig` (e.g. WiFi configuration, node_id, node_labels).
Then, it can be build using `idf.py build`.

### Dependencies
ESP-IDF --- please follow [Espressif's instruction](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/), or use the Docker container [espressif/esp](https://hub.docker.com/r/espressif/idf).

## POSIX
The application is configured using flags at runtime (except for compile-time flags such as the one enabling the testbed-tooling).
It can be build by creating and moving to the build directory, running `cmake`, and finally running `ninja`.

### Dependencies
* Cmake
* Ninja
* Boost

#### Debian / Ubuntu
```bash
apt-get install build-essential cmake ninja-build python3-pip
pip3 install conan
```

#### macOS
```bash
brew install cmake ninja boost
```

### Crossbuilding
Crossbuilding using cmake toolchains is supported. Prebuild toolchains can be found in the `toolchains` folder.
#### ARMv7
##### Dependencies
Debian / Ubuntu
```bash
apt-get install build-essential cmake ninja-build python3-pip crossbuild-essential-armhf g++-8-arm-linux-gnueabihf
pip3 install conan
```
##### Buidling
Debian / Ubuntu
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchains/armv7_linux.cmake
cmake --build .
```

Alternatively, an ARMv7 build can be downloaded [here](gitlab.lrz.de/cm/uactor/-/jobs/artifacts/master/download?job=build_armv7).

#### ARM64
##### Dependencies
Debian / Ubuntu
```bash
apt-get install build-essential cmake ninja-build python3-pip crossbuild-essential-arm64 g++-8-aarch64-linux-gnu
pip3 install conan
```
##### Buidling
Debian / Ubuntu
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm64_linux.cmake
cmake --build .
```

Alternatively, an ARMv7 build can be downloaded [here](gitlab.lrz.de/cm/uactor/-/jobs/artifacts/master/download?job=build_arm64).

## License and Copyright

uActor is released under the [MIT license](LICENSE.txt). Please refer to CONTRIBUTORS.md for a list of contributors and their affiliations.

uActor was originally developed and licensed under the MIT license by Raphael Hetzel.
Parts of the software where developed while being employed at the Technical University of Munich,
which now holds a copyright on this project. 