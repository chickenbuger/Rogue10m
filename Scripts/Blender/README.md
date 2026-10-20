# Rogue10m Blender MCP

사용자가 지정한 [hassledzebra/codex_blender_mcp](https://github.com/hassledzebra/codex_blender_mcp)의 실제 stdio 서버와 live Blender 애드온을 사용한다. 해당 저장소에는 `SKILL.md`가 없다. 설치된 것은 Codex 스킬 파일이 아닌 MCP 연결이다.

- upstream commit: `3a6eb8b69252a651e5c1eeff543486e108d56bb6`
- local source: `tmp/codex-blender-mcp-source` (배포 패키지에 포함하지 않음)
- Python environment: `tmp/blender-mcp-venv`
- Blender: `tmp/apollo-tools/blender-4.5.14-windows-x64/blender.exe`, 4.5.14 LTS
- Blender scripts/preferences: `tmp/blender-mcp-user/scripts`, `tmp/blender-mcp-user/config`
- transport: Codex stdio MCP → 실제 live addon, `127.0.0.1:9877`

Blender 시작:

```powershell
.\Scripts\StartBlenderMCP.ps1 -BlendFile .\Art\NPC\Arin\Arin.blend
```

이미9877포트를 사용하는 인스턴스가 있으면 시작 스크립트는 중복 실행을 멈춘다. 현재 소유 인스턴스는 `Feature/doc/evidence/arin-blender-20261007/bootstrap.json`에서 확인한다. 애드온은 `Codex MCP` 사이드바에 표시되며 시작 스크립트가 loopback 서버를 켠다. 별도로 일반 Blender를 실행할 때는 위 프로젝트 전용 환경과 설정이 자동 적용되지 않으므로 시작 스크립트를 사용한다.

현재 턴에서 실제 stdio 도구를 호출하는 방법:

```powershell
.\tmp\blender-mcp-venv\Scripts\python.exe .\Scripts\BlenderMCP.py --tool blender_ping
.\tmp\blender-mcp-venv\Scripts\python.exe .\Scripts\BlenderMCP.py --tool blender_exec_python --script Scripts/Blender/BuildArin.py
.\tmp\blender-mcp-venv\Scripts\python.exe .\Scripts\BlenderMCP.py --tool blender_exec_python --script Scripts/Blender/ValidateArin.py
```

`BuildArin.py`는 **제작용 인스턴스의 현재 씬을 교체**한다. 사용자 작업 씬에서 실행하지 않는다. `ValidateArin.py`도 실제 FBX 검사용 씬으로 바꾼다. 검증 후 `blender_open_file`로 `Arin.blend`를 다시 연다. 실시간 호출은 하나씩 실행한다. timeout이 발생해도 Blender에서 작업이 계속될 수 있으므로 수정 명령을 무작정 재시도하지 않는다.

Codex에는 `blender` MCP 항목을 등록했다. 현재 턴은 위 실제 client로 검증했으며, 새 native MCP 도구 목록은 Codex 연결을 다시 읽는 다음 세션에 표시된다. 기존 `unreal-mcp`와 `node_repl` 항목은 유지한다.

`BootstrapMCP.py`/`StartBlenderMCP.ps1`/`BlenderMCP.py`는 프로젝트 연결 래퍼다. 실제 upstream 서버/애드온 원본은 수정하지 않는다. upstream에는 확인한 시점 기준 LICENSE 파일이 없어 vendor 코드를 제작 모델 배포물에 재포장하지 않았다.

## 아린 Refined v2

현재 NPC에는 [Refined/Arin.blend](D:/Project/Rogue10m/Art/NPC/Arin/Refined/Arin.blend)와 같은 최종 FBX를 적용했다. [파일·UV/PBR·골격·엔진 전달 설명](D:/Project/Rogue10m/Art/NPC/Arin/Refined/README.md)과 [품질 개선 결과](D:/Project/Rogue10m/Feature/doc/2026-10-07_arin-quality-refinement.md)를 기준으로 사용한다. v1 경로는 기준선이며 재제작 시 v2 파일을 덮어쓰지 않도록 출력 폴더를 구분한다.

`BuildRefinedArin.py`는 새 background Blender 프로세스에서 `--factory-startup --python-exit-code 1`로 실행해 실시간 작업 씬과 분리했다. `InspectArinQuality.py`는 BLEND/FBX/GLB 각각 새 프로세스로 검증하며, `RenderArinQuality.py`는 실제 export 메시의 다각도·변형 자세를 렌더한다. 최종 런타임 모델은 4atlas·16PNG·23제작 본·2초 Idle이다. UE 생성 LOD는 원본 FBX에 포함되지 않으며, `ApplyRefinedArin.py`의 현재 경계 보호 설정과 엔진 결과를 사용한다.

최종 파일과 검사 입력의 동일성 및 UE 저장 에셋 해시는 [final-handoff.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-handoff.json)에 있다. 기존 v1과 실패 후보·Blender 저장 백업은 품질 evidence 아래에 보존했고, 최종 아트 폴더에서는 `.blend1`을 제외했다.
