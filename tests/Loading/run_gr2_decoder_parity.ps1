param([Parameter(Mandatory=$true)][string]$CorpusList,
      [Parameter(Mandatory=$true)][string]$OutputDirectory,
      [string]$Baseline='5563d78')
# Run in a VS x64 developer shell. Only reads source/assets; all generated
# baseline code, object files and results go into a fresh build directory.
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$list=(Resolve-Path -LiteralPath $CorpusList).Path
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh output directory required'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path "$output/reference" | Out-Null
foreach($name in 'GR2File.h','GR2File.cpp','GR2Compression.cpp') {
    $content=& git -C $source show "${Baseline}:src/AssetRuntime/GR2/$name"
    if($LASTEXITCODE -ne 0){throw 'Cannot read baseline revision'}
    [IO.File]::WriteAllLines("$output/reference/$name",$content,[Text.UTF8Encoding]::new($false))
}
& cl /nologo /std:c++20 /O2 /EHsc "/I$source/src" "/I$output/reference" `
    "$PSScriptRoot/gr2_decoder_parity.cpp" "/Fe:$output/parity.exe" "/Fo:$output/parity.obj" *> "$output/build.log"
if($LASTEXITCODE -ne 0){throw 'Parity test build failed'}
& "$output/parity.exe" $list *> "$output/result.txt"
if($LASTEXITCODE -ne 0){throw (Get-Content -LiteralPath "$output/result.txt" -Raw)}
Get-Content -LiteralPath "$output/result.txt"
