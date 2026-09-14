param([string]$SourceRoot = "$PSScriptRoot/../..")
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $SourceRoot).Path
$patterns = [ordered]@{
    Includes = '#\s*include\s*[<"][^>"\r\n]*granny\.h[>"]'
    PoseCalls = '\bGranny(?:New|Free|Get|Set|Build|Sample|Accumulate|Apply)\w*Pose\w*\s*\('
    SamplingCalls = '\bGranny(?:Sample|Evaluate)\w*\s*\('
    ControlCalls = '\bGranny\w*Control\w*\s*\('
    PoseTypes = '\bgranny_(?:local|world)_pose\b'
    AnimationTypes = '\bgranny_(?:animation|skeleton|bone|track_group|transform_track|curve2|control|local_pose|world_pose|model_instance)\b'
}
$groups = @{}
foreach ($scope in @('All', 'PrivateProvider', 'Actor', 'AnimationCore')) {
    $groups[$scope] = [ordered]@{ Scope = $scope; Files = 0 }
    foreach ($metric in $patterns.Keys) { $groups[$scope][$metric] = 0 }
}
$paths = & rg --files "$root/src" -g '*.h' -g '*.cpp'
if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate source files.' }
foreach ($path in $paths) {
    $relative = $path.Substring($root.Length).Replace('\', '/')
    $code = [regex]::Replace((Get-Content -LiteralPath $path -Raw), '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')
    $scopes = @('All')
    if ($relative -like '/src/AssetRuntime/Granny/*') { $scopes += 'PrivateProvider' }
    if ($relative -like '/src/GameLib/Actor*' -or $relative -like '/src/UserInterface/InstanceBase*') { $scopes += 'Actor' }
    if ($relative -like '/src/AnimationRuntime/*') { $scopes += 'AnimationCore' }
    foreach ($scope in $scopes) {
        ++$groups[$scope].Files
        foreach ($metric in $patterns.Keys) { $groups[$scope][$metric] += [regex]::Matches($code, $patterns[$metric]).Count }
    }
}
@('All', 'PrivateProvider', 'Actor', 'AnimationCore') | ForEach-Object { [pscustomobject]$groups[$_] }
