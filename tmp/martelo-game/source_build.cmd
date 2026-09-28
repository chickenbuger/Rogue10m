@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc /MD /DFBXSDK_SHARED /I"D:\Program Files\UE_5.8\Engine\Source\ThirdParty\FBX\2020.2\include" source_bind_proxy.cpp /Fe:source_bind_proxy.exe /link /LIBPATH:"D:\Program Files\UE_5.8\Engine\Source\ThirdParty\FBX\2020.2\lib\vs2017\x64\release" libfbxsdk.lib
