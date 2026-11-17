# 맵 통합 체크포인트 B — 비GUI 지형 기준값 확보

## 실제 완료

사용자의 `Compute use 사용하지말고 작업해줘` 지시 이후 CUA·화면·마우스·키보드 조작은 0회다. CUA 종료 요약 메타데이터만 기록했다. 이전에는 창 전면화 Raise 1회와 화면/접근성 조회를 사용했으며 Recovery 버튼, 지형 삭제, 키 입력, 저장은 실행하지 않았다. Recovery 창은 우리 입력 없이 사라졌으므로 선택 내용을 추정하지 않는다.

공식 Python Remote Execution에서 정확한 후보 A, PIE false, dirty map/content 0을 확인했다. 원본 2,478개와 후보 1,419개 파일 SHA256도 전부 동일했다. 시작 인자의 공백 때문에 `ModelContextProtocol.StartServer`가 인자 없이 실행되어 포트 8000으로 바뀐 오류를 진단했고, 공식 API에 명시적 8001 명령을 전달하여 복구했다. MCP ListSkills 읽기가 성공했다. 원래 에디터 PID 17560은 dirty 0 확인 후 공식 QuitEditor로 정상 종료했다.

물리 백업과 사전 해시를 확인한 뒤 소유 helper 2개만 v2로 갱신했다. 정상 UHT/Editor 빌드 exit 0, 30.55초(UBT 29.94초). DLL SHA256은 `D59CFD58E24090E4D3DFCB311B8ABF01136BDAE4EAD19C9AFBEA0D9301478AA5`다. 이어 숨김 NullRHI Python commandlet PID 12828이 exit 0, 21.67초로 끝났다.

| 실제 후보 검사 | 결과 |
| --- | --- |
| 월드 | `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A` |
| 현재 범위 | X/Y -900..900m, **아직 1800×1800m** |
| 지형/충돌/프록시 | 256 / 256 / 16, 전부 로드·등록 |
| 샘플/배율 | 1009×1009, XY 178.571428571cm, Z 256 |
| 편집 레이어 | Layer 1개, 기존 GUID 유지 |
| 유지 예정 ROI | X 315..882, Y 630..945; 568×316 =179,488샘플 |
| 검사 데이터 | composite와 raw GUID별 RG 높이 uint16, 페인트 uint8 두 층, 메타데이터 |
| 이음새 | 각 채널 경계 중복 4,832개 일치 |
| dirty/저장/선택/축소 | dirty 0, 저장 0, 선택 0, 축소 0 |

BLAKE3 semantic 기준값은 `623961bac1faadd4138952f15694b5c54ae2226f51afba9acd5b7fd5857a9e1c`다. 파일 SHA256과 구분한다. CPU Texture.Source의 RG 높이와 페인트 비교이며 GPU 결과, 원시 레이어의 B/A alpha/flags 전체, 충돌 높이 및 플레이 검증을 포함하지 않는다.

## 보류와 다음 작업

9×5 격자안은 1012.5×562.5m, 유지 45개·제외 211개이며 예상 XY 범위는 (-337.5,225)..(675,787.5)m다. 현재 preview는 확인 인자 false 때문에 `success:false / confirmation_required:true`를 반환했고 실제 선택은 하지 않았다. 이는 예상 동작이며 축소 통과가 아니다.

native Landscape Manage/Delete는 현재 GUI 금지로 보류한다. private EdMode 호출이나 단순 Component 파괴로 우회하지 않는다. public Import는 새 레이어 GUID 발급, 브러시 소유권 이동, 원시 B/A 보존 차이가 있어 full fidelity 보존 검토가 끝나기 전 실행하지 않는다. 원본/후보/기본 플레이맵·cook 목록은 바꾸지 않았다.

물리 입력 직행 3회, 성장 15분, 완주, 실제 편집 Undo/저장 재로드, 시각 검수, 새 Windows 패키지와 종료 UI 검증은 미완료다. 이전 프로그램 보행 48.537초를 이번 물리 검증으로 재사용하지 않는다. Quit fixture도 OnClicked·Slate·InputKey를 사용하므로 NullRHI만으로 금지 예외가 되지 않는다.

다음 허용 작업은 검토된 G08+VFX 누적 패치와 NPC full 패킷의 해시/백업 후 소스 통합, 정규 빌드 및 GUI 입력 없는 native 모델/타이머 테스트다. Windows candidate 도구는 UNSEALED로 유지한다. 기능 완료·공개 승격은 아직 하지 않는다.

## 근거

- [원본/후보 해시](evidence/map-resume-20261113/LiveEditorA/protected-hash-post-recovery.json)
- [정규 helper 빌드](evidence/map-resume-20261113/RetainedSnapshotBuildA/result.json)
- [실제 commandlet](evidence/map-resume-20261113/terrain-snapshot-a-exit.json)
- [현재 지형](evidence/map-resume-20261113/TerrainSnapshotA/terrain.json)
- [샘플 기준값](evidence/map-resume-20261113/TerrainSnapshotA/retained-data.json)
- [저장/선택/GUI 경계](evidence/map-resume-20261113/TerrainSnapshotA/result.json)
