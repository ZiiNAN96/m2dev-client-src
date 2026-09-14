param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [Parameter(Mandatory=$true)][string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binary = (Resolve-Path -LiteralPath "$build/bin/Release/Metin2_Release.exe").Path
$packMaker = (Resolve-Path -LiteralPath "$build/bin/Release/PackMaker.exe").Path
$assetTool = (Resolve-Path -LiteralPath "$build/tools/AssetTool/Release/ziinan-asset-tool.exe").Path
$rootTemplate = (Resolve-Path -LiteralPath "$original/assets/root").Path
$target = Join-Path $source "build-e2x/runtime/$Name"
if (Test-Path -LiteralPath $target) { throw 'A fresh E2-X manual evidence directory is required.' }
New-Item -ItemType Directory -Path "$target/test-root", "$target/pack", "$target/log", "$target/mark", "$target/upload" | Out-Null
Copy-Item -LiteralPath $binary -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath $rootTemplate -Destination "$target/test-root/root" -Recurse
New-Item -ItemType Directory -Path "$target/test-root/root/assettool" | Out-Null
$converted = "$target/test-root/root/assettool/market_stall.glb"
& $assetTool convert "$PSScriptRoot/fixtures/market_stall.obj" $converted --json |
    Tee-Object -FilePath "$target/asset-conversion.json"
if ($LASTEXITCODE -ne 0) { throw 'Fresh OBJ-to-GLB conversion failed.' }
& $assetTool validate $converted --json | Tee-Object -FilePath "$target/asset-validation.json"
if ($LASTEXITCODE -ne 0) { throw 'Fresh GLB failed the E1-X provider validation.' }
@'
ScriptType RaceDataScript
BaseModelFileName "assettool/market_stall.glb"
'@ | Set-Content -LiteralPath "$target/test-root/root/assettool/market_stall.msm" -Encoding ascii

# ZiiNAN: The only Python change is in the private copied root pack, after the normal game update.
$hook = @'
"""Private E2-X manual evidence helper; normal login and character selection are unchanged."""
import background, builtins, chr, chrmgr, grp, player, time


def record(text):
    with builtins.old_open("e2x-manual-asset.log", "a") as stream:
        stream.write(text + "\n")


def update(owner):
    if getattr(owner, "_e2x_proof_disabled", False):
        return
    try:
        main = player.GetMainCharacterIndex()
        if not main or not chr.HasInstance(main):
            return
        now = time.monotonic()
        if not hasattr(owner, "_e2x_proof_entered"):
            owner._e2x_proof_entered = now
            record("entered-ingame main-vid=%d" % main)
        if now - owner._e2x_proof_entered < 1.0:
            return
        if not getattr(owner, "_e2x_proof_spawned", False):
            if chr.HasInstance(57999):
                raise RuntimeError("Reserved local proof VID 57999 already exists; existing instance preserved")
            chrmgr.CreateRace(57999)
            chrmgr.SelectRace(57999)
            chrmgr.LoadLocalRaceData("assettool/market_stall.msm")
            x, y, z = player.GetMainCharacterPosition()
            px, py = int(x - 350), int(y + 100)
            pz = int(background.GetHeight(px, py))
            chr.CreateInstance(57999)
            chr.SelectInstance(57999)
            try:
                chr.SetVirtualID(57999)
                chr.SetInstanceType(1)
                chr.SetRace(57999)
                chr.SetArmor(0)
                chr.SetNameString("E2-X Converted OBJ Stall")
                chr.SetPixelPosition(px, py, pz)
                chr.SetRotation(0.0)
                chr.Show()
            finally:
                chr.SelectInstance(main)
            owner._e2x_proof_spawned = True
            owner._e2x_proof_shot_at = now + 2.0
            record("spawned race=57999 vid=57999 glb=assettool/market_stall.glb position=%d,%d,%d main-vid=%d" % (px, py, pz, main))
        elif not getattr(owner, "_e2x_proof_shot", False) and now >= owner._e2x_proof_shot_at:
            owner._e2x_proof_shot = True
            success, path = grp.SaveScreenShotToPath("e2x-manual-ingame-")
            record("screenshot success=%d file=%s" % (bool(success), path))
    except Exception as error:
        owner._e2x_proof_disabled = True
        record("ERROR: " + str(error))
'@
$utf8 = [System.Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText("$target/test-root/root/e2x_asset_proof.py", $hook, $utf8)
$gamePath = "$target/test-root/root/game.py"
$byteEncoding = [Text.Encoding]::GetEncoding(28591)
$gameSource = $byteEncoding.GetString([IO.File]::ReadAllBytes($gamePath))
$marker = "`t`tapp.UpdateGame()"
if ([regex]::Matches($gameSource, [regex]::Escape($marker)).Count -ne 1) {
    throw 'Expected exactly one normal game update insertion point in the copied game.py.'
}
$newline = if ($gameSource.Contains("`r`n")) { "`r`n" } else { "`n" }
$replacement = $marker + $newline + "`t`timport e2x_asset_proof" + $newline + "`t`te2x_asset_proof.update(self)"
[IO.File]::WriteAllBytes($gamePath, $byteEncoding.GetBytes($gameSource.Replace($marker, $replacement)))
foreach ($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if ($package.Name -ne 'root.pck') {
        New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null
    }
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
& $packMaker --input "$target/test-root/root" --output "$target/pack"
if ($LASTEXITCODE -ne 0) { throw 'Private manual root packaging failed.' }
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash -ne $hash) {
    throw 'Manual runtime copy differs from the completed Release build.'
}
$assetHash = (Get-FileHash -LiteralPath $converted -Algorithm SHA256).Hash
"SourceBinary=$binary`nSHA256=$hash`nAssetTool=$assetTool`nConvertedGLB=$converted`nGLB_SHA256=$assetHash`nArguments=--renderer-diagnostics`nNormal Login -> Character Select -> Ingame; private game.py hook adds one local static GLB NPC." |
    Set-Content -LiteralPath "$target/artifact.txt"
@'
Start Metin2_Release.exe from this directory with --renderer-diagnostics.
Complete normal Login -> Character Select -> Ingame.
The converted market stall appears 350 cm left and 100 cm forward of the initial player position.
Check the wooden counter/posts, red awning, cream trim/sign and three green produce crates alongside the original GR2 world/player.
Check resize, minimize/restore, then close with X.
Evidence: e2x-manual-asset.log (entered/spawned/screenshot); e2x-manual-ingame-*.jpg screenshot;
actor-renderer.log (race=57999 file=assettool/market_stall.glb, deform_vertices=0, rigid_vertices>0 and rigid=1);
renderer-startup.log ExitCode=0; source-resource-audit.log zero final resources/CPU deformation/fallbacks.
The existing actor diagnostic label CPU-skinned main body also covers rigid bodies; its vertex counters and global CPU counters establish the actual path.
'@ | Set-Content -LiteralPath "$target/manual-checklist.txt" -Encoding ascii
Write-Output "Prepared manual E2-X runtime: $target"
Write-Output "Binary: $target/Metin2_Release.exe"
Write-Output 'Start separately with working directory above and --renderer-diagnostics; this preparation script does not launch a process.'
