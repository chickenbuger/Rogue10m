#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$ModelPath = '',
    [string]$ReportPath = '',
    [ValidateRange(5,60)][int]$TimeoutSeconds = 30
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$taskRoot = [IO.Path]::GetFullPath((Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$taskEvidence = Join-Path $taskRoot 'Feature/doc/evidence/five-village-quests-20261111'
if ([string]::IsNullOrWhiteSpace($ModelPath)) {
    $ModelPath = Join-Path $taskRoot 'Feature/implementation/five-village-quests-20261111/Source/Rogue10m/World/Rogue10mVillageQuestModel.h'
}
if ([string]::IsNullOrWhiteSpace($ReportPath)) { $ReportPath = Join-Path $taskEvidence 'portable-tests.json' }
$taskModel = (Resolve-Path -LiteralPath $ModelPath -ErrorAction Stop).Path
$taskTest = Join-Path $PSScriptRoot 'VillageQuestModel20261111.cpp'
if (-not (Test-Path -LiteralPath $taskTest -PathType Leaf -ErrorAction Stop)) { throw 'Portable quest test source is missing.' }
if ([IO.Path]::GetFileName($taskModel) -cne 'Rogue10mVillageQuestModel.h') { throw 'Expected the reviewed quest model header.' }
if (-not $taskModel.StartsWith($taskRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Model must be a reviewed header inside this workspace.'
}
$taskReportPath = [IO.Path]::GetFullPath($ReportPath)
$taskAllowedReportRoots = @([IO.Path]::GetFullPath($taskEvidence), [IO.Path]::GetFullPath($env:TEMP))
$taskReportAllowed = $false
foreach ($taskBoundary in $taskAllowedReportRoots) {
    if ($taskReportPath.StartsWith([IO.Path]::TrimEndingDirectorySeparator($taskBoundary) + [IO.Path]::DirectorySeparatorChar,
                                  [StringComparison]::OrdinalIgnoreCase)) { $taskReportAllowed = $true }
}
if (-not $taskReportAllowed) { throw 'Report must be within this feature evidence directory or the temporary directory.' }
if (Test-Path -LiteralPath $taskReportPath -ErrorAction Stop) { throw 'Report already exists; preserve it and choose a fresh report path.' }
$taskTemp = Join-Path $env:TEMP ('Rogue10mVillageQuestModel_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($taskTemp) | Out-Null
[IO.Directory]::CreateDirectory((Split-Path -Parent $taskReportPath)) | Out-Null
$taskReport = [ordered]@{
    schema_version = 1
    status = 'not_started'
    recorded_utc = [DateTime]::UtcNow.ToString('o')
    scope = 'standalone standard C++ quest model only; no Unreal headers, UHT, UBT, or editor'
    model_path = [IO.Path]::GetRelativePath($taskRoot, $taskModel).Replace('\','/')
    model_sha256 = (Get-FileHash -LiteralPath $taskModel -Algorithm SHA256).Hash
    tests_path = [IO.Path]::GetRelativePath($taskRoot, $taskTest).Replace('\','/')
    tests_sha256 = (Get-FileHash -LiteralPath $taskTest -Algorithm SHA256).Hash
    runner_path = 'Scripts/Tests/TestVillageQuestModel20261111.ps1'
    runner_sha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    temporary_output_directory = $taskTemp
    compiler = $null; compiler_version = $null; toolchain_environment = $null
    compile_flags = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/permissive-')
    compile_exit_code = $null; execution_exit_code = $null
    compile_stdout = ''; compile_stderr = ''; execution_stdout = ''; execution_stderr = ''
    execution = $null; timed_out = $false; failure = $null
    shared_source_modified = $false; assets_config_modified = $false
    unreal_editor = 'not_run'; unreal_build_tool = 'not_run'; unreal_wrapper_validation = 'not_run'
}
$taskProcess = $null

function Invoke-QuestOwnedProcess {
    param([Parameter(Mandatory)][string]$Executable,[Parameter(Mandatory)][string[]]$Arguments,[Parameter(Mandatory)][string]$Prefix)
    $stdout = Join-Path $taskTemp ($Prefix + '.stdout.txt')
    $stderr = Join-Path $taskTemp ($Prefix + '.stderr.txt')
    $script:taskProcess = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $taskTemp -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $script:taskProcess.WaitForExit($TimeoutSeconds * 1000)) {
        $taskReport.timed_out = $true
        $script:taskProcess.Kill($true)
        $null = $script:taskProcess.WaitForExit(10000)
        throw ('Owned standalone ' + $Prefix + ' process timed out.')
    }
    $script:taskProcess.Refresh()
    return [pscustomobject]@{
        exit_code=$script:taskProcess.ExitCode
        stdout=if(Test-Path -LiteralPath $stdout){[IO.File]::ReadAllText($stdout)}else{''}
        stderr=if(Test-Path -LiteralPath $stderr){[IO.File]::ReadAllText($stderr)}else{''}
    }
}

try {
    $taskVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $taskVsWhere -PathType Leaf)) { throw 'Visual Studio discovery tool is unavailable.' }
    $taskInstall = @(& $taskVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -utf8)
    if ($LASTEXITCODE -ne 0 -or $taskInstall.Count -ne 1 -or [string]::IsNullOrWhiteSpace($taskInstall[0])) { throw 'A single installed MSVC toolchain was not discovered.' }
    $taskVcVars = Join-Path $taskInstall[0].Trim() 'VC/Auxiliary/Build/vcvars64.bat'
    if (-not (Test-Path -LiteralPath $taskVcVars -PathType Leaf)) { throw 'MSVC environment script is unavailable.' }
    # No filesystem actions cross shells. This child shell only prints the
    # toolchain environment; only four compiler-related variables are imported.
    $taskEnvironment = @(& $env:ComSpec /d /s /c ('call "' + $taskVcVars + '" >nul && set'))
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
    foreach ($taskLine in $taskEnvironment) {
        $taskEquals = $taskLine.IndexOf('=')
        if ($taskEquals -le 0) { continue }
        $taskName = $taskLine.Substring(0,$taskEquals)
        if ($taskName -in @('Path','INCLUDE','LIB','LIBPATH')) {
            [Environment]::SetEnvironmentVariable($taskName,$taskLine.Substring($taskEquals+1),'Process')
        }
    }
    $taskCompiler = (Get-Command cl.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $taskReport.compiler = $taskCompiler
    $taskReport.compiler_version = [Diagnostics.FileVersionInfo]::GetVersionInfo($taskCompiler).FileVersion
    $taskReport.toolchain_environment = $taskVcVars
    $taskExe = Join-Path $taskTemp 'quest-model-tests.exe'
    $taskObj = Join-Path $taskTemp 'quest-model-tests.obj'
    $taskPdb = Join-Path $taskTemp 'quest-model-tests.pdb'
    $taskArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/permissive-',
        ('/I"'+(Split-Path -Parent $taskModel)+'"'),('"'+$taskTest+'"'),
        ('/Fo"'+$taskObj+'"'),('/Fe"'+$taskExe+'"'),('/Fd"'+$taskPdb+'"'))
    $compile = Invoke-QuestOwnedProcess -Executable $taskCompiler -Arguments $taskArguments -Prefix compile
    $taskReport.compile_exit_code = $compile.exit_code
    $taskReport.compile_stdout = $compile.stdout; $taskReport.compile_stderr = $compile.stderr
    if ($compile.exit_code -ne 0 -or -not (Test-Path -LiteralPath $taskExe -PathType Leaf)) { throw 'Standalone C++ compilation failed.' }
    $execution = Invoke-QuestOwnedProcess -Executable $taskExe -Arguments @('standalone') -Prefix execution
    $taskReport.execution_exit_code = $execution.exit_code
    $taskReport.execution_stdout = $execution.stdout; $taskReport.execution_stderr = $execution.stderr
    if ($execution.exit_code -ne 0) { throw 'Standalone quest model tests failed.' }
    $result = $execution.stdout | ConvertFrom-Json -NoEnumerate -ErrorAction Stop
    if ($null -eq $result -or $result -is [array] -or $result.status -cne 'passed' -or $result.failures -ne 0 -or $result.checks -le 0) { throw 'Standalone result JSON is incomplete or failed.' }
    $taskReport.execution = $result
    if ((Get-FileHash -LiteralPath $taskModel -Algorithm SHA256).Hash -ne $taskReport.model_sha256 -or
        (Get-FileHash -LiteralPath $taskTest -Algorithm SHA256).Hash -ne $taskReport.tests_sha256) { throw 'Model/test source changed during standalone verification.' }
    $taskReport.status = 'portable_cpp_tests_passed'
} catch {
    $taskReport.status = 'portable_cpp_tests_failed'
    $taskReport.failure = $_.ToString()
} finally {
    if ($taskProcess -and -not $taskProcess.HasExited) {
        $taskProcess.Kill($true)
        $null = $taskProcess.WaitForExit(10000)
    }
    $taskReport.finished_utc = [DateTime]::UtcNow.ToString('o')
    # CreateNew also prevents overwriting a report created by another verifier
    # after this process's preflight check.
    $taskJson = $taskReport | ConvertTo-Json -Depth 12
    $taskStream = [IO.File]::Open($taskReportPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    $taskWriter = [IO.StreamWriter]::new($taskStream, [Text.UTF8Encoding]::new($false))
    try { $taskWriter.WriteLine($taskJson) }
    finally { $taskWriter.Dispose() }
    Write-Output ('Quest portable test status=' + $taskReport.status + ' report=' + $taskReportPath)
}
if ($taskReport.status -cne 'portable_cpp_tests_passed') { throw $taskReport.failure }
