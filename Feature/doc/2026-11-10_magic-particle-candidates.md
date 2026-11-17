# 마법 파티클 후보 제작 결과

2026-11-10 UTC. **실제 Niagara 4종과 전용 머티리얼 4개를 생성·컴파일·저장했고, 공식 Unreal MCP로 실제 렌더와 만료를 확인했다.** 스킬 참조·게임플레이 동작에는 연결하지 않았다. 에디터 PID26632는 정상 종료했으며 21:38:20 UTC 기준 UE·빌드·셰이더 프로세스는 0개다.

![실제 후보 4종](evidence/magic-particle-candidates-20261110/MagicMcp01/render-clean/all-mid.png)

화면 왼쪽부터 적중, 방출, 마법진, 예고다. [만료 후 실제 화면](evidence/magic-particle-candidates-20261110/MagicMcp01/render-clean/all-expired.png)에서는 모두 사라진다. 공중 스프라이트 후보이며, 지면 AoE·이동 투사체·실제 적중 판정은 구현하지 않았다.

## 저장한 자산

공통 경로: `/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01/`. 아래 각 행은 `NS_` Niagara와 같은 이름의 `M_` 머티리얼 한 쌍이다.

| Niagara 이름 | 형태 / 제안 크기·수명 |
| --- | --- |
| NS_MagicTelegraph_Preparation_Candidate | 네 모서리 / 40×40cm·.30초 |
| NS_MagicCircle_FloatingCasterSigil_Candidate | 빈 원과 여섯 획 / 64×64cm·.48초 |
| NS_MagicRelease_DirectionalFork_Candidate | 방향 섬광 / 64×24cm·.16초 |
| NS_MagicImpact_DiamondStar_Candidate | 별과 마름모 / 44×44cm·.22초 |

확정된 스킬 수치가 아닌 시각 검수용 제안이다. CPU emitter1·burst1·particle1, Once/Complete, world space·FaceCamera·고정 bounds, velocity0, shadow/light/collision/ribbon/decal/camera-offset 없음으로 저장됐다. EffectType은 None을 유지했다. 검수 component scale과 time dilation은1이었다.

## 실제 검증과 한계

- **native MCP 생성·저장:** [Niagara 결과](evidence/magic-particle-candidates-20261110/MagicMcp01/author-niagara/summary.json), [저장 후 재확인](evidence/magic-particle-candidates-20261110/MagicMcp01/author-verify/summary.json), [머티리얼 결과](evidence/magic-particle-candidates-20261110/MagicMcp01/mcp-materials/summary.json). NS4개 UpToDate·오류0·경고0. 재질 recompile 호출 성공과 이후 실제 렌더를 각각 기록했다.
- **실제 렌더:** [clean manifest](evidence/magic-particle-candidates-20261110/MagicMcp01/render-clean/manifest.json)의8캡처. 초기 아이콘 간섭 화면도 `render/`에 보존했다. clean 화면에서 네 형상이 구분됐고 early/mid/late를 확인했다. 1/240초 manual advance 후 pause한 장면이며 요청한 정규화 age를 실제 dataset age 측정치로 주장하지 않는다.
- **수명 종료:** requested age3의 만료 화면에서 발광 픽셀이 사라졌고 NS 컴포넌트4개 모두 inactive였다. 파티클 내부 dataset 수를 읽은 검사는 아니다.
- **보존·정리:** [최종 검증](evidence/magic-particle-candidates-20261110/MagicMcp01/final-verification.json). 보호 원본·스킬·맵12파일 및 관련2011파일 해시 일치. 카메라는 1e-6 허용 오차 안에서 복원했다. [임시 액터 정리](evidence/magic-particle-candidates-20261110/MagicMcp01/preview-cleanup.json): 액터4개 제거·baseline복원·dirty map/content0. [종료 증거](evidence/magic-particle-candidates-20261110/MagicMcp01/shutdown-processes.json): PID26632 종료·관련 프로세스0.
- **오프라인 검사:** 4검사군104검사 통과, Python6파일 AST 파싱 통과. [준비 증거](evidence/magic-particle-candidates-20261110/preparation/MagicPrep01/summary.json)의 UV 미리보기는 실제 Unreal 캡처와 구분한다. C++ 변경이 없어 빌드는 실행하지 않았다.

검수는 `/Engine/Maps/Entry`의 어두운 D3D11 offscreen 에디터 장면이다. cold reload·실제 전투 배경·밝은 지면·다중 적 가독성·GPU 비용·물리 입력·전체 플레이는 검증하지 않았다. CPU1파티클 설정만으로 성능 통과를 주장하지 않는다.

## 재사용과 도구

원본은 프로젝트 `WeaponFantasy20261110/NS_KnuckleFlurry_Acceptance`와 전용 머티리얼이며 IronShell→엔진 DirectionalBurst 계보다. 새 후보의 모양·수명·크기와 world space·고정 bounds 등을 설정했고 기존 Staff/너클/대검/스킬 참조는 보존했다. Fab Portal 에셋이나 Combat Magic 애니메이션 상품의 라이선스를 이 VFX의 근거로 사용하지 않는다. 로컬 가용성·계정 보유·현재 실행 연결은 서로 다른 상태다.

실제 자산 저작과 캡처는 공식 Unreal MCP를 사용했다. MCP가 노출하지 않는 시뮬레이션 advance/pause와 임시 액터·Game View·정상 종료만 기존 UE Python bridge로 처리했다. 일반 샌드박스에서 실행한 세 번의 시작은 실제 편집기가 뜨지 않았고, 승인된 호스트 실행 한 개만 사용했다. preview의 API 이름 오류는 UE5.8 헤더를 확인해 수정했다. 이를 Niagara 생성 실패나 미확인 성공으로 숨기지 않았다.

새 스크립트는 [spec](../../Scripts/MagicParticleCandidateSpec20261110.py), [오프라인 준비](../../Scripts/PrepareMagicParticleCandidatePreview20261110.py), [MCP 저작](../../Scripts/Editor/AuthorMagicParticleCandidatesMCP20261110.py), [시작](../../Scripts/Editor/OpenMagicParticleCandidateSession20261110.py), [임시 검수 제어](../../Scripts/Editor/ReviewMagicParticleCandidates20261110.py), [실제 MCP 캡처](../../Scripts/CaptureMagicParticleCandidates20261110.py)다. 전체 범위와 롤백 경계는 [계획](../architect/2026-11-10_magic-particle-candidates.md)에 있다.

## 후속 작업과 부모 통합

다음 적용은 스킬별 이벤트·손/지팡이 소켓·애니메이션 notify 시점이 정해진 뒤 후보를 선택해 연결하는 별도 작업이다. 색·크기·시간은 사용자 검토가 가능한 후보로 남겼다. 외부 상품 구매나 새 약관 동의가 필요한 작업은 하지 않았다. 이 패킷에서 커밋·푸시는 하지 않았다.

공유 DevLog·SprintChangeLog와 Git 통합은 부모 조정자가 맡는다. [한국어 DevLog·Notion·Sprint 기록](evidence/magic-particle-candidates-20261110/MagicMcp01/parent-log-handoff.md)을 함께 전달한다.
