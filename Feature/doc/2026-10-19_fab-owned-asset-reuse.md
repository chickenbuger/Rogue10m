# Fab 보유 에셋 우선 재사용 적용 결과

## 반영 내용

사용자 요청에 따라 앞으로 콘텐츠 개발에서 프로젝트 기존 자원과 Fab 보유 에셋을 우선 평가하도록 `AGENTS.md`에 규칙을 저장했다. `Docs/FabAssetReuseGuide.md`에 실제 보유 후보와 프로젝트 자원, 기능별 선택·반입·검증 순서를 정리했다.

작업 브랜치는 `Sprint#5-36-fab-owned-asset-reuse`이다. 특정 새 게임 기능을 임의로 개발하거나 자산을 일괄 반입한 작업은 아니다.

## 완료 패킷

| Packet | 결과 | 검증 |
| --- | --- | --- |
| F1 Architect | 런처 보유 카드 30개와 로컬 주요 팩 8개 조사 | 실제 Fab 내 라이브러리 및 Combat/Monster 내 라이브러리 검색, 독립 로컬 읽기 조사 |
| F2 Implementer | 지침과 가이드 저장 | 현재 화풍·C++/Data Asset 구조·카메라/저장 계약, source/status 구분 |
| F3 Builder / Reviewer / Doc Writer | 범위·근거 검토와 한국어 로그/개발 이력 | 문서·JSON·경로/기존 변경 보존 검사와 독립 검토 |

## 확인한 우선 후보

- 격투: Close Combat Animset. 로컬 준비 프리뷰와 현재 연결을 재확인하고 머리 카메라·접촉 타이밍에 맞는 클립부터 검토한다.
- 직업/마법: Combat Magic Animations. 기존 제작 스크립트와 클립 자원을 먼저 활용한다.
- 몬스터: Monster for Survival Game PBR Polyart. 기존 종별 프로필·동작 매핑을 이용한다.
- 환경: Stylized Village와 모듈형 Stylized Characters. 프로젝트 폴더와 기존 구현을 우선 보존하며 정확한 제품 대응과 필요한 항목을 확인한다.
- 추가 시각/청각 후보: Slash Trail FX Elemental, Monster Roars & Growls - Audio Pack, White castle, tree ents/Wood Monster. 보유 카드 확인과 실제 반입/적용 상태를 구분한다.
- 프레임워크: 보유 전투/인벤토리/난이도 팩은 기존 시스템에 맞는 사용 범위를 평가한다. 현재 코드·성장·영구 저장을 이유 없이 교체하지 않는다.

이번 목록은 최신순 상단과 두 번의 스크롤 및 Combat/Monster 검색에서 확인한 부분 목록이다. 전체 계정 보유 수가 30개라는 뜻이 아니다. 말줄임 제목과 디스크 표시를 그대로 기록했으며, 파일 내부·클립·성능·라이선스·버전은 조사하지 않았다.

## 변경 파일

- `AGENTS.md`: Fab 보유/기존 프로젝트 자원 우선 평가와 선별 재사용 규칙.
- `Docs/FabAssetReuseGuide.md`: 실제 후보·기존 자원·후속 적용 기준.
- 이 계획/결과 및 `Feature/doc/evidence/fab-owned-assets-20261019/`: 보유 카드 관찰과 로컬 조사·검토·검증 자료.
- `Docs/SprintChangeLog.md`, `DevLog/20261019.txt`: 목표·변경·검증·한계와 Notion 요약 후보.

## 범위와 다음 적용

구매·다운로드·팩 설치·플러그인 실행·에셋 import·게임 소스/설정 변경·빌드·PIE를 수행하지 않았다. 이후 구체적인 콘텐츠 작업에서 이 가이드의 해당 후보를 우선 확인하고 필요한 자산부터 선별 적용한다. 기존 미커밋 작업은 유지했고 커밋·푸시는 하지 않았다.

런처 접근 승인 타임아웃은 문서화된 창 재선택 1회로 복구했다. 후반 Portal 입력 시 사용자 입력 보호 신호가 있어 상태만 다시 읽고 추가 UI 조작을 멈췄다. 보유 확인이 끝나지 않은 검색은 목록에 넣지 않았다. 사용자 인증이나 계정 캐시·숨은 API는 사용하지 않았다.

## 후속 반영 — 기본 애니메이션·VFX 편집 우선

사용자 선호에 따라 기본 동작·효과는 보유 원본을 선택하고 프로젝트용 파생본에서 수정·편집하는 방식을 기본 규칙으로 추가했다. 애니메이션의 구간·재생률·블렌딩·몽타주/Notify와 VFX의 색·크기·수명·방출·타이밍·부착 등 구체적 편집 범위를 가이드에 기록했다. 적합한 에셋으로 요구를 충족하기 어렵거나 새 제작이 명시된 경우에는 이유를 기록한다. 기존 원본과 게임 계약을 보존하며 이번 후속에서도 실제 에셋·소스·설정은 변경하지 않았다.

## 관련 자료

- [계획](../architect/2026-10-19_fab-owned-asset-reuse.md)
- [상시 활용 가이드](../../Docs/FabAssetReuseGuide.md)
- [실제 카드 30개 관찰 기록](evidence/fab-owned-assets-20261019/launcher-observations.json)
- [로컬 자원 조사](evidence/fab-owned-assets-20261019/project-assets.md)
- [한국어 일일 로그](../../DevLog/20261019.txt)
