# plcstub

## A stub library for talking to the PLC.

Exposes a very small subset of the libplc interface, but returns dummy
information instead of talking to actual hardware.

## Contributing

Please run `clang-format` and kindly include a unit test (see the Testing
section below) for non-trivial changes.  Otherwise, go hog wild.

## Building

Running `make` will generate binary-compatable `libplctag.a` file in the root
directory.

## Testing

There are some test programs that link against the stub `libplctag.a` in the 
`tests/` directory.

For now, just run the tests with `for prog in $(find test -type f -perm +u+x); do $prog; done` and we will make this better shortly.
