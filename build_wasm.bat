@echo off
setlocal EnableDelayedExpansion
call D:\project\emsdk-3.1.69\emsdk_env.bat >nul 2>&1
set CFLAGS=-std=c99 -O2 -Iinclude -Isrc

echo === wasm32 ===
if not exist build\llvm\wasm32 mkdir build\llvm\wasm32
for %%f in (sn_alloc sn_ctx sn_value sn_int sn_str sn_float sn_float_mp sn_crypto sn_math sn_math_soft sn_complex sn_rng sn_tensor sn_api) do (
  echo   [wasm32] %%f
  emcc %CFLAGS% -c src\%%f.c -o build\llvm\wasm32\%%f.o
  if errorlevel 1 exit /b 1
)
if exist build\llvm\libsn-wasm32.a del build\llvm\libsn-wasm32.a
emar rcs build\llvm\libsn-wasm32.a build\llvm\wasm32\sn_alloc.o build\llvm\wasm32\sn_ctx.o build\llvm\wasm32\sn_value.o build\llvm\wasm32\sn_int.o build\llvm\wasm32\sn_str.o build\llvm\wasm32\sn_float.o build\llvm\wasm32\sn_float_mp.o build\llvm\wasm32\sn_crypto.o build\llvm\wasm32\sn_math.o build\llvm\wasm32\sn_math_soft.o build\llvm\wasm32\sn_complex.o build\llvm\wasm32\sn_rng.o build\llvm\wasm32\sn_tensor.o build\llvm\wasm32\sn_api.o
if errorlevel 1 exit /b 1
echo OK wasm32

echo === wasm64 ===
if not exist build\llvm\wasm64 mkdir build\llvm\wasm64
for %%f in (sn_alloc sn_ctx sn_value sn_int sn_str sn_float sn_float_mp sn_crypto sn_math sn_math_soft sn_complex sn_rng sn_tensor sn_api) do (
  echo   [wasm64] %%f
  emcc %CFLAGS% -sMEMORY64=1 -c src\%%f.c -o build\llvm\wasm64\%%f.o
  if errorlevel 1 exit /b 1
)
if exist build\llvm\libsn-wasm64.a del build\llvm\libsn-wasm64.a
emar rcs build\llvm\libsn-wasm64.a build\llvm\wasm64\sn_alloc.o build\llvm\wasm64\sn_ctx.o build\llvm\wasm64\sn_value.o build\llvm\wasm64\sn_int.o build\llvm\wasm64\sn_str.o build\llvm\wasm64\sn_float.o build\llvm\wasm64\sn_float_mp.o build\llvm\wasm64\sn_crypto.o build\llvm\wasm64\sn_math.o build\llvm\wasm64\sn_math_soft.o build\llvm\wasm64\sn_complex.o build\llvm\wasm64\sn_rng.o build\llvm\wasm64\sn_tensor.o build\llvm\wasm64\sn_api.o
if errorlevel 1 exit /b 1
echo OK wasm64

dir build\llvm\libsn-*.a
exit /b 0
