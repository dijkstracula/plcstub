# plcstub

## A stub library for talking to the PLC.

Exposes a very small subset of the libplc interface, but returns dummy
information instead of talking to actual hardware.

## Contributing

Please run `clang-format` and kindly include a unit test (see the Testing
section below) for non-trivial changes.  Otherwise, go hog wild.

## Building

To build the project in a directory called `build`, from the project root directory run  these commands:
1. `mkdir -p build`
2. `cd build`
3. `cmake ..`
4. `cmake --build .`

Additional actions commonly performed:
* Install: `sudo cmake --install .`
* Run built tests: `cd test; ctest` - it should print "100% tests passed"
* Configure build with debug symbols: `cmake .. -DCMAKE_BUILD_TYPE=Debug`.
* Configure library to print debug: `cmake -DBUILD_WITH_DEBUG=ON ..`
* Build a static instead of shared library: `cmake -DBUILD_SHARED_LIBS=OFF ..`
