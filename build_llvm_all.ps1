$clang = "D:\project\emsdk-3.1.69\upstream\bin\clang.exe"
$llvm_ar = "D:\project\emsdk-3.1.69\upstream\bin\llvm-ar.exe"
$src = @(
  "src/sn_alloc.c","src/sn_ctx.c","src/sn_value.c","src/sn_int.c","src/sn_str.c",
  "src/sn_float.c","src/sn_float_mp.c","src/sn_crypto.c","src/sn_math.c",
  "src/sn_math_soft.c","src/sn_complex.c","src/sn_rng.c","src/sn_api.c"
)
$targets = @(
  @{ name="x86_64-windows"; triple="x86_64-pc-windows-gnu"; flags=@("-std=c99","-O2","-Iinclude","-Isrc") },
  @{ name="i686-windows";   triple="i686-pc-windows-gnu";   flags=@("-std=c99","-O2","-Iinclude","-Isrc") },
  @{ name="wasm32";         triple="wasm32-unknown-unknown"; flags=@("-std=c99","-O2","-Iinclude","-Isrc","-ffreestanding","-fno-builtin") },
  @{ name="wasm64";         triple="wasm64-unknown-unknown"; flags=@("-std=c99","-O2","-Iinclude","-Isrc","-ffreestanding","-fno-builtin") }
)

New-Item -ItemType Directory -Force -Path build/llvm | Out-Null
$summary = New-Object System.Collections.Generic.List[string]

foreach ($t in $targets) {
  $odir = "build/llvm/$($t.name)"
  New-Item -ItemType Directory -Force -Path $odir | Out-Null
  $objs = New-Object System.Collections.Generic.List[string]
  $ok = $true
  $errMsg = ""
  foreach ($f in $src) {
    $base = [IO.Path]::GetFileNameWithoutExtension($f)
    $o = Join-Path $odir ($base + ".o")
    $errf = Join-Path $odir ($base + ".err")
    $outf = Join-Path $odir ($base + ".out")
    $argList = @("-target", $t.triple) + $t.flags + @("-c", $f, "-o", $o)
    $p = Start-Process -FilePath $clang -ArgumentList $argList -NoNewWindow -Wait -PassThru -RedirectStandardError $errf -RedirectStandardOutput $outf
    if ($p.ExitCode -ne 0) {
      $ok = $false
      $errMsg = Get-Content $errf -Raw -ErrorAction SilentlyContinue
      break
    }
    $objs.Add($o) | Out-Null
  }
  if ($ok) {
    $lib = "build/llvm/libsn-$($t.name).a"
    if (Test-Path $lib) { Remove-Item $lib -Force }
    $arErr = Join-Path $odir "ar.err"
    $arOut = Join-Path $odir "ar.out"
    $arArgs = @("rcs", $lib) + $objs
    $p = Start-Process -FilePath $llvm_ar -ArgumentList $arArgs -NoNewWindow -Wait -PassThru -RedirectStandardError $arErr -RedirectStandardOutput $arOut
    if ($p.ExitCode -ne 0) {
      $ok = $false
      $errMsg = Get-Content $arErr -Raw -ErrorAction SilentlyContinue
      $summary.Add("FAIL $($t.name) ($($t.triple)): $errMsg") | Out-Null
    } else {
      $sz = (Get-Item $lib).Length
      $summary.Add("OK  $($t.name) ($($t.triple)) -> $lib ($sz bytes)") | Out-Null
    }
  } else {
    $summary.Add("FAIL $($t.name) ($($t.triple)): $errMsg") | Out-Null
  }
}
$summary | ForEach-Object { $_ }
Get-ChildItem build/llvm -Filter "*.a" | Format-Table Name,Length
