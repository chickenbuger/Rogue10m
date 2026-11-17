# 너클·대검 파티클 30후보 실제 제작 결과

- 공식 MCP에서 SkillCandidates/SkillParticlesA 아래 30개를 실제 생성·컴파일·저장했다. 컴파일/스택 오류0, 스택 경고0, 원본4개 해시 보존. 아직 게임 스킬에 연결하지 않았다.
- 원본 효과를 재사용한 크기·수명·local/world space 변형이며 연속 ribbon 완성품은 아니다. GPU 비용 또는 실제 전투 가시성 검증을 끝냈다는 의미도 아니다.
- 기존 활성 VFX2스킬의 전체 반영 필드 readback은 새 대검 motion의 2개 허용 필드 변경과 함께 통과했다.
- 첫 렌더 시도는 SimCache Python 반환형 검사에서0PNG로 중단했다. 맵 realtime override 호출에서도 ensure가 발생했다. 원본/후보/스킬44개 보호 파일 해시는 보존했고 소유 editor는 정상 종료했다.
- 프리뷰 도구를 수정 중이며 렌더/시각 승인 전까지 후보를 게임에 연결하지 않는다. 검토용 scale1/dilation1과 실제 게임 스킬 scale/dilation은 구분해 기록한다.
- 마법 파티클 담당에게 UE/빌드 프로세스0을 확인한 후 에디터 인계를 제안했다. 회수 전에는 다른 UE/빌드를 시작하지 않는다.

근거: Feature/doc/evidence/sword-knuckle-20261110/particle-candidates/checkpoint.json 및 SkillParticlesA/manifest.json.


### 프리뷰 도구 수정 완료, 실제 재실행 대기

- UE5.8 Python bool+단일 out 반환은 성공 시 NiagaraSimCache 객체, 실패 시 None임을 확인했다. tuple 가정을 제거하고 반환 type/repr와 실제 age 진단을 기록한다.
- editor_set_viewport_realtime(True)는 기존 override 제거 경로이므로 사용하지 않는다. 명시적인 원본 설정 변경 없이 Slate tick에서 viewport invalidate로 갱신한다.
- AST/30개 성공 객체 mock/잘못된 반환 차단 검증 통과. 보호44개 패키지 해시 동일. PreviewA 실패0PNG는 보존하며 PreviewB 실제 실행 전까지 시각 완료로 처리하지 않는다. 현재 마법 담당 인계 조율 중이며 UE/빌드를 시작하지 않는다.


## 회수 후 자연 AI·파티클 실제 검증 후속

- BossNaturalB 자연 AI/공격/정리 회귀는 112개 성공, 실패0, 기존 저장 보존이다. 실제 wave1.6초/zone1.9초 windup을 관측했다. 고정 검사 시점을 제거한 QA 수정의 재검증이며 AI/밸런스를 바꾼 결과가 아니다.
- 관련 PNG8장을 직접 확인했다. 파동의 분홍 효과는 보이나 장판은 첫 Niagara 컴파일 이전에 캡처를 요청해 작은 점과 Preparing Niagara Systems가 남았다. 장판 시각·첫 사용·패키지 검증 완료로 승인하지 않는다.
- 파티클 최소 ProbeA는 running/paused/restored 캡처 차이와 실제 Age 키·입자1을 실증했다. 이를 적용한 PreviewC는30개 모두 실제 데이터 age/입자1 검증 및 PNG30장 확보, 보호44개 해시 보존에 성공했다.
- C의 대표 사진에서 카메라와 target cylinder 가림 때문에 효과를 판독하기 어려워 시각 승인하지 않았다. 색/재질/자산은 건드리지 않고 반대 카메라·가림 제거의 대표6개 DiagnosticD로 검토를 좁혔다.


### 파티클 시각 판독 문제 후속 진단

PreviewC의30개 실제 age/입자1 검증과 캡처는 성공했다. 대표6장 검토에서 효과가 뚜렷하지 않았고, 가림 제거·반대 카메라의 DiagnosticD6장도 같은 한계가 있어 시각 승인은 보류했다. 후보/재질을 무작정 바꾸지 않고 성공한 마법 프리뷰와 같은 transient NiagaraActor 소유 방식 및 실제 visibility/render flags를 비교하는 최소 진단을 준비한다. WorldSettings는 AInfo의 hidden 기본값을 다시 false로 바꾸므로 owner hidden을 확정 원인으로 기록하지 않는다. 원본44개 해시는 보존했다.


### 단일 밝기 개선 프로토타입 실제 제작·비교

- 공식 MCP에서 새 MIC1개와 이를 쓰는 대검 기본1타 Cast NS1개만 생성·컴파일·저장했다. inherited CueTint의 linear RGB를8배로 바꾸고 alpha1을 보존했다. 원본56개 해시는 동일하다.
- 실제 ReadabilityPrototypeF1장과 baselineE 사진을 비교해 금색 세로 시전 flash가 더 뚜렷함을 확인했다. 프리뷰는58개 파일 해시 및 실제 age/입자1을 보존·검증했다.
- 이는1개 고정 포즈 후보 비교다. 기존30개로 HDR를 확대한 것도, 게임 스킬에 연결한 것도 아니며 체감 밝기8배나 실제 전투 가시성 통과를 뜻하지 않는다.
- Shader schema의 flat property map을 잘못 가정한 ReadabilityGain8A는 생성0건으로 중단했고 근거를 보존했다. 실제 schema 회귀검사 후 새B라벨에서 제작했다.


## 통합 회귀 및 파티클 전체 실제 화면 검토 마무리

- 무기11, 타깃 생명주기5, 카탈로그 소비/성장6, 드롭 전체8 및 Write26/Read9, 자연AI112개 검사 결과를 통합했다. 기존432개 저장 해시를 현재 디스크와 재대조해 변경·소실0을 확인했다.
- SkillParticlesReviewG에서 기존30후보 전부 실제 age/입자1·Color/Size/NormalizedAge와 PNG완료를 확인하고30장을 직접 보았다. 보호56개 해시 동일, 소유 임시 NiagaraActor30개 정리, 오류/ensure0이다.
- 기본 효과 형태는 보이지만 일부 시전은 몸/손에 겹치고 전반 대비가 낮다. 최종 게임 가시성/스킬 연결 승인은 아니다. 별도 MIC/NS gain8 프로토타입1개는 더 명확한 금색 시전 flash로 확인했으며 기존30개에는 확대하지 않았다.
- 장판 첫 사용/cooked 패키지 가시성, 실제 스킬 scale/dilation/부착/타격 연계, 연속 ribbon, 지팡이 모델 지정, 신규20 기술 콘텐츠는 후속 범위다. 1인칭은 계속 보류했다.
- 소유 PID55388은22:28:07 UTC 정상 종료 로그를 남겼고22:30:28 UTC UE/빌드/셰이더0을 확인했다. 엔진 종료 뒤 launcher의 PowerShell false/$false 오타가 있었으나 실제 작업은 반복하지 않고 캡처·종료로그·프로세스 상태를 독립 확인해 근거를 보완했다.
- 최종 근거: Feature/doc/evidence/sword-knuckle-20261110/particle-candidates/final-integration-summary.json 및 SkillParticlesReviewG/visual-review.json.
[Notion 요약 후보] 대검 편집 모션과 통합 기능 회귀를 검증했고, 너클·대검 파티클30후보 전부 실제 렌더를 검토했다. 최종 게임 연결 전 대비/위치/실제 전투 조건 보완이 남아 있으며 원본·저장·마법 자산은 보존했다.
