param(
    [string]$PlanPath = (Join-Path $PSScriptRoot 'route-plan.json'),
    [string]$ReportPath = (Join-Path $PSScriptRoot 'static-validation.json')
)

# Design arithmetic only: never launches Unreal or changes gameplay/content.
$ErrorActionPreference = 'Stop'
$plan = Get-Content -LiteralPath $PlanPath -Raw -Encoding UTF8 | ConvertFrom-Json
$checks = [System.Collections.Generic.List[object]]::new()
$nodes = @{}
$adjacency = @{}
function Assert-Design([string]$Name, [bool]$Passed) {
    $checks.Add([pscustomobject]@{ name = $Name; passed = $Passed })
}
foreach ($node in $plan.nodes) {
    $nodes[$node.id] = $node
    $adjacency[$node.id] = [System.Collections.Generic.List[string]]::new()
}
Assert-Design 'unique_node_ids' ($nodes.Count -eq $plan.nodes.Count)
Assert-Design 'five_villages' (@($plan.nodes | Where-Object kind -eq 'village').Count -eq 5)
Assert-Design 'design_only_no_runtime_measurement' (($plan.evidence_kind -eq 'design_static_only') -and (-not $plan.runtime_measured))
$edgeKeys = [System.Collections.Generic.HashSet[string]]::new()
foreach ($edge in $plan.edges) {
    Assert-Design "endpoints_$($edge.a)_$($edge.b)" ($nodes.ContainsKey($edge.a) -and $nodes.ContainsKey($edge.b) -and $edge.a -ne $edge.b)
    Assert-Design "open_walk_both_ways_$($edge.a)_$($edge.b)" ($edge.bidirectional -and @($edge.required_quests).Count -eq 0 -and $edge.transition -eq 'walk_same_world')
    $key = (@($edge.a, $edge.b) | Sort-Object) -join '|'
    Assert-Design "unique_edge_$key" ($edgeKeys.Add($key))
    $adjacency[$edge.a].Add($edge.b)
    $adjacency[$edge.b].Add($edge.a)
}
function Get-Reachable([string]$Start, [string[]]$Blocked) {
    $visited = [System.Collections.Generic.HashSet[string]]::new()
    $queue = [System.Collections.Generic.Queue[string]]::new()
    if ($Blocked -notcontains $Start) { [void]$visited.Add($Start); $queue.Enqueue($Start) }
    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        foreach ($next in $adjacency[$current]) {
            if ($Blocked -notcontains $next -and $visited.Add($next)) { $queue.Enqueue($next) }
        }
    }
    return ,$visited
}
Assert-Design 'all_nodes_reachable' ((Get-Reachable 'S' @()).Count -eq $nodes.Count)
$villageIds = @($plan.nodes | Where-Object kind -eq 'village' | ForEach-Object id)
Assert-Design 'boss_reachable_skipping_all_villages' ((Get-Reachable 'S' $villageIds).Contains('B'))
foreach ($villageId in $villageIds) {
    Assert-Design "boss_reachable_without_$villageId" ((Get-Reachable 'S' @($villageId)).Contains('B'))
}
# Every geometric crossing must be an explicit shared endpoint/junction.
for ($i = 0; $i -lt $plan.edges.Count; $i++) {
    for ($j = $i + 1; $j -lt $plan.edges.Count; $j++) {
        $a = $plan.edges[$i]; $b = $plan.edges[$j]
        if (@($a.a, $a.b) -contains $b.a -or @($a.a, $a.b) -contains $b.b) { continue }
        $p = $nodes[$a.a].xy; $p2 = $nodes[$a.b].xy
        $q = $nodes[$b.a].xy; $q2 = $nodes[$b.b].xy
        $rx = $p2[0] - $p[0]; $ry = $p2[1] - $p[1]
        $sx = $q2[0] - $q[0]; $sy = $q2[1] - $q[1]
        $denominator = $rx * $sy - $ry * $sx
        if ([Math]::Abs($denominator) -lt 0.000001) { continue }
        $dx = $q[0] - $p[0]; $dy = $q[1] - $p[1]
        $t = ($dx * $sy - $dy * $sx) / $denominator
        $u = ($dx * $ry - $dy * $rx) / $denominator
        Assert-Design "no_unmodelled_crossing_${i}_${j}" (-not ($t -ge 0 -and $t -le 1 -and $u -ge 0 -and $u -le 1))
    }
}
$routeResults = @()
foreach ($route in $plan.routes) {
    $distance = 0.0
    $legal = $true
    for ($i = 1; $i -lt $route.nodes.Count; $i++) {
        $previous = $route.nodes[$i - 1]; $next = $route.nodes[$i]
        if (-not $adjacency[$previous].Contains($next) -or -not $adjacency[$next].Contains($previous)) { $legal = $false }
        $a = $nodes[$previous].xy; $b = $nodes[$next].xy
        $distance += [Math]::Sqrt([Math]::Pow($a[0] - $b[0], 2) + [Math]::Pow($a[1] - $b[1], 2))
    }
    $seconds = $distance / $plan.timing.walk_speed_mps_assumed
    Assert-Design "route_$($route.id)_legal_and_reversible" $legal
    Assert-Design "route_$($route.id)_distance_matches" ([Math]::Abs($distance - $route.expected_length_m) -lt 0.01)
    Assert-Design "route_$($route.id)_walk_time_matches" ([Math]::Abs($seconds - $route.expected_walk_seconds) -lt 0.01)
    $routeResults += [pscustomobject]@{ id = $route.id; length_m_lower_bound = [Math]::Round($distance, 3); walk_seconds_lower_bound = [Math]::Round($seconds, 3); runtime_measured = $false }
}
$direct = @($plan.routes | Where-Object id -eq 'direct')[0]
Assert-Design 'direct_visits_no_village' (@($direct.nodes | Where-Object { $villageIds -contains $_ }).Count -eq 0)
Assert-Design 'direct_geometry_under_60_seconds' ($direct.expected_walk_seconds -le $plan.timing.direct_entry_target_seconds)
$five = @($plan.routes | Where-Object id -eq 'five_visits')[0]
Assert-Design 'five_visit_example_contains_every_village' (@($villageIds | Where-Object { $five.nodes -notcontains $_ }).Count -eq 0)
$revisit = @($plan.routes | Where-Object id -eq 'revisit')[0]
Assert-Design 'revisit_returns_to_V1_and_skips_V2_V5' (@($revisit.nodes | Where-Object { $_ -eq 'V1' }).Count -eq 2 -and $revisit.nodes -notcontains 'V2' -and $revisit.nodes -notcontains 'V5')
foreach ($budget in $plan.activity_budgets) {
    $sum = ($budget.activities | Measure-Object seconds -Sum).Sum
    Assert-Design "budget_$($budget.route)_900_seconds_to_entrance" ($sum -eq 900 -and $sum -eq $budget.seconds -and $plan.timing.endpoint -eq 'boss_entrance_crossing')
    Assert-Design "budget_$($budget.route)_no_duplicate_activity_id" (@($budget.activities.id | Select-Object -Unique).Count -eq $budget.activities.Count)
    $route = @($plan.routes | Where-Object id -eq $budget.route)[0]
    $travel = @($budget.activities | Where-Object kind -eq 'travel')[0]
    Assert-Design "budget_$($budget.route)_travel_covers_geometry" ($travel.seconds -ge $route.expected_walk_seconds)
}
Assert-Design '15_min_entry_plus_3_min_boss_plus_2_min_reserve_equals_cap' (($plan.timing.content_entry_target_seconds + $plan.timing.boss_combat_budget_seconds + $plan.timing.contingency_budget_seconds) -eq $plan.timing.run_cap_seconds)
$failures = @($checks | Where-Object { -not $_.passed })
$report = [ordered]@{
    status = $(if ($failures.Count -eq 0) { 'static_checks_passed' } else { 'static_checks_failed' })
    generated_utc = (Get-Date).ToUniversalTime().ToString('o')
    evidence_kind = 'design_static_only'
    runtime_validation = 'not_run'
    node_count = $nodes.Count
    edge_count = $plan.edges.Count
    check_count = $checks.Count
    failure_count = $failures.Count
    route_results = $routeResults
    checks = $checks
    limitations = @('Local 2D straight segments only; terrain/collision not measured.', 'Graph reversibility does not establish runtime quest persistence.', 'No editor/build/PIE/physical-input/full-run validation executed.')
}
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
Write-Output "$($report.status): $($checks.Count) checks, $($failures.Count) failures; runtime validation not run."
if ($failures.Count -gt 0) { throw 'Design verification failed. See static-validation.json.' }
