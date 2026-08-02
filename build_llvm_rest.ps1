$ErrorActionPreference = "Continue"
$emcc = "D:\project\emsdk-3.1.69\upstream\emscripten\emcc.bat"
$emar = "D:\project\emsdk-3.1.69\upstream\emscripten\emar.bat"
$src = @(
  "src/sn_alloc.c","src/sn_ctx.c","src/sn_value.c","src/sn_int.c","src/sn_str.c",
  "src/sn_float.c","src/sn_float_mp.c","src/sn_crypto.c","src/sn_math.c",
  "src/sn_math_soft.c","src/sn_complex.c","src/sn_rng.c","src/sn_api.c"
)

function Build-With {
  param($Name,$Compiler,$ExtraArgs,$ArCmd,$ArIsEmar)
  $odir = "build/llvm/$Name"
  New-Item -ItemType Directory -Force -Path $odir | Out-Null
  $objs = @()
  foreach ($f in $src) {
    $base = [IO.Path]::GetFileNameWithoutExtension($f)
    $o = Join-Path $odir ($base + ".o")
    $arg = @("-std=c99","-O2","-Iinclude","-Isrc") + $ExtraArgs + @("-c",$f,"-o",$o)
    Write-Host "  [$Name] $f"
    & $Compiler @arg
    if ($LASTEXITCODE -ne 0) {
      Write-Host "FAIL $Name $f code=$LASTEXITCODE"
      return $false
    }
    $objs += $o
  }
  $lib = "build/llvm/libsn-$Name.a"
  if (Test-Path $lib) { Remove-Item $lib -Force }
  if ($ArIsEmar) {
    & $ArCmd rcs $lib @objs
  } else {
    & $ArCmd rcs $lib @objs
  }
  if ($LASTEXITCODE -ne 0) {
    Write-Host "AR FAIL $Name"
    return $false
  }
  $sz = (Get-Item $lib).Length
  Write-Host "OK $Name -> $lib ($sz)"
  return $true
}

Write-Host "=== i686 via gcc -m32 ==="
[void](Build-With -Name "i686-windows" -Compiler "gcc" -ExtraArgs @("-m32") -ArCmd "ar" -ArIsEmar $false)

Write-Host "=== wasm32 via emcc ==="
[void](Build-With -Name "wasm32" -Compiler $emcc -ExtraArgs @() -ArCmd $emar -ArIsEmar $true)

Write-Host "=== wasm64 via emcc -sMEMORY64=1 ==="
[void](Build-With -Name "wasm64" -Compiler $emcc -ExtraArgs @("-sMEMORY64=1") -ArCmd $emar -ArIsEmar $true)

Write-Host "=== summary ==="
Get-ChildItem build/llvm -Filter "libsn-*.a" | Format-Table Name,Length -AutoSize
