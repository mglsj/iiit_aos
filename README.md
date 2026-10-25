# AOS

Assignments for CS3.304 Advanced Operating Systems (Monsoon 2026, IIIT Hyderabad).

| | Assignment | Language |
|---|---|---|
| [a1](a1) | File reversal and verification | C |
| [a2](a2) | POSIX shell | C |
| [a3](a3) | Peer-to-peer file sharing | C++26 |
| [a4](a4) | Mini version control system | C++26 |

Each folder has its own `assignment_N.pdf` with the full spec.

## Build

Needs CMake and GCC 16. a2 needs readline, a4 needs OpenSSL and zlib; the `devenv.nix` in each folder provides them.

```sh
make build          # debug, with sanitizers, in build/
make build-release  # optimized, in build/release/
```

`compile_flags.txt` is written at configure time for clangd.
