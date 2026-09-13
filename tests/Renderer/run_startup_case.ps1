# ZiiNAN: Exercise the real WinMain selection in a private directory, including expected errors.
param([Parameter(Mandatory=$true)][string]$ClientBinary,
      [Parameter(Mandatory=$true)][string]$OutputDirectory,
      [Parameter(Mandatory=$true)][ValidateSet('default','d3d11','alias','invalid','removed')][string]$Case,
      [Parameter(Mandatory=$true)][ValidateSet('d3d11','none')][string]$ExpectedBackend,
      [int]$ExpectedExit=0)
$ErrorActionPreference='Stop'
$binary=(Resolve-Path -LiteralPath $ClientBinary).Path
if(Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh evidence directory.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
Copy-Item -LiteralPath $binary -Destination "$output/Metin2_Release.exe"
$arguments=switch($Case) {
    'default' { @('--renderer-smoke-test') }
    'd3d11' { @('--renderer=d3d11','--renderer-smoke-test') }
    'alias' { @('--renderer=diligent-d3d11','--renderer-smoke-test') }
    'removed' { @('--renderer=legacy-d3d9','--renderer-smoke-test') }
    'invalid' { @('--renderer=vulkan') }
}
$process=Start-Process -FilePath "$output/Metin2_Release.exe" -WorkingDirectory $output -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "Started startup case=$Case PID=$($process.Id)"
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) ExpectedExit=$ExpectedExit" | Tee-Object -FilePath "$output/exit.txt"
if($process.ExitCode -ne $ExpectedExit) { throw 'Unexpected process exit code.' }
$log=Get-Content -LiteralPath "$output/renderer-startup.log" -Raw
if($ExpectedBackend -ne 'none') {
    $label='Renderer: Diligent D3D11'
    if(!$log.Contains($label)) { throw 'Unexpected selected backend.' }
} elseif($log.Contains('Renderer: ')) { throw 'Invalid selection reached backend setup.' }
if($ExpectedExit -eq 0) {
    $bootstrap=Get-Content -LiteralPath "$output/renderer-bootstrap.log" -Raw
    if(!$bootstrap.Contains('Initialize OK') -or !$bootstrap.Contains('Shutdown OK') -or !$bootstrap.Contains('WM_SIZE resize OK')) { throw 'Incomplete lifecycle.' }
}
Write-Output "Startup case $Case PASS"
