# SuperNumber Lab web build (modular + single-html)
# Recommended: double-click / no-arg webapp\build.bat or repo root build.bat
# Default (no switches) = full: assemble + modular + smoke + single-html
# Usage:
#   build.bat
#   build.bat modular | single | smoke | clean | nosmoke
#   powershell -File webapp/build.ps1
#   powershell -File webapp/build.ps1 -SingleOnly
#   powershell -File webapp/build.ps1 -Emsdk D:\project\emsdk-3.1.69
param(
  [string]$Emsdk = $env:EMSDK,
  [switch]$SingleOnly,
  [switch]$ModularOnly,
  [switch]$SkipSmoke,
  [switch]$Clean
)

$ErrorActionPreference = "Stop"
$env:EMSDK_QUIET = '1'
$WebRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $WebRoot
Set-Location $WebRoot

function Find-Emsdk {
  param([string]$Hint)
  if ($Hint -and (Test-Path (Join-Path $Hint "emsdk_env.bat"))) { return (Resolve-Path $Hint).Path }
  if ($env:EMSDK -and (Test-Path (Join-Path $env:EMSDK "emsdk_env.bat"))) { return $env:EMSDK }
  $candidates = @(
    "D:\project\emsdk-3.1.69",
    "D:\project\emsdk",
    "C:\emsdk",
    "$env:USERPROFILE\emsdk"
  )
  foreach ($c in $candidates) {
    if ($c -and (Test-Path (Join-Path $c "emsdk_env.bat"))) { return (Resolve-Path $c).Path }
  }
  return $null
}

$em = Find-Emsdk -Hint $Emsdk
if (-not $em) {
  Write-Error "emsdk not found. Pass -Emsdk path or set EMSDK."
}

Write-Host "== SuperNumber Lab build =="
Write-Host "webapp: $WebRoot"
Write-Host "emsdk:  $em"

# Activate emsdk in this process via cmd bridge
$envBat = Join-Path $em "emsdk_env.bat"
$tmpEnv = Join-Path $env:TEMP "snlab_em_env_$PID.ps1"
cmd /c "`"$envBat`" && set" | ForEach-Object {
  if ($_ -match '^(.*?)=(.*)$') {
    $name = $matches[1]
    $val = $matches[2]
    if ($name -match '^[A-Za-z_][A-Za-z0-9_]*$') {
      Set-Item -Path "Env:$name" -Value $val
    }
  }
} | Out-Null

$emcc = Get-Command emcc -ErrorAction SilentlyContinue
if (-not $emcc) {
  Write-Error "emcc not on PATH after activating emsdk"
}
Write-Host "emcc: $($emcc.Source)"

Write-Host "== assemble app.js from js/* =="
node tools\assemble-app.js
if ($LASTEXITCODE -ne 0) { throw "assemble-app failed: $LASTEXITCODE" }


if ($Clean) {
  Remove-Item -ErrorAction SilentlyContinue snlab.mjs, snlab.wasm, shell-single.generated.html, snlab-single.html
  Write-Host "cleaned outputs"
}

function Invoke-EmccModular {
  $srcs = @(
    "$RepoRoot\src\sn_alloc.c",
    "$RepoRoot\src\sn_ctx.c",
    "$RepoRoot\src\sn_value.c",
    "$RepoRoot\src\sn_int.c",
    "$RepoRoot\src\sn_str.c",
    "$RepoRoot\src\sn_float.c",
    "$RepoRoot\src\sn_float_mp.c",
    "$RepoRoot\src\sn_crypto.c",
    "$RepoRoot\src\sn_math.c",
    "$RepoRoot\src\sn_math_soft.c",
    "$RepoRoot\src\sn_complex.c",
    "$RepoRoot\src\sn_rng.c",
    "$RepoRoot\src\sn_tensor.c",
    "$RepoRoot\src\sn_api.c",
    "$RepoRoot\web\sn_web_bridge.c"
  )
  $exported = "_snw_create,_snw_destroy,_snw_last_error,_snw_clear_error,_snw_set_format,_snw_new_value,_snw_free_value,_snw_set_f64,_snw_set_from_str,_snw_set_i64,_snw_set_int,_snw_set_float,_snw_set_i64_width,_snw_shift,_snw_crypto,_snw_copy,_snw_cast,_snw_seed_rng,_snw_random_u64,_snw_random_u64_mod,_snw_to_str,_snw_free_str,_snw_to_f64,_snw_kind,_snw_flags,_snw_clear_flags,_snw_unary,_snw_binary,_snw_ternary,_snw_cmp,_snw_version,_snw_new_cplx,_snw_free_cplx,_snw_set_cplx_d,_snw_cplx_set_reim,_snw_cplx_to_str,_snw_cplx_unary,_snw_cplx_binary,_snw_cplx_abs,_snw_cplx_arg,_snw_cplx_re,_snw_cplx_im,_snw_cplx_from_polar,_snw_new_tensor,_snw_free_tensor,_snw_tensor_from_str,_snw_tensor_to_str,_snw_tensor_dims,_snw_tensor_copy,_snw_tensor_unary,_snw_tensor_binary,_snw_tensor_scale,_snw_tensor_rms_norm,_snw_tensor_layer_norm,_snw_tensor_sin_pe,_snw_tensor_rope,_snw_tensor_reshape,_snw_tensor_attention_sdp,_snw_tensor_get_f64,_malloc,_free"
  $common = @(
    "-std=c99", "-O2",
    "-I$RepoRoot\include", "-I$RepoRoot\src",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sINVOKE_RUN=0",
    "-sEXIT_RUNTIME=0",
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,getValue,setValue",
    "-sEXPORTED_FUNCTIONS=$exported"
  )
  Write-Host "== modular snlab.mjs =="
  & emcc @srcs @common `
    "-sMODULARIZE=1" `
    "-sEXPORT_NAME=createSnLab" `
    "-sENVIRONMENT=web,worker,node" `
    "-o" "snlab.mjs"
  if ($LASTEXITCODE -ne 0) { throw "emcc modular failed: $LASTEXITCODE" }
}

function Invoke-EmccSingle {
  Write-Host "== shell-single.generated.html =="
  node tools\build-single-shell.js
  if ($LASTEXITCODE -ne 0) { throw "build-single-shell failed" }

  $srcs = @(
    "$RepoRoot\src\sn_alloc.c",
    "$RepoRoot\src\sn_ctx.c",
    "$RepoRoot\src\sn_value.c",
    "$RepoRoot\src\sn_int.c",
    "$RepoRoot\src\sn_str.c",
    "$RepoRoot\src\sn_float.c",
    "$RepoRoot\src\sn_float_mp.c",
    "$RepoRoot\src\sn_crypto.c",
    "$RepoRoot\src\sn_math.c",
    "$RepoRoot\src\sn_math_soft.c",
    "$RepoRoot\src\sn_complex.c",
    "$RepoRoot\src\sn_rng.c",
    "$RepoRoot\src\sn_tensor.c",
    "$RepoRoot\src\sn_api.c",
    "$RepoRoot\web\sn_web_bridge.c"
  )
  $exported = "_snw_create,_snw_destroy,_snw_last_error,_snw_clear_error,_snw_set_format,_snw_new_value,_snw_free_value,_snw_set_f64,_snw_set_from_str,_snw_set_i64,_snw_set_int,_snw_set_float,_snw_set_i64_width,_snw_shift,_snw_crypto,_snw_copy,_snw_cast,_snw_seed_rng,_snw_random_u64,_snw_random_u64_mod,_snw_to_str,_snw_free_str,_snw_to_f64,_snw_kind,_snw_flags,_snw_clear_flags,_snw_unary,_snw_binary,_snw_ternary,_snw_cmp,_snw_version,_snw_new_cplx,_snw_free_cplx,_snw_set_cplx_d,_snw_cplx_set_reim,_snw_cplx_to_str,_snw_cplx_unary,_snw_cplx_binary,_snw_cplx_abs,_snw_cplx_arg,_snw_cplx_re,_snw_cplx_im,_snw_cplx_from_polar,_snw_new_tensor,_snw_free_tensor,_snw_tensor_from_str,_snw_tensor_to_str,_snw_tensor_dims,_snw_tensor_copy,_snw_tensor_unary,_snw_tensor_binary,_snw_tensor_scale,_snw_tensor_rms_norm,_snw_tensor_layer_norm,_snw_tensor_sin_pe,_snw_tensor_rope,_snw_tensor_reshape,_snw_tensor_attention_sdp,_snw_tensor_get_f64,_malloc,_free"
  $common = @(
    "-std=c99", "-O2",
    "-I$RepoRoot\include", "-I$RepoRoot\src",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sINVOKE_RUN=0",
    "-sEXIT_RUNTIME=0",
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,getValue,setValue",
    "-sEXPORTED_FUNCTIONS=$exported"
  )
  Write-Host "== snlab-single.html =="
  & emcc @srcs @common `
    "-sWASM=0" `
    "-sSINGLE_FILE=1" `
    "-sENVIRONMENT=web" `
    "--shell-file" "shell-single.generated.html" `
    "-o" "snlab-single.html"
  if ($LASTEXITCODE -ne 0) { throw "emcc single-html failed: $LASTEXITCODE" }
}

if (-not $SingleOnly) {
  Invoke-EmccModular
  if (-not $SkipSmoke) {
    Write-Host "== smoke =="
    node _smoke.mjs
    if ($LASTEXITCODE -ne 0) { throw "smoke failed" }
  }
}

if (-not $ModularOnly) {
  Invoke-EmccSingle
}

Write-Host "OK: build finished"
Get-Item snlab.mjs, snlab.wasm, snlab-single.html -ErrorAction SilentlyContinue | Format-Table Name, Length, LastWriteTime
