# 편집 가능한 스킬 VFX 패킷 재개 계획

공유 프로젝트의 수정 사용권은 통합 담당 Work에 있다. 이 Work는 `D:\Project\Rogue10m`을 읽기만 하고 물리적 격리 폴더에서 구현·검사·전달물을 만든다. 공유 Git, Editor, MCP, UBT, Cook, 게임을 실행하거나 종료하지 않는다.

1. 현재 Source·Scripts·증거·에셋 해시를 기존 패킷과 비교해 이미 연결된 Staff Primary01과 미통합 가독성 보정·미연결 후보를 구분한다.
2. 기존 DataAsset/Niagara/MIC 편집 계약을 유지한다. 피해 타이밍·피격 취소·카메라·스킬 catalog·무기 프로필은 보존하고 면역은 추가하지 않는다.
3. 오프라인 통합 준비 검사 도구를 추가한다. 파일별 원본/후보 SHA로 미반영·이미 반영·충돌을 판별하고 경로 이탈·링크·검사 중 변경은 중단한다. 도구는 파일을 적용하지 않는다.
4. 기존 계약 검사와 새 경로/동시 변경 검사를 독립 Python으로 실행한다. Staff C++와 31장 캡처 검사는 정적 검토와 실제 UE 실행을 구분한다.
5. 현재 기준 full 패킷과 이전 전달물 대비 delta, 파일 해시, 별도 한글 DevLog/Sprint 추가문을 제공한다. 공유 원본이 바뀌면 현재 기준을 다시 비교한다.

공유 `.agents/skills`는 이번 확인에서 없었다. AGENTS.md, Docs/LazyCodexHarness.md, Docs/FabAssetReuseGuide.md와 설치된 Unreal 작업 지침을 읽었다. 프로젝트 지침의 공유 빌드·기록 단계는 이번 읽기 전용 사용권으로 실행할 수 없으므로 격리 증거와 통합 전제조건으로 남긴다.

완료 판단에는 fresh saved readback, cold reload, 기본 3인칭 cue 가독성, 피격 취소, 실제 입력, Cook/Windows 패키징이 필요하다. 과거 숫자 검사와 PNG는 이번 후보의 렌더 완료 증거로 사용하지 않는다.
