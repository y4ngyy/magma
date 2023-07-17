# Local Compile Deug

## Driver compile

SymCC

```bash
export SYMCC_REGULAR_LIBCXX=1
/home/yy/work/Hybrid-ASan/qsym++-wrapper -c -fPIC /home/yy/work/fuzz-experiments/magma/fuzzers/symcc_analysis/src/driver.cpp -o /home/yy/work/fuzz-experiments/magma-target/libpng/fuzz/driver.o
```

SymSan

```bash
export KO_CC=clang-12 KO_CXX=clang++-12 KO_USE_NATIVE_LIBCXX=1 KO_USE_Z3=1 KO_USE_SANITIZER=1 
/home/yy/work/Hybrid-ASan/build-symsan/bin/ko-clang++ -c -fPIC /home/yy/work/fuzz-experiments/magma/fuzzers/symcc_analysis/src/driver.cpp -o /home/yy/work/fuzz-experiments/magma-target/libpng/symsan-fuzz/driver.o
```

## libpng

### compile zlib

SymCC

```bash
CC=/home/yy/work/Hybrid-ASan/qsymcc-wrapper ./configure
make
```

SymSan

```bash
export KO_CC=clang-12 KO_CXX=clang++-12 KO_USE_NATIVE_LIBCXX=1 KO_USE_Z3=1 KO_USE_SANITIZER=1 
CC=/home/yy/work/Hybrid-ASan/build-symsan/bin/ko-clang ./configure --static
make
```

### compile libpng

SymCC

```bash
export SYMCC_REGULAR_LIBCXX=1
autoreconf -f -i
CC=/home/yy/work/Hybrid-ASan/qsymcc-wrapper ./configure --disable-shared
make clean
make -j2 libpng16.la
cp .libs/libpng16.a fuzz/
```

compile libpng_read_fuzzer driver

```bash
/home/yy/work/Hybrid-ASan/qsym++-wrapper contrib/oss-fuzz/libpng_read_fuzzer.cc \
-o fuzz/libpng_read_fuzzer \
-L fuzz/ .libs/libpng16.a -l:driver.o \
-Wl,-rpath /home/yy/work/fuzz-experiments/magma-target/dependencies/zlib-1.2.13 -lz

/home/yy/work/Hybrid-ASan/qsymcc-wrapper contrib/oss-fuzz/libpng_read_fuzzer.c \
-o fuzz/libpng_read_fuzzer \
-L fuzz/ .libs/libpng16.a -l:driver.o \
-Wl,-rpath /home/yy/work/fuzz-experiments/magma-target/dependencies/zlib-1.2.13 -lz -lm
```

SymSan

```bash
export export KO_CC=clang-12 KO_CXX=clang++-12 KO_USE_NATIVE_LIBCXX=1 KO_USE_Z3=1 KO_USE_SANITIZER=1 
/home/yy/work/Hybrid-ASan/build-symsan/bin/ko-clang contrib/oss-fuzz/libpng_read_fuzzer.c \
-o symsan-fuzz/libpng_read_fuzzer \
-L symsan-fuzz/ .libs/libpng16.a -l:driver.o /home/yy/work/fuzz-experiments/magma-target/dependencies/zlib-1.2.13/libz.a
```

SymSan Environment

```bash
export TAINT_OPTIONS=taint_file=/home/yy/work/fuzz-experiments/magma/targets/libpng/corpus/libpng_read_fuzzer/not_kitty.png:output_dir=output
./libpng_read_fuzzer /home/yy/work/fuzz-experiments/magma/targets/libpng/corpus/libpng_read_fuzzer/not_kitty.png
```
