param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [switch]$WaitForClose,
    [switch]$StallAudit,
    [switch]$ProductionDefault
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binary = (Resolve-Path -LiteralPath "$build/bin/Release/Metin2_Release.exe").Path
$target = Join-Path $source "build/f2x/runtime/$Name"
if (Test-Path -LiteralPath $target) { throw 'A fresh manual evidence directory is required.' }
New-Item -ItemType Directory -Path "$target/pack", "$target/log", "$target/mark", "$target/upload" | Out-Null
Copy-Item -LiteralPath $binary -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
foreach ($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash -ne $hash) {
    throw 'Manual copy does not match the completed Release build.'
}
$clientArguments = @('--renderer-diagnostics', '--animation-runtime=ziinan', '--gr2-reader=ziinan')
if ($ProductionDefault) { $clientArguments = @('--renderer-diagnostics') }
if ($StallAudit) { $clientArguments += '--animation-stall-audit' }
"SourceBinary=$binary`nSHA256=$hash`nArguments=$($clientArguments -join ' ')" |
    Set-Content -LiteralPath "$target/artifact.txt"
# The milestone explicitly requests an interactive native client for the visual check.
$process = Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Normal `
    -ArgumentList $clientArguments -PassThru
"PID=$($process.Id)`nRuntime=$target" | Tee-Object -FilePath "$target/manual-process.txt"
if ($WaitForClose) {
    $process.WaitForExit()
    "PID=$($process.Id) ExitCode=$($process.ExitCode)" | Tee-Object -FilePath "$target/exit.txt"
    if ($process.ExitCode -ne 0) { throw 'Manual F2-X client exited with a nonzero code.' }
    Get-Content -LiteralPath "$target/source-resource-audit.log"
}

