# Brownout Replay Lab

Brownout Replay Lab is a C++20 framework for deterministic replay of interrupted
NOR-flash operations and crash-consistency checks for compact diagnostic logs.
It models software-visible behaviour, not an electrical circuit or a specific
microcontroller.

## Build

Requirements: CMake 3.24 or newer and a C++20 compiler.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Sanitizer build:

```sh
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan
```

Run the CLI:

```sh
./build/debug/brlab --help
./build/debug/brlab run demo
```

The project is being implemented in verified vertical slices. The first slice
is `write -> power cut -> prefix torn write -> reboot -> recovery result`.

## Scope

Version 1.0 uses integer virtual time, explicit seeds, a byte-array NOR model,
and a deterministic prefix torn-write rule. It intentionally excludes circuit
simulation, specific MCU registers, parallel execution, GUI, and hardware-in-
the-loop.

## License

MIT.
