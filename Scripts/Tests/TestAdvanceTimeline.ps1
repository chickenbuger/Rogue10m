param(
    [string]$CompilerBin='C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64',
    [string]$MsvcRoot='C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207',
    [string]$WindowsSdk='C:\Program Files (x86)\Windows Kits\10',
    [string]$SdkVersion='10.0.22621.0'
)
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$out=Join-Path $root 'Delivery\StandaloneTimeline'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$compiler=Join-Path $CompilerBin 'cl.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw ('Standalone compiler missing: '+$compiler) }
$env:PATH=$CompilerBin+';'+$env:PATH
$source=Join-Path $PSScriptRoot 'test_advance_timeline.cpp'
$exe=Join-Path $out 'test_advance_timeline.exe'
$obj=Join-Path $out 'test_advance_timeline.obj'
& $compiler /nologo /std:c++17 /EHsc /W4 /WX /MT ('/I'+(Join-Path $MsvcRoot 'include')) ('/I'+(Join-Path $WindowsSdk ('Include\'+$SdkVersion+'\ucrt'))) $source ('/Fe:'+ $exe) ('/Fo:'+ $obj) /link ('/LIBPATH:'+(Join-Path $MsvcRoot 'lib\x64')) ('/LIBPATH:'+(Join-Path $WindowsSdk ('Lib\'+$SdkVersion+'\ucrt\x64'))) ('/LIBPATH:'+(Join-Path $WindowsSdk ('Lib\'+$SdkVersion+'\um\x64')))
if ($LASTEXITCODE -ne 0) { throw ('Standalone timeline compilation failed: '+$LASTEXITCODE) }
& $exe
if ($LASTEXITCODE -ne 0) { throw ('Standalone timeline test failed: '+$LASTEXITCODE) }
