#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$launcher = Join-Path $root 'Scripts/VerifyFiveVillageTravel.ps1'
$label = 'Plan' + [guid]::NewGuid().ToString('N').Substring(0,8)
$evidence = 'C:\Users\PC\tmp\FiveVillageSpeedContractE'
$checks = [Collections.Generic.List[object]]::new()
$script:LaunchAttempts = 0
# Shadow the process cmdlet in this isolated test scope. No UE process may start.
function Start-Process { $script:LaunchAttempts++; throw 'Offline test forbids process launch.' }
function Check([bool]$Good, [string]$Name) { $checks.Add([pscustomobject]@{name=$Name;passed=$Good}) }
function Read-Plan([string]$Mode, [switch]$Headless) {
    $text = & $launcher -Label $label -Scenario $Mode -NullRHI:$Headless
    return (($text -join "`n") | ConvertFrom-Json -NoEnumerate)
}
$both = Read-Plan 'Both'
Check ($both.status -eq 'plan_only_not_executed') 'default_is_plan_only'
Check ($both.cases.Count -eq 2) 'both_has_two_separate_cases'
Check ($both.cases[0].scenario -ceq 'Direct' -and $both.cases[1].scenario -ceq 'Revisit') 'canonical_case_order'
Check ($both.outcome -ceq 'Abandoned' -and -not $both.victory_certified -and -not $both.physical_input) 'scope_is_abandon_programmatic_travel'
Check ($both.cases[0].profileSlot -cne $both.cases[1].profileSlot) 'separate_profile_slots'
Check ($both.cases[0].progressionPrefix -cne $both.cases[1].progressionPrefix) 'separate_progression_prefixes'
Check ($both.cases[0].skillSlot -cne $both.cases[1].skillSlot) 'separate_skill_slots'
foreach ($case in $both.cases) {
    Check ($case.arguments -contains '/Game/Rogue10m/Maps/StartHub') ($case.scenario + '_starts_in_normal_hub')
    Check ($case.arguments -contains '-ExecCmds="t.MaxFPS 60,Rogue10m.TestFiveVillageTravel"') ($case.scenario + '_uses_real_fixture_command')
    Check ($case.arguments -contains '-RenderOffScreen' -and $case.arguments -notcontains '-nullrhi') ($case.scenario + '_rendered_default')
    Check ($case.runtime.EndsWith('runtime')) ($case.scenario + '_runtime_has_separate_fresh_child')
    Check (-not (Test-Path -LiteralPath $case.evidence)) ($case.scenario + '_plan_creates_no_case_directory')
}
$direct = Read-Plan 'direct' -Headless
Check ($direct.cases.Count -eq 1 -and $direct.cases[0].scenario -ceq 'Direct') 'case_alias_is_canonicalized'
Check ($direct.cases[0].arguments -contains '-nullrhi' -and $direct.cases[0].arguments -notcontains '-RenderOffScreen') 'explicit_headless_plan'
$revisit = Read-Plan 'Revisit'
Check ($revisit.cases.Count -eq 1 -and $revisit.cases[0].timeoutSeconds -ge 300) 'longer_revisit_budget'
$guardBlocked = $false
try { & $launcher -Label $label -Execute | Out-Null } catch { $guardBlocked = $_.Exception.Message.Contains('handoff') }
Check $guardBlocked 'execute_requires_actual_owner_handoff_before_io_or_launch'
$labelBlocked = $false
try { & $launcher -Label '../unsafe' | Out-Null } catch { $labelBlocked = $true }
Check $labelBlocked 'path_traversal_label_rejected'
$lengthBlocked = $false
try { & $launcher -Label ('A' * 21) | Out-Null } catch { $lengthBlocked = $true }
Check $lengthBlocked 'overlong_namespace_rejected'
$unicodeBlocked = $false
try { & $launcher -Label '마을' | Out-Null } catch { $unicodeBlocked = $true }
Check $unicodeBlocked 'non_ascii_namespace_rejected'
$taskTemp = Join-Path ([IO.Path]::GetTempPath()) ('Rogue10mTravelPlan-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($taskTemp) | Out-Null
$packagedStub = Join-Path $taskTemp 'Rogue10m.exe'
[IO.File]::WriteAllText($packagedStub, 'Offline filename guard; never executed.')
$packagedBlocked = $false
try { & $launcher -Label $label -Execute -EditorHandoff 'offline target guard' -EditorExe $packagedStub | Out-Null }
catch { $packagedBlocked = $_.Exception.Message.Contains('packaged game EXE') }
Check $packagedBlocked 'packaged_executable_rejected_before_profile_or_process_operations'
Check ($script:LaunchAttempts -eq 0) 'zero_process_launch_attempts'
Check (-not (Test-Path -LiteralPath (Split-Path -Parent $both.cases[0].evidence))) 'no_label_evidence_created_by_plan'
$failed = @($checks | Where-Object { -not $_.passed })
[IO.Directory]::CreateDirectory($evidence) | Out-Null
[ordered]@{status=if($failed.Count){'offline_plan_tests_failed'}else{'offline_plan_tests_passed'}
    recorded_utc=[DateTime]::UtcNow.ToString('o');checks=$checks.Count;failures=$failed.Count
    process_launch_attempts=$script:LaunchAttempts;source_modified=$false;unreal_editor='not_run';unreal_build_tool='not_run'
    actual_travel='not_run';results=@($checks.ToArray())} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $evidence 'offline-plan-tests.json') -Encoding utf8NoBOM
Write-Output ('Offline launcher plan checks=' + $checks.Count + ' failures=' + $failed.Count + ' process attempts=' + $script:LaunchAttempts)
if ($failed.Count) { throw 'Offline plan test failed.' }
