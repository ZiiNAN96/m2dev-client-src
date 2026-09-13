# ZiiNAN: Complete occurrence index plus reviewed caller routes; runtime evidence is separate.
param([string]$Output='docs/renderer-milestone11-d3d9.csv')
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$api='\b(?:IDirect3D\w*9(?:Ex)?|LPDIRECT3D\w*9(?:EX)?|D3DX\w*(?:Texture|Surface|Mesh|Shader)\w*|DrawPrimitive(?:UP)?|DrawIndexedPrimitive(?:UP)?|DrawSubset|(?:Set|Get|Save|Restore)(?:FVF|VertexDeclaration|VertexShader(?:ConstantF?)?|PixelShader(?:ConstantF?)?|Texture(?:StageState)?|RenderState|SamplerState|Transform|StreamSource|Indices|Material|Light|Viewport|ScissorRect)|LightEnable|GetLightEnable|CreateVertexBuffer|CreateIndexBuffer|CreateTexture|CreateRenderTarget|CreateDepthStencilSurface|CreateVertexShader|CreatePixelShader|SetRenderTarget|GetRenderTarget|SetDepthStencilSurface|GetDepthStencilSurface|GetBackBuffer|StretchRect|ColorFill|CreateStateBlock|SetGammaRamp|BeginScene|EndScene|PresentEx|ResetEx)\b'
# Explicit indirect consumers/gates: these can hide a visible path without spelling a D3D9 API.
$api+='|\b(?:UIExcludeScope|RenderEventSet)\b'
# Routes are based on source callers and the original assets/root entry points, not API spelling.
$routes=@(
    @('src/EffectLib/','RenderGame -> EffectManager/area effects -> EffectInstance -> ParticleSystemInstance/EffectMeshInstance -> EffectRenderBridge','M7 effect renderer'),
    @('src/EterGrnLib/','actors/map Things/items -> ThingInstance -> ModelInstanceRender; Model/Material own native CPU conversion/resources','M4/M5/M8/M11 shared actor/static material bridges'),
    @('src/EterImageLib/','GrpImageTexture -> DDSTextureLoader9 native asset decoding/resource creation','CPU texture decode/upload; native resource dependency retained'),
    @('src/EterLib/(GrpFontTexture|GrpTextInstance|TextRenderBridge)','PythonWindow/textTail -> CGraphicTextInstance -> TextRenderBridge; font atlas owner','M10 text renderer'),
    @('src/EterLib/(GrpImageInstance|GrpExpandedImageInstance|GrpMarkInstance|UIRenderBridge)','PythonWindow image/slot/mark -> original quad -> UIRenderBridge','M9 UI renderer'),
    @('src/EterLib/(BlockTexture|DibBar)','uitip.TextBar -> grp.RenderTextBar -> DibBar -> BlockTexture','M11 original DIB pixel upload through UI'),
    @('src/EterLib/(SkyBox|LensFlare|WorldRenderBridge)','RenderGame -> PythonBackground sky/cloud/lens -> original world quads','M8/M11 world renderer'),
    @('src/EterLib/(StateManager|NativeStateView|NativeMaterialSnapshot|GrpLightManager)','all native render callers -> compatibility state cache -> draw snapshots; Legacy device behind startup gate','M11 CPU state view; five native draw wrappers gated'),
    @('src/EterLib/GrpScreen','grp/UI/world/debug callers -> CScreen primitives; per-function exceptions below','M9 UI primitive / M11 lens-flare bridges; debug excluded'),
    @('src/EterLib/','application/map/image/model -> native device, buffers, image/texture owners and transforms','M1-M11 bridges; D3D9 resource/bootstrap dependency retained'),
    @('src/EterPythonLib/','PythonWindow / grp module -> PythonGraphic -> CScreen / image/text bridges','M9/M10 UI/text; M11 preview viewport, light, screenshot'),
    @('src/GameLib/(Actor|WeaponTrace|FlyTrace)','InstanceBase -> ActorInstance -> Granny render; weapon/flying traces -> EffectRenderBridge','M5/M7/M11 shared actors and effects'),
    @('src/GameLib/(DungeonBlock)','Area property DungeonBlockFile -> CDungeonBlock -> rigid PNT2 triangle groups','M11 two-texture world material'),
    @('src/GameLib/(MapOutdoor|Area|TerrainPatch|StaticObjectBridge|MapManager)','RenderGame -> PythonBackground -> MapOutdoor -> terrain/area objects/water/PC blockers; native map resource owners','M2-M4/M8/M11 terrain/static/actor/world bridges'),
    @('src/GameLib/SnowEnvironment','RenderGame -> PythonBackground.RenderSnow -> SnowEnvironment.Render','M7 snow particles; inactive blur helpers classified separately'),
    @('src/PRTerrainLib/','MapOutdoor terrain resources -> original terrain/texture set data','M2/M3 terrain and texture upload'),
    @('src/SpeedTreeLib/','MapOutdoor.RenderArea -> SpeedTreeForestDirectX -> SpeedTreeWrapper -> TreeRenderBridge','M6 original SDK geometry/materials'),
    @('src/UserInterface/PythonMiniMap','uiMiniMap -> PythonMiniMap.Render/RenderAtlas -> nine tiles/mask/images/markers','M11 masked tiles plus M9 images; no render target'),
    @('src/UserInterface/PythonEventManager','uiQuest.DescriptionWindow -> event.RenderEventSet -> PythonEventManager text lines -> CGraphicTextInstance.Render','M10 text renderer; M11 removes obsolete event-text exclusion'),
    @('src/UserInterface/PythonApplication','Python main loop/RenderGame -> renderer BeginFrame/Present; scene-specific exceptions below','M1-M11 lifecycle and visible paths'),
    @('src/UserInterface/(InstanceBase|PythonCharacterManager)','network/race -> InstanceBase -> character manager/ActorInstance; damage -> original effects','M5/M7/M10/M11 actors/effects/floating text'),
    @('src/UserInterface/(PythonBackground|PythonSystem)','RenderGame/system options -> Background/MapOutdoor environment and native settings','M1-M11 CPU state / world bridges')
)
$files=& rg --files "$source/src" "$source/extern" "$source/vendor" "$source/buildtool" -g '*.cpp' -g '*.h' -g '*.hpp' -g '*.inl' -g '*.c'
$rows=[System.Collections.Generic.List[object]]::new()
foreach($file in $files) {
    $raw=[IO.File]::ReadAllText($file)
    if($raw -notmatch $api) { continue }
    $relative=[IO.Path]::GetRelativePath($source,$file).Replace('\','/')
    $code=[regex]::Replace($raw,'(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*''', { param($m) [regex]::Replace($m.Value,'[^\r\n]',' ') })
    # Compute brace intervals once, avoiding quadratic rescans for each API occurrence.
    $ends=@{}; $stack=[System.Collections.Generic.Stack[int]]::new()
    foreach($brace in [regex]::Matches($code,'[{}]')) {
        if($brace.Value -eq '{') { $stack.Push($brace.Index) }
        elseif($stack.Count) { $ends[$stack.Pop()]=$brace.Index }
    }
    $functions=@([regex]::Matches($code,'(?m)^[\w\s:*&<>~]+?\b(?<name>(?:\w+::)*~?\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?\{') | Where-Object { $_.Groups['name'].Value -notmatch '^(if|else|for|while|switch|catch)$' })
    $classes=@([regex]::Matches($code,'\b(?:class|struct)\s+(?<name>\w+)[^;{}]*\{'))
    $newlines=[regex]::Matches($raw,'\n'); $line=1; $newline=0
    foreach($hit in [regex]::Matches($raw,$api)) {
        while($newline -lt $newlines.Count -and $newlines[$newline].Index -lt $hit.Index) { ++$line; ++$newline }
        $comment=$code.Substring($hit.Index,$hit.Length).Trim().Length -eq 0
        $function='declaration/global'; $class='-'
        foreach($f in $functions) {
            if($f.Index -gt $hit.Index) { break }
            $open=$f.Index+$f.Length-1
            if($ends.ContainsKey($open) -and $hit.Index -le $ends[$open]) { $function=$f.Groups['name'].Value }
        }
        if($function.Contains('::')) { $class=$function.Substring(0,$function.LastIndexOf('::')) }
        else { foreach($c in $classes) {
            $open=$c.Index+$c.Length-1
            if($c.Index -lt $hit.Index -and $ends.ContainsKey($open) -and $hit.Index -lt $ends[$open]) { $class=$c.Groups['name'].Value }
        } }
        $category='G'; $reachable='yes: shared runtime, wrapper or resource declaration'
        $chain=''; $alternative=''; $action='Keep parallel Legacy path; CPU state and Diligent submission in selected backend'
        foreach($route in $routes) { if($relative -match $route[0]) { $chain=$route[1]; $alternative=$route[2]; break } }
        if($relative.StartsWith('extern/')) { $category='E'; $reachable='SDK declaration / external library implementation, not client draw'; $chain='external header; not a game call chain'; $alternative='Diligent API, or retained asset dependency'; $action='No SDK edit' }
        elseif($relative.StartsWith('vendor/') -or $relative.StartsWith('buildtool/')) { $category='E'; $reachable='dependency/tool'; $chain='build/dependency source, not normal scene'; $alternative='n/a'; $action='No tool/vendor migration' }
        elseif($relative -match '/Renderer/') { $category='G'; $reachable='renderer implementation or interface; same API name need not mean D3D9'; $chain='renderer interface -> Diligent D3D11'; $alternative='this IS the Diligent implementation'; $action='Do not count D3D11 name collisions as D3D9 calls' }
        elseif($relative -match '(TerrainDecal|/Decal\.|SpeedGrassWrapper|GrpPixelShader|GrpVertexShader|ScreenFilter|PythonApplicationLogo|GrpShadowTexture)') {
            $category='F'; $reachable='no active normal render caller'
            $chain=switch -Regex ($relative) {
                'ScreenFilter' { 'SetEnable ignores argument and always assigns FALSE'; break }
                'PythonApplicationLogo' { 'grp/app logo entry no longer calls OnLogoRender; MovieMan plays OS video'; break }
                'GrpShadowTexture' { 'CGraphicShadowTexture: no external instantiation/caller in src'; break }
                default { 'library definitions only; no external instance/use in src or original root Python' }
            }
            $alternative='not required'; $action='Retain inactive library/helper; no feature invented'
        }
        elseif($relative -match 'BlockTexture') { $category='A'; $action='M11: original DIB blocks uploaded through UI' }
        elseif($relative -match 'MapOutdoorCharacterShadow' -or $function -match '(Render.*Shadow|Shadow.*Texture)') {
            $category='C'; $reachable='Legacy normal shadow/helper, disabled in Diligent'
            $chain='RenderGame -> RenderCharacterShadowToTexture -> MapOutdoor shadow RT / receivers'
            $alternative='Option B, intentionally no Diligent dynamic shadow'; $action='Diligent-only generation/receiver gate; no new shadow engine'
        }
        elseif($relative -match 'CollisionData' -or $class -match 'CD3DXMeshRenderingOption' -or $function -match '(Collision|WireFrame|Wireframe|RenderD3DXMesh|RenderCube|RenderSphere|RenderCylinder|Granny_RenderBoxBones|RenderAmbience|CD3DXMeshRenderingOption)') {
            $category='D'; $reachable='only explicit developer visualization'
            $chain='game.py console.Console.collision (default false), debug bone _TEST, or tool ambience helper'
            $alternative='not required'; $action='No debug migration'
        }
        elseif($relative -match 'DungeonBlock' -or $function -match 'RenderMeshNodeListWithTwoTexture|RenderDungeon|RenderMarkedArea|DrawPatchAttr') { $category='A'; $reachable='yes: original dungeon/guild special pass'; $action='M11: original geometry, textures and combiner' }
        elseif($relative -match 'PythonMiniMap') { $category='A'; $reachable='yes: minimap/atlas'; $action='M11: nine masked tiles and original atlas/marker images' }
        elseif($relative -match 'PythonEventManager') { $category='A'; $reachable='yes: quest/NPC dialog text'; $action='M11: remove inherited UIExcludeScope, use original text lines and existing M10 renderer' }
        elseif($function -match 'SaveScreenShot|FinishScreenShot') { $category='A'; $reachable='yes: screenshot key'; $chain='PrintScreen -> grp.SaveScreenShot -> queued complete frame -> CaptureRGB -> JPEG'; $alternative='M11 D3D11 readback + existing stb encoder'; $action='Do not capture hidden native backbuffer; Legacy writer stub retained' }
        elseif($function -match 'SetOmniLight|RestoreViewport' -or ($relative -match 'PythonGraphic' -and $function -match 'SetViewport')) { $category='B'; $reachable='yes: selection/creation preview'; $chain='introSelect/introCreate CharacterRenderer -> grp light/viewport -> chr.Deform/Render'; $alternative='M11 actor viewport/native spotlight'; $action='Reuse actor renderer and CPU state view' }
        if($relative -match 'SnowEnvironment' -and ($function -match '__BeginBlur|__RenderBlur|__CreateBlur|__ApplyBlur' -or $hit.Value -match 'RenderTarget|DepthStencil|D3DUSAGE_RENDERTARGET')) { $category='F'; $reachable='blur flag private, initialized FALSE, never enabled'; $chain='inactive snow blur branch; snow particles remain M7'; $alternative='n/a'; $action='No postprocess port' }
        if($function -eq 'CScreen::RenderBillboard') { $category='F'; $reachable='no callers'; $chain='CScreen definition only (not SpeedTree RenderBillboards)'; $alternative='n/a'; $action='No migration' }
        if($relative -eq 'src/Renderer/UIRenderData.h' -and $hit.Value -eq 'UIExcludeScope') { $category='F'; $reachable='declaration only; no remaining scope instances in src'; $chain='old UI exclusion helper; minimap/atlas and event-text exclusions removed'; $alternative='normal UI/text submissions'; $action='Keep inert helper, no hidden runtime exclusion' }
        if($function -eq 'CScreen::RenderBox3d') { $category='G'; $reachable='yes via UI RenderBox2d, also debug'; $chain='RenderBox2d -> RenderBox3d -> original lines -> UIRenderBridge'; $alternative='M9 lines; 3D debug not migrated'; $action='Do not classify the shared UI primitive as debug-only' }
        if($relative -match 'NativeStateView|StateManager') { $reachable='shared compatibility wrapper, not independent visual system' }
        if($comment) { $category='F'; $reachable='comment or string literal only'; $chain='no API execution'; $alternative='n/a'; $action='Retain text; excluded from runtime API counts' }
        if(!$chain -or !$alternative) { throw "Unreviewed file route: $relative" }
        $rows.Add([pscustomobject][ordered]@{File=$relative;Line=$line;Class=$class;Function=$function;API=$hit.Value;Category=$category;NormalClientReachability=$reachable;DiligentAlternative=$alternative;M11Action=$action;CallChain=$chain})
    }
}
$destination=if([IO.Path]::IsPathRooted($Output)) { $Output } else { Join-Path $source $Output }
$rows | Sort-Object File,Line,API | Export-Csv -LiteralPath $destination -NoTypeInformation -Encoding utf8
$rows | Group-Object Category | Select-Object Name,Count
Write-Output "Indexed $($rows.Count) occurrences; SDK/types/comments are explicitly NOT runtime calls."
