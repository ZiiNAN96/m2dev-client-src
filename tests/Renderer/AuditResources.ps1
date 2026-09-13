param([string]$Output = 'docs/renderer/milestone13a-resources-before.csv',[switch]$CheckGuards)
$ErrorActionPreference = 'Stop'
# Reviewed resource families; the CSV preserves every matching source statement.
$families = @(
    @('TextureBinding', 'A', 'TextureBinding', 'image wrapper supplies optional handle', 'shared CPU source; borrowed optional Legacy handle', 'neutral material/image identity', 'native member is null in neutral mode; no COM ownership or GPU source dependency'),
    @('Grp(ExpandedImage|ImageInstance|MarkInstance)|EffectMeshInstance|ParticleSystemInstance|MapOutdoor(Water|RenderHTP|Render.cpp)|SkyBox', 'A', 'Image/effect/world draw callers', 'image or mesh wrappers', 'borrowed resource owned by image/mesh', 'existing Diligent bridge and native state binding', 'bind neutral image identity; retain CPU geometry and Legacy getters where native-only'),
    @('Grp(Texture|ImageTexture)|GrpImage\.|GrpSubImage', 'A', 'CGraphicTexture/CGraphicImageTexture/CGraphicImage', 'ImageTexture Create/decoded asset loader', 'texture wrapper (COM); image (Diligent cache)', 'all image users, bridges, subimages', 'CPU metadata/source ownership and neutral texture binding'),
    @('GrpFontTexture|GrpTextInstance|TextRenderBridge', 'A', 'CGraphicFontTexture/CGraphicTextInstance/TextRenderBridge', 'font atlas AppendTexture/UpdateTexture', 'font pages and text renderer', 'glyph draw and selected atlas page', 'CPU atlas pages; remove native pointer page identity'),
    @('BlockTexture', 'A', 'CBlockTexture', 'Create/InvalidateRect', 'block owns COM and UI texture', 'CDibBar/notice banner', 'original DIB pixels and neutral binding'),
    @('Grp(Vertex|Index)Buffer', 'A', 'CGraphicVertexBuffer/CGraphicIndexBuffer', 'CreateDeviceObjects/Lock', 'wrapper owns buffer', 'Granny terrain water UI minimap', 'owned CPU buffers with same producer lock contract'),
    @('StateManager|NativeStateView', 'A', 'CStateManager/NativeStateView', 'device seed and Save/Set/Restore', 'borrowed Legacy state; Diligent COM retention', 'material snapshots and all render bridges', 'add neutral binding/restore ownership; retain numeric state cache'),
    @('NativeMaterialSnapshot', 'A', 'NativeMaterialSnapshot', 'state capture (does not create)', 'temporary queried COM references', 'all Diligent material captures', 'neutral texture presence; RT query stays Legacy-only'),
    @('UIRenderBridge|EffectRenderBridge|WorldRenderBridge|TreeRenderBridge', 'A', 'UI/Effect/World/TreeRenderBridge', 'image registration and state capture', 'image owns source; subsystem owns Diligent handles', 'texture identity and material validation', 'neutral binding identity; CPU/file upload remains source'),
    @('StaticObjectBridge', 'A', 'StaticObjectBridge', 'Capture material format', 'material owns image; renderer owns GPU', 'static object format validation', 'neutral asset metadata instead of GetLevelDesc'),
    @('EterGrnLib/(Material|Model|ModelInstance)', 'A', 'CGrannyMaterial/CGrannyModel/CGrannyModelInstance', 'material images and model/deform buffers', 'image refs/model/local or shared deform wrapper', 'Actor and static-object submissions; native mesh loops', 'neutral material bindings and CPU geometry existence tests; no Granny algorithm change'),
    @('AreaTerrain|PRTerrainLib/(Terrain|TextureSet)', 'A', 'CTerrain/CTerrainImpl/STerrainTexture/TTerrainSplat', 'AddTexture32/AllocateMarkedSplats/LoadMiniMap', 'terrain owns alpha and marked COM; images own maps', 'terrain layers/minimap/guild projection', 'CPU alpha generation and image metadata; native splat POD retained for Legacy'),
    @('PythonMiniMap', 'A', 'CPythonMiniMap', 'image cache and vertex/index wrappers', 'map images and minimap buffers', 'minimap/atlas draw loops', 'neutral texture presence and CPU buffers'),
    @('MapOutdoorRenderSTP', 'A', 'CMapOutdoor', 'dynamic software terrain buffer allocation', 'outdoor map', 'STP layer draw', 'skip native stream allocation in Diligent; retain CPU terrain source'),
    @('MapOutdoorCharacterShadow', 'B', 'CMapOutdoor', 'CreateCharacterShadowTexture', 'outdoor shadow COM', 'Legacy character shadow pass', 'retain existing Diligent early guard; no dynamic shadow migration'),
    @('MapOutdoor.h', 'A', 'CMapOutdoor', 'terrain buffer and character shadow helpers', 'outdoor map', 'terrain native streams and Legacy shadow pass', 'terrain streams decouple; shadow fields are B and remain null in Diligent'),
    @('SpeedTreeWrapper|SpeedTreeForestDirectX|VertexShaders', 'A', 'CSpeedTreeWrapper/CSpeedTreeForestDirectX', 'SetupBuffers/InitVertexShaders', 'base tree owns COM; instances borrow; forest owns shaders', 'tree LOD setup and draw; CPU SpeedTree geometry capture', 'no native allocation in Diligent; keep CPU LOD/strip data and bindings'),
    @('SnowEnvironment', 'A', 'CSnowEnvironment', '__CreateGeometry/__CreateBlurTexture', 'snow owns geometry and optional blur surfaces', 'particle CPU writes; effect submission', 'CPU particle vertices in Diligent; disabled Legacy blur is B'),
    @('GrpDevice|GrpBase', 'A', 'CGraphicDevice/CGraphicBase', 'device Create/default IB/PDT/declarations/debug mesh', 'device/base static owners', 'common startup and primitive helpers', 'skip explicit native resources for Diligent; retain device implicit surfaces for M13B'),
    @('DungeonBlock', 'A', 'CDungeonBlock', 'model wrapper creation', 'Granny model', 'dungeon static submission and Legacy bindings', 'CPU wrapper source; native binds remain inert in Diligent'),
    @('WeaponTrace', 'A', 'CWeaponTrace', 'image wrapper', 'image ref', 'weapon-trace effect submission', 'neutral texture binding; original CPU ribbon source'),
    @('DDSTextureLoader9', 'B', 'DirectX DDS loader', 'CreateTexture/Cube/Volume and staging resources', 'returned COM caller; local staging released', 'Legacy texture wrapper; CPU GetDDS2DView is shared', 'retain native loader; Diligent uses CPU parser only; format headers E'),
    @('GrpScreen|EterPythonLib/PythonGraphic', 'B', 'CScreen/CPythonGraphic', 'GetBackBuffer/readback and debug meshes', 'temporary queried surfaces/base debug meshes', 'Legacy screenshot/debug rendering', 'Diligent screenshot already owns D3D11 staging; retain Legacy helpers'),
    @('LensFlare', 'B', 'CLensFlare', 'GetDepthStencilSurface in disabled depth-read block', 'temporary query', 'Legacy flare visibility helper', 'retain; Diligent flare vertices already CPU; texture binding is A'),
    @('GrpShadowTexture', 'D', 'CGraphicShadowTexture', 'Create/Begin/End', 'wrapper COM surfaces/texture', 'no production caller; ThingInstance declaration only', 'retain unused Legacy helper, no new shadow feature'),
    @('Grp(Vertex|Pixel)Shader', 'D', 'CVertexShader/CPixelShader', 'CreateFromDiskFile', 'shader wrapper', 'no production instances found', 'retain unused Legacy wrapper'),
    @('PythonApplicationLogo', 'D', 'CPythonApplication logo helpers', 'OnLogoOpen/OnLogoUpdate', 'logo image wrapper', 'Python entrypoints are stubs', 'retain disconnected DirectShow path; no video migration'),
    @('SpeedGrassWrapper', 'E', 'CSpeedGrassWrapper', 'USE_SPEEDGRASS conditional code', 'borrowed image texture', 'not enabled in supported builds', 'retain inactive conditional header path'),
    @('FlyTrace', 'D', 'CFlyTrace', 'commented declaration', 'none', 'none', 'retain comment')
)
$types = 'IDirect3D(?:BaseTexture|CubeTexture|VolumeTexture|Texture|VertexBuffer|IndexBuffer|Surface|Query|StateBlock|VertexDeclaration|VertexShader|PixelShader)9|LPDIRECT3D(?:BASETEXTURE|CUBETEXTURE|VOLUMETEXTURE|TEXTURE|VERTEXBUFFER|INDEXBUFFER|SURFACE|QUERY|STATEBLOCK|VERTEXDECLARATION|VERTEXSHADER|PIXELSHADER)9|LPD3DXMESH|ID3DXMesh'
$calls = 'Create(?:Texture|CubeTexture|VolumeTexture|VertexBuffer|IndexBuffer|VertexDeclaration|VertexShader|PixelShader|RenderTarget|DepthStencilSurface|OffscreenPlainSurface|Query|StateBlock)\(|D3DXCreate(?:Sphere|Cylinder|Mesh|\w*Texture)|GetD3DTexture\(|GetD3DVertexBuffer\(|GetD3DIndexBuffer\('
$matches = & rg --json "$types|$calls" src tests/Renderer | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object type -eq match
$rows = foreach ($match in $matches) {
    $file = $match.data.path.text.Replace('\','/')
    if ($file -match 'AuditResources.ps1|src/Renderer/Diligent') { continue }
    $statement = $match.data.lines.text.Trim()
    $family = $null
    foreach ($candidate in $families) { if ($file -match $candidate[0]) { $family = $candidate; break } }
    # Mixed owners are classified at the member/call level, not only by filename.
    if ($file -eq 'src/GameLib/MapOutdoor.h' -and $statement -match 'Shadow|Backup') {
        $family = @('', 'B', 'CMapOutdoor', 'CreateCharacterShadowTexture/BeginRenderCharacterShadowToTexture', 'outdoor shadow COM; temporary retained backup surfaces', 'Legacy shadow pass only', 'retain null-in-Diligent shadow/backup members; no new shadow path')
    }
    if ($file -match 'LensFlare' -and $statement -match 'GetD3DTexture') {
        $family = @('', 'A', 'CLensFlare', 'image wrapper', 'flare image instance', 'world flare draw bridge', 'neutral texture binding; original CPU flare vertices')
    }
    if ($file.StartsWith('tests/')) { $family = @('', 'C', 'Renderer test/probe', 'explicit test device setup', 'test-local RAII/COM', 'test assertions/parity', 'retain native reference renderer; add neutral production-resource tests') }
    if (!$family) { throw "Unclassified resource occurrence: ${file}:$($match.data.line_number) $statement" }
    $native = ([regex]::Matches($statement,$types) | ForEach-Object Value | Sort-Object -Unique) -join '; '
    if (!$native) { $native = 'wrapper/native creation or getter (see statement)' }
    [pscustomobject][ordered]@{File=$file; Line=$match.data.line_number; Class=$family[2]; MemberOrReference=$statement; NativeType=$native; Creator=$family[3]; Owner=$family[4]; Readers=$family[5]; DiligentReachable=($family[1] -eq 'A'); LegacyOnly=($family[1] -eq 'B'); ToolDebugOnly=($family[1] -eq 'C'); Category=$family[1]; M13AAction=$family[6]}
}
$directory = Split-Path $Output
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$rows | Sort-Object File,Line | Export-Csv -LiteralPath $Output -NoTypeInformation -Encoding UTF8
$rows | Group-Object Category | Select-Object Name,Count
"Occurrences: $($rows.Count); files: $(($rows.File | Sort-Object -Unique).Count)"
if ($CheckGuards) {
    $creation = '->\s*Create(?:Texture|CubeTexture|VolumeTexture|VertexBuffer|IndexBuffer|VertexDeclaration|VertexShader|PixelShader|RenderTarget|DepthStencilSurface|OffscreenPlainSurface|Query|StateBlock)\s*\(|\bD3DXCreate(?:Sphere|Cylinder|Mesh\w*|\w*Texture\w*)\s*\('
    $sites = & rg --json -e $creation src | ForEach-Object { $_ | ConvertFrom-Json } |
        Where-Object { $_.type -eq 'match' -and $_.data.path.text.Replace('\','/') -notmatch '^src/Renderer/Diligent' }
    if ($LASTEXITCODE -ne 0 -or !$sites.Count) { throw 'Native creation scan failed or unexpectedly empty.' }
    foreach ($site in $sites) {
        if ($site.data.lines.text -notmatch 'M2_NATIVE_RESOURCE\(') {
            throw "Unguarded native creation: $($site.data.path.text):$($site.data.line_number)"
        }
    }
    "Guarded native resource creation sites: $($sites.Count). Device implicit surfaces and D3DX helper internals are reported separately."
}
