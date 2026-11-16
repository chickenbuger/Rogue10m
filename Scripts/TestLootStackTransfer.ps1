param(
    [ValidatePattern('^[A-Za-z0-9_]{1,32}$')][string]$RunLabel = 'LootCore01',
    [string]$CompilerPath = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root ('tmp\loot-stack-tests\' + $RunLabel)
if (Test-Path -LiteralPath $output) { throw 'Evidence exists; choose a fresh RunLabel.' }
if (-not $CompilerPath) {
    $found = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($found) { $CompilerPath = $found.Source }
    else {
        $candidate = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang++.exe'
        if (Test-Path -LiteralPath $candidate) { $CompilerPath = $candidate }
    }
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath)) {
    throw 'Pass -CompilerPath with a Windows clang++ toolchain. This test does not use Unreal Editor or UBT.'
}
$source = Join-Path $root 'Scripts\Tests\LootStackTransferTests.cpp'
$include = Join-Path $root 'Source\Rogue10m'
$header = Join-Path $include 'Components\Rogue10mLootStackTransfer.h'
foreach ($required in @($source, $header)) {
    if (-not (Test-Path -LiteralPath $required)) { throw ('Missing input: ' + $required) }
}
New-Item -ItemType Directory -Path $output | Out-Null
$executable = Join-Path $output 'LootStackTransferTests.exe'
& $CompilerPath --version 2>&1 | Set-Content -LiteralPath (Join-Path $output 'compiler.txt') -Encoding UTF8
if ($LASTEXITCODE -ne 0) { throw 'Compiler version query failed.' }
& $CompilerPath '-std=c++17' '-Wall' '-Wextra' '-Werror' '-pedantic' '-O2' $source '-I' $include '-o' $executable 2>&1 |
    Tee-Object -FilePath (Join-Path $output 'compile.log')
if ($LASTEXITCODE -ne 0) { throw ('Standalone compile failed; inspect ' + $output) }
$result = & $executable 2>&1
$testExit = $LASTEXITCODE
$result | Tee-Object -FilePath (Join-Path $output 'tests.log')
if ($testExit -ne 0 -or ($result -join "`n") -notmatch '^RESULT=LOOT_STACK_TRANSFER_PASSED cases=9 randomized=50000 checks=\d+$') {
    throw ('Standalone tests failed; inspect ' + $output)
}
[ordered]@{
    status = 'standalone_passed'
    runtime_integrated = $false
    unreal_editor_or_ubt_started = $false
    compiler = $CompilerPath
    header_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $header).Hash
    test_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
    result = ($result -join "`n")
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'summary.json') -Encoding UTF8
Write-Output ('Evidence: ' + $output)
