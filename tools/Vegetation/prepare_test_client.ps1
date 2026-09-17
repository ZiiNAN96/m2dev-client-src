param(
    [string]$BuildDirectory = 'build',
    [string]$CompiledDirectory = 'test-data/vegetation',
    [string]$OutputDirectory = 'build/hx/final-test-client'
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$binary = (Resolve-Path -LiteralPath "$BuildDirectory/bin/Release/Metin2_Release.exe").Path
$compiled = (Resolve-Path -LiteralPath "$CompiledDirectory/vegetation").Path
$destination = [IO.Path]::GetFullPath($OutputDirectory, $source)
if (-not $destination.StartsWith("$source\build\", [StringComparison]::OrdinalIgnoreCase)) { throw 'Test client must be in the source build directory.' }
if (Test-Path -LiteralPath $destination) { throw 'Use a fresh test-client directory; existing copies are preserved.' }
New-Item -ItemType Directory -Path $destination,"$destination/pack","$destination/log","$destination/mark","$destination/upload" | Out-Null
Copy-Item -LiteralPath $binary -Destination "$destination/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$destination/config" -Recurse
Copy-Item -LiteralPath $compiled -Destination "$destination/vegetation" -Recurse
foreach ($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    New-Item -ItemType HardLink -Path "$destination/pack/$($package.Name)" -Target $package.FullName | Out-Null
}
New-Item -ItemType Junction -Path "$destination/bgm" -Target "$original/bgm" | Out-Null
$launcher = "@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" `"%~dp0Metin2_Release.exe`"`r`n"
[IO.File]::WriteAllText("$destination/start-client.cmd", $launcher, [Text.Encoding]::ASCII)
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath "$destination/Metin2_Release.exe" -Algorithm SHA256).Hash -ne $hash) { throw 'Test executable hash differs from build.' }
$count = 0
foreach ($file in Get-ChildItem -LiteralPath $compiled -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($compiled, $file.FullName)
    if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath "$destination/vegetation/$relative" -Algorithm SHA256).Hash) { throw "Compiled asset copy mismatch: $relative" }
    ++$count
}
"ReleaseSHA256=$hash`nCompiledFilesVerified=$count`nCreated=$(Get-Date -Format o)`nGateA2=GO`nGateB=FINAL MANUAL VERIFICATION PENDING" |
    Set-Content -LiteralPath "$destination/artifact.txt"
@'
H-X Sichtpruefung

start-client.cmd startet die fertige Release-EXE ohne Vegetationsschalter.
ZiiNAN Vegetation ist der einzige produktive Pfad.

Login -> Character Select -> Ingame.
A1, B1 und Wald: Baeume, Position/Groesse/Ausrichtung, Aeste, Wedel,
Blaetter, Texturen/Blattalpha, Nebel, Kameraverdeckung, Wind,
Kamera nah -> mittel -> fern -> mittel -> nah, LODs/Billboards/Pops.
Danach Resize, Minimize/Restore und mit X beenden.

Die Original-Packs sind platzsparend verknuepft. Keine Packs hier bearbeiten.
Die Konfiguration ist eine eigene Kopie. Der normale Client bleibt unveraendert.
Die finale manuelle Gate-B-Pruefung steht aus.
'@ | Set-Content -LiteralPath "$destination/LIESMICH.txt"
Write-Output "Test client ready: $destination; SHA256=$hash; compiled files verified=$count"
