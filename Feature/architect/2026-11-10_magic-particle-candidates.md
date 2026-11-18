# 마법 파티클 후보 제작 계획과 실행 경계

2026-11-10 UTC. 사용자가 요청한 스킬 대비 파티클을 예고·공중 마법진·방출·적중의 네 후보로 제작한다. 현재 단계의 결과는 **Niagara 4종과 전용 머티리얼 4개 저장 및 실제 에디터 렌더 검수 완료**다. 스킬 연결은 별도 작업이다.

## 재사용 근거

원본은 `/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/NS_KnuckleFlurry_Acceptance`와 같은 폴더의 `M_KnuckleFlurry_Acceptance`다. 프로젝트 IronShell에서 엔진 DirectionalBurst로 이어지는 CPU 단발 시스템과 UV/Age 재질 구조를 복제한다. 원본을 편집하지 않고 별도 후보 경로로 분리한다. 기존 Staff·대검·너클 이펙트와 스킬 참조를 보존한다.

현재 로컬 에셋과 실제 시스템 구조를 MCP로 확인했다. 이 계보는 Fab Portal이나 Combat Magic 애니메이션의 계정 보유·이펙트 라이선스 증명이 아니다. 외부 상품을 다운로드하거나 구매하지 않았다. 애니메이션 상품과 별도 제작 VFX를 구분한다. 프로젝트 재사용 지침과 기존 마법 설계는 [마법 연계 계획](2026-11-10_magic-circle-niagara-integration-plan.md)을 따른다.

## 후보의 의미와 제안값

경로는 `/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01/`이다. 수치는 시각 검수용 제안이며 시전·판정·쿨다운 계약이 아니다.

| 역할 | 형태 | 스프라이트 크기 / 파티클 수명 | 범위 |
| --- | --- | --- | --- |
| Telegraph | 안으로 모이는 네 모서리 | 40×40cm / .30초 | 입력·차징 성공 여부와 미연결 |
| Circle | 빈 원과 내부 여섯 획 | 64×64cm / .48초 | 카메라를 보는 공중 문양. 지면 AoE 아님 |
| Release | 좁고 방향성 있는 갈래 섬광 | 64×24cm / .16초 | 정지한 방출 표식. 이동 투사체 아님 |
| Impact | 별과 확장하는 마름모 | 44×44cm / .22초 | 실제 적중 이벤트와 미연결 |

각 시스템은 CPU emitter 1개·burst 1개·particle 1개, velocity 0, Once/Complete, ParticleState 수명 종료 제거, world space, FaceCamera/Unaligned, SortNone이다. component scale·dilation은 검수에서 1이었다. 고정 bounds는 스프라이트 반대각선+2cm를 기준으로 한다. light·collision·ribbon·decal·post process·camera offset은 넣지 않는다. EffectType은 기존 None을 유지하며 새 분류를 만들지 않는다. UV와 ParticleRelativeTime으로 모양을 표현한다.

## Ultrawork Packet

| 패킷 / 역할 | 목표와 변경 영역 | 완료 조건과 검증 | 롤백 경계 |
| --- | --- | --- | --- |
| A Architect/Implementer | 독립 spec·HLSL·오프라인 미리보기 | 4검사군104검사, 유한값·수명 양끝·경로/label 보호 | 새 Scripts 2개와 준비 증거만 제거 |
| B Content | 소유권 인계 후 공식 MCP로 4M+4NS 복제·설정·컴파일·저장 | 최신 schema/AgentSkill 확인, 실제 readback·compile·stack·원본 해시 | 새 MagicMcp01 폴더만 대상으로 삼음 |
| C Reviewer | 실제 임시 장면 렌더·종료·보존 확인 | 공식 CaptureViewport, 만료 후 inactive4, 액터 baseline/미저장0, 보호 파일 해시 | 임시 Entry 액터만 정리하고 카메라/표시 상태 복원 |
| D Doc Writer | 실제 결과·미연결·미검증 경계 기록 | 결과 문서와 부모 통합용 DevLog/Sprint 기록 | 공유 로그와 Git은 부모 조정자가 통합 |

## 실행 순서와 검증

1. 부모가 대검 편집기 종료와 에디터 사용권을 명시적으로 인계했다. 실제 호스트의 UE·빌드 프로세스 0을 재확인하고, 이 작업의 에디터 한 개만 실행했다. 일반 샌드박스 실행은 실제 호스트 편집기가 뜨지 않아 실패 기록을 보존했고, 승인된 호스트 실행으로 PID26632를 확보했다.
2. 공식 MCP의 toolset/schema·프로젝트 AgentSkill·PIE false·원본 topology/renderer/module/lifecycle·재질 그래프를 읽었다. 엔진/플러그인/프로젝트 자산 검색도 실시했다. 적합한 기존 스프라이트 시스템의 프로젝트 파생본을 선택했다.
3. 머티리얼은 기존 UV/Age 그래프를 복제하고 Custom HLSL과 tint만 변경했다. Niagara는 해당 원본에서 fresh 시스템을 만들고 burst/count/lifetime/size/velocity/lifecycle/renderer/fixed bounds를 설정했다. native Material recompile, Niagara compile/readback/stack 확인 후 8자산을 저장했다.
4. 공식 CaptureViewport로 실제 D3D11 offscreen 에디터 장면을 캡처했다. native MCP에 age/advance/pause가 없어 기존 로컬 UE Python bridge는 소유 임시 액터의 시간 제어·표시 상태·정리에만 사용했다. 원본 조회와 후보 자산 편집은 native MCP로 수행했다.
5. 처음 아이콘이 겹친 화면을 실패 증거로 보존하고 Game View에서 clean 8캡처를 추가했다. 만료 프레임에서 모두 사라졌고 컴포넌트4개 inactive를 읽었다. 정리는 반환값과 액터 baseline을 확인한 뒤 registry를 비우도록 검토·수정했다.
6. 소유 임시 액터4개를 제거하고 카메라/표시 상태를 복원했다. dirty map/content 0, 보호 파일12개와 관련2011개 해시 일치. 정상 quit 후 PID26632 종료와 UE/빌드/셰이더 프로세스0을 기록했다.

HLSL signed pow 문제는 `d*d`로 수정했다. [Microsoft pow 문서](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-pow)를 참고했다. 단순 수치 검사를 실제 화면·GPU 성능 검증으로 취급하지 않는다.

## Exit Gate

**후보 제작 패킷 완료, 스킬 연결 미실시.** 동일 에디터에서 저장 후 재확인했고 cold reload는 하지 않았다. 어두운 빈 장면의 형태·소멸만 검수했다. 실제 캐릭터 소켓·애니메이션 notify·전투 배경·밝은 지면·다중 적·성능·물리 입력 검증은 후속 패킷이다. 지면 마법진이나 투사체 이동 구현은 이 후보에 포함되지 않는다. [결과와 근거](../doc/2026-11-10_magic-particle-candidates.md)에 상태를 구분해 기록했다.
