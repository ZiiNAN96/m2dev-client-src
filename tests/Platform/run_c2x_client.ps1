# ZiiNAN: Platform abstraction
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [switch]$Diagnostics,
    [switch]$Smoke,
    [switch]$SetupOnly
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$binary = (Resolve-Path -LiteralPath "$source/build-c2x-clean/bin/Release/Metin2_Release.exe").Path
$target = Join-Path $source "build-c2x-clean/runtime/$Name"
if (Test-Path -LiteralPath $target) { throw 'A fresh C2-X evidence directory is required.' }
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $binary -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
foreach ($directory in @('pack', 'bgm')) {
    New-Item -ItemType Junction -Path "$target/$directory" -Target "$original/$directory" | Out-Null
}
foreach ($directory in @('log', 'mark', 'upload')) {
    New-Item -ItemType Directory -Path "$target/$directory" | Out-Null
}
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
"SourceBinary=$binary`nSHA256=$hash`nDiagnostics=$Diagnostics`nSmoke=$Smoke" |
    Set-Content -LiteralPath "$target/artifact.txt"
if ($SetupOnly) { Write-Output $target; exit 0 }
$arguments = @()
if ($Diagnostics) { $arguments += '--renderer-diagnostics' }
if ($Smoke) { $arguments += '--renderer-smoke-test' }
$start = @{FilePath="$target/Metin2_Release.exe"; WorkingDirectory=$target; WindowStyle='Normal'; PassThru=$true}
if ($arguments.Count) { $start.ArgumentList=$arguments }
$process = Start-Process @start
Write-Output "C2-X fresh Release PID=$($process.Id) Runtime=$target"
$watch = [Diagnostics.Stopwatch]::StartNew()
while (-not $process.WaitForExit(5000)) {
    $process.Refresh()
    if (-not $process.HasExited) {
        [pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1); privateMB=[math]::Round($process.PrivateMemorySize64/1MB,1); handles=$process.HandleCount} |
            Export-Csv -LiteralPath "$target/resources.csv" -NoTypeInformation -Append
    }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" |
    Tee-Object -FilePath "$target/exit.txt"
if ($process.ExitCode -ne 0) { throw 'C2-X client exited with a nonzero code.' }
