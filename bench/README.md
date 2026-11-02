# stduuid benchmarks

This directory contains a small benchmark that compares the SIMD accelerated
implementation in `include/uuid.h` with the original scalar implementation.
The original header is kept, unchanged except for its namespace and include
guard, in `reference/uuid_original.h`, so both implementations are exercised by the same
test data in the same process and with the same compiler flags.

## Build

`UUID_BUILD_BENCH` defaults to `UUID_MAIN_PROJECT`, so the benchmark is built
when stduuid is the top-level project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bench/bench_stduuid
```

To only check that the SIMD and scalar implementations agree, without running
the timings, pass `--verify-only`. This is useful for slow or emulated targets
such as ARM under QEMU:

```sh
./build/bench/bench_stduuid --verify-only
```

When stduuid is used with `add_subdirectory()`, enable the benchmark explicitly
with `-DUUID_BUILD_BENCH=ON`.

The benchmark deliberately does **not** use `-march=native`: the library picks
SSE2/SSSE3/SHA at run time, which is the configuration ordinary users get. Pass
`-DUUID_BENCH_NATIVE=ON` if you also want to measure that variant.

## What is measured

* converting a uuid to text (allocating `std::string` and writing to a 36-byte
  buffer)
* parsing canonical, braced and dash-less text, plus a spelling with dashes in
  non-canonical positions (which exercises the scalar fallback)
* `is_valid_uuid`
* `is_nil`, `operator==` and `std::hash`
* the name based generator, i.e. SHA-1 (short names and a 2048-byte name)

The benchmark first verifies that both implementations produce identical
results for all of the above, then reports the best of five timed rounds.

## SIMD backends

| architecture | formatting / parsing | comparisons | SHA-1 |
|---|---|---|---|
| x86 / x86-64 | SSE2 + SSSE3 (runtime selected on GCC/Clang) | SSE2 | SHA extensions (`sha_ni`, runtime selected) |
| ARM / AArch64 | NEON | NEON | scalar |
| others | portable scalar | portable scalar | portable scalar |

Define `STDUUID_DISABLE_SIMD` to force the portable implementation everywhere.

## Cross-architecture verification

`--verify-only` is small enough to run under QEMU. With the multi-arch GCC
container images (arm64 and arm/v7), the NEON paths can be checked without a
target sysroot on the host:

```sh
mkdir -p build-arm64 build-armv7

podman run --rm --arch arm64 --security-opt label=disable \
  -v "$PWD:/src:ro" -v "$PWD/build-arm64:/out" \
  docker.io/library/gcc:14 sh -c \
  'g++ -std=c++17 -O2 -I/src/include -I/src/bench -I/src \
     /src/bench/bench.cpp -o /out/bench && /out/bench --verify-only'

podman run --rm --arch arm --variant v7 --security-opt label=disable \
  -v "$PWD:/src:ro" -v "$PWD/build-armv7:/out" docker.io/library/gcc:14 sh -c \
  'g++ -std=c++17 -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard \
     -I/src/include -I/src/bench -I/src \
     /src/bench/bench.cpp -o /out/bench && /out/bench --verify-only'
```

## Sample results

AMD Ryzen 7 5700U (Zen 2), GCC 16, `-O3`, C++17:

```
operation                                 original ns/op    SIMD ns/op     speedup
----------------------------------------------------------------------------------
to_string (std::string)                            17.27         14.06       1.23x
to_string (36 byte buffer)                          8.92          2.76       3.23x
from_string (canonical)                           158.24         11.20      14.13x
from_string (braced)                              157.64         11.40      13.83x
from_string (no dashes)                           136.95          4.77      28.74x
from_string (odd dashes, scalar)                  141.07        159.24       0.89x
is_valid_uuid                                     117.69         10.43      11.28x
is_nil (random, early exit)                         0.36          0.35       1.03x
is_nil (all nil)                                    1.78          0.70       2.56x
operator==                                          0.75          0.52       1.45x
std::hash                                           0.47          0.47       1.00x
name generator (short name)                       257.75         93.90       2.75x
name generator (2048 byte name)                  7648.42       3460.48       2.21x
----------------------------------------------------------------------------------
```

`std::hash` is unchanged and is included as a reference point.  The allocating
`to_string` is dominated by the heap allocation, which is why the buffered
variant shows a much larger gain.  The `is_nil` row with random uuids is
reported separately because the scalar loop exits at the first non-zero byte;
the SIMD check wins when the uuid really is nil (and is constant time).
Spellings with dashes in non-canonical positions deliberately fall back to the
general scalar parser, so that row is essentially unchanged.
