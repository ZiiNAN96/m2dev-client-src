# ZiiNAN: Removed final D3D9 compile-time dependency. Read-only build/import audit.
param([Parameter(Mandatory=$true)][ValidateSet('Release','Debug')][string]$Configuration,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$BuildDirectory)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
if(-not $BuildDirectory) {$BuildDirectory="$source/build"}
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$binary=(Resolve-Path -LiteralPath "$build/bin/$Configuration/Metin2_$Configuration.exe").Path
$dumpbin='C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe'
if(-not(Test-Path -LiteralPath $dumpbin)) { throw 'Set the installed Visual C++ dumpbin path in the audit script.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$dependencyRows=[Collections.Generic.List[object]]::new()
$pending=[Collections.Generic.Queue[string]]::new(); $pending.Enqueue($binary)
$visited=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
while($pending.Count) {
    $file=$pending.Dequeue()
    if(-not $visited.Add($file)) { continue }
    $raw=& $dumpbin /nologo /dependents $file
    if($LASTEXITCODE) { throw "Cannot inspect $file" }
    foreach($line in $raw) {
        if($line -notmatch '^\s+([a-zA-Z0-9_.-]+\.dll)\s*$') { continue }
        $dll=$Matches[1]
        $local=Join-Path (Split-Path $file) $dll
        $applicationOwned=(Test-Path -LiteralPath $local) -and -not $local.StartsWith($env:windir,[StringComparison]::OrdinalIgnoreCase)
        $dependencyRows.Add([pscustomobject]@{importer=$file;dll=$dll;kind=if($applicationOwned){'application'}else{'Windows/driver/API-set'}})
        if($dll -match '^d3d9\.dll$|^d3dx9.*\.dll$') { throw "Required obsolete graphics import: $file -> $dll" }
        if($applicationOwned) { $pending.Enqueue((Resolve-Path -LiteralPath $local).Path) }
    }
}
$dependencyRows | Export-Csv -LiteralPath "$OutputDirectory/$Configuration-imports.csv" -NoTypeInformation
$headerHits=[Collections.Generic.List[object]]::new()
$oldWindowsHeaders=[Collections.Generic.HashSet[string]]::new()
$units=[Collections.Generic.HashSet[string]]::new()
Get-ChildItem -LiteralPath "$build/src","$build/vendor","$build/_deps" -Recurse -Filter 'CL.read.1.tlog' |
    Where-Object { $_.FullName -match "\\$Configuration\\" } | ForEach-Object {
    $active=$false; $unit=''
    foreach($line in Get-Content -LiteralPath $_.FullName -Encoding Unicode) {
        if($line.StartsWith('^')) {
            $unit=$line.Substring(1); $active=Test-Path -LiteralPath $unit
            if($active) { [void]$units.Add($unit) }
        } elseif($active) {
            if($line -match '\\(?:d3d9[a-zA-Z0-9_]*|d3dx9[a-zA-Z0-9_]*)\.h$') { $headerHits.Add([pscustomobject]@{unit=$unit;header=$line}) }
            if($line -match '\\(?:d3dtypes|d3dcaps)\.h$') { [void]$oldWindowsHeaders.Add($line) }
        }
    }
}
if($headerHits.Count) { $headerHits | Export-Csv -LiteralPath "$OutputDirectory/$Configuration-forbidden-headers.csv" -NoTypeInformation; throw 'Forbidden compiler header dependencies.' }
$commands=Get-Content -LiteralPath "$build/src/UserInterface/UserInterface.dir/$Configuration/UserInterface.tlog/link.command.1.tlog" -Encoding Unicode
if($commands -match '(?i)(?:d3d9|d3dx9[^\\\s"]*)\.lib') { throw 'Forbidden library in actual link command.' }
$hash=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
@(
"Configuration=$Configuration"
"Binary=$binary"
"SHA256=$hash"
"InspectedTranslationUnits=$($units.Count)"
"D3D9HeaderDependencies=$($headerHits.Count)"
"D3D9LinkLibraries=0"
"D3D9RequiredImports=0"
"ApplicationBinariesInspected=$($visited.Count)"
"DirectDraw/DirectInput/DirectShow are independent retained input/video components, not D3D9."
"Windows-internal imports are not recursively treated as client library requirements."
"Older Windows video-interface headers (not D3D9 SDK):"
$oldWindowsHeaders
) | Set-Content -LiteralPath "$OutputDirectory/$Configuration-dependency-summary.txt"
Get-Content -LiteralPath "$OutputDirectory/$Configuration-dependency-summary.txt"
