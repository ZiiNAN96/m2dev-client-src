# ZiiNAN: GPU skinning production path — preserve raw per-frame evidence, never score FPS alone.
param([Parameter(Mandatory=$true)][string]$Csv, [ValidateSet('isolated','world')][string]$Kind='isolated',
      [ValidateSet('cpu','gpu')][string]$WorldMode='cpu')
$ErrorActionPreference='Stop'
$rows=@(Import-Csv -LiteralPath $Csv)
if($Kind -eq 'world') { $rows=@($rows | Where-Object { [int]$_.stage -gt 0 -and [int]$_.sample -ge 0 }) }
if(!$rows.Count) { throw 'No measured frames.' }
function Stats($items,[string]$name) {
    $values=@($items | Where-Object { $_.$name -ne '' } | ForEach-Object { [double]::Parse($_.$name,[Globalization.CultureInfo]::InvariantCulture) } | Sort-Object)
    if(!$values.Count) { return $null }
    [ordered]@{n=$values.Count;mean=($values|Measure-Object -Average).Average;median=$values[[int][math]::Floor(($values.Count-1)*.5)];p95=$values[[int][math]::Ceiling(($values.Count-1)*.95)];p99=$values[[int][math]::Ceiling(($values.Count-1)*.99)];max=$values[-1]}
}
$grouping=if($Kind -eq 'world') { @('stage') } else { @('round','actors','mode') }
$results=@(foreach($group in ($rows | Group-Object -Property $grouping)) {
    $sample=$group.Group[0];$mode=if($Kind -eq 'world') {$WorldMode} else {$sample.mode}
    $result=[ordered]@{kind=$Kind;group=$group.Name;mode=$mode;frames=$group.Count;metrics=[ordered]@{}}
    foreach($metric in @('cpu_skin_us','gpu_prep_us','deform_us','render_cpu_us','cpu_frame_us','process_cpu_us','world_cpu_us','wall_frame_us','gpu_frame_us','cpu_calls','cpu_vertices','bone_bytes','vertex_bytes','vertex_updates','draws','gpu_deforms','visible','fallbacks','static_meshes','palettes')) {
        if($sample.PSObject.Properties.Name -contains $metric) { $result.metrics[$metric]=Stats $group.Group $metric }
    }
    if($mode -eq 'gpu' -and ($result.metrics.cpu_calls.max -ne 0 -or $result.metrics.vertex_bytes.max -ne 0)) { throw "GPU CPU-deformation/upload in group $($group.Name)" }
    if($result.metrics.Contains('fallbacks') -and $result.metrics.fallbacks.max -ne 0) { throw 'Unexpected fallback.' }
    $result.fps=1000000/$result.metrics.wall_frame_us.mean
    $result
})
$results | ConvertTo-Json -Depth 8
