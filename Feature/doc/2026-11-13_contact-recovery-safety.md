# G08 접촉·회복 타이밍 안전 통합

작업 사본: `tmp/SkillTiming20261113`, 미커밋 `Sprint#9-33-contact-recovery-safety`. 공유 원본의 모든 파일/Git·UE/MCP/UBT/Cook/게임은 읽기 또는 미실행을 유지했다. 기존 V1 패킷은 공유에 아직 미통합이다. 공유339개 Source/Config를 안정 복사하고 기존 G08 코드 패치를 사본에만 적용했다. 11일 이후 달라진 Controller.cpp/h·MainMenuWidget.cpp를 현재 baseline으로 보존하며 패치에 포함하지 않는다. `.agents/skills`는 현재 없었다.

## 이번 구현

- `AuthoredContactTimes`, `AuthoredRecoveryEndTime`을 `EditAnywhere/BlueprintReadOnly`로 노출했다. 접촉 시각은 montage1x 좌표다. 회복 값도 montage1x **절대 종료시각**이며,0은 기존 V1 contact-only 입력 소유권을 보존한다. 양수는 마지막 contact 이상/몽타주 길이 이하/배속 환산30초 이하/finite 조건을 비용 전에 검증한다. 접촉 배열과 회복 값은 승인 때 복사하므로 실행 도중 DA를 편집해도 이미 승인된 타이밍은 바뀌지 않는다.
- 양수 회복은 마지막 contact 뒤에도 execution map/monitor를 유지한다. 회복 시한까지 새 공격을 막으며, 종료·피격·외부 중단에 해제한다. 해당 execution이 설치한 action deadline과 현재 값이 같을 때만 이전 값을 복원해 다른 action 소유권을 지우지 않는다. 공유쿨다운과 기지급 비용/use·기확정 명중 credit은 보존한다.
- 명시 회복 시한은 남은 contact에도 적용한다. 시한+1µs 뒤까지 지연된 contact는 취소되며 추격 타격으로 몰아 실행하지 않는다. 시한 정각의 contact는 허용한다. 타이머가 프레임 단위로 관찰되므로 회복 종료를 마지막 접촉과 같게 설정하면 늦은 프레임의 접촉이 버려질 수 있다. 실제 프레임/모션을 보며 여유를 조정해야 한다. 회복0의 V1 catch-up 동작은 유지한다.
- authored timer는 execution ID 외에 arm generation·예상 타격번호를 캡처한다. 유효 ticket을 먼저 소비한 뒤에만 pulse를 호출하고, 재예약/취소/완료 뒤 같은 옛 delegate는 상태·타이머·피해를 바꾸지 않는다. 정상 UE TimerManager가 중복 콜백을 전달한다고 주장하지 않는다. 수동 replay·지연 delegate에 대한 명시 방어다.
- accepted instance PlayRate × montage intrinsic RateScale을 시간축과 회복에 반영한다. 둘 중 하나가 실행 중 바뀌면 남은 타격을 취소한다. ASC 공속만 바뀌어도 이미 승인된 시각은 보존한다. 자세 유지 요청도 effective duration을 사용한다. 수락 취소 때는 변경 가능한 DA의 현재 montage가 아닌 실제 승인된 재생 snapshot을 중단한다.
- generated montage는 auto blend-out=true/trigger0을 명시·검증한다. runtime authored mode도 trigger0 또는 auto blend-out=false만 지원한다. 기존 기본 .25초 선행 blend-out이 마지막 contact를 지우는 구성을 비용 전에 거절한다. 실제 자연 재생/종료는 아직 확인하지 않았다.

피격 시 취소·면역 보류·3인칭을 유지했다. 기본공격·빈 배열의 legacy timer·맨손/Ctrl·무기6슬롯·마법 입력·월드/책 배치·현재 프로필/성장/저장은 변경하지 않는다. position/section seek, actor/mesh custom time dilation의 추가 지원과 실제 body/무기 궤적 정합은 이번 작은 패킷 밖이다.

## 제작기

기존 `Rogue10m.AuthorG08Sequence`를 보강했다. plan 기본값을 유지하고 apply에는 source-local contact3개, 임시 rate, **recovery 절대시각의 명시 입력**이 모두 필요하다. `recovery=0`도 의도적으로 명시할 수 있다. 입력 원본 double·저장 float·reflection readback 모두 같은 production validator와 범위를 검사하고, 양수 값이 float0으로 바뀌는 입력은 거절한다.

새 목적지: `/Game/Rogue10m/Skills/MeleeSequence20261113/G08`, schema v2. 새 Sequence3·montage1·DA1만 만든다. 11일 후보경로/기존 package·dirty 원본은 덮지 않는다. source 비용/피해/쿨다운/VFX 보호와 partial-save 목록·원본 dirty/디스크 해시 검사를 유지한다. clean 메모리 객체와 디스크가 동일한지는 이 검사만으로 증명되지 않으므로 소유자의 fresh load·cold reload가 필요하다. 여러 package 저장은 원자적이지 않다. 실패 후 자동 삭제/재시도하지 않는다.

```text
Rogue10m.AuthorG08Sequence plan
Rogue10m.AuthorG08Sequence plan contacts=<각 source의 로컬초3개> rate=<임시속도> recovery=<montage1x절대종료초>
Rogue10m.AuthorG08Sequence apply contacts=<동일한 세 제작값> rate=<동일값> recovery=<동일값 또는 의도적0>
```

위 placeholder는 실제 숫자가 아니며 접촉을 임의로 추정하지 않았다. 새 후보는 제작중/시각·timing 미검증/production opt-in=false다. 현재 DA와 카탈로그를 자동 교체하지 않는다.

## 근거 구분

| 구분 | 상태 |
|---|---|
| 과거 검사 | 2026-11-11 production kernel15시나리오155검사 통과. UE12개는 작성·미실행이었다 |
| 이번 독립 검사 | 실제 확장 kernel을 MSVC C++17/W4/WX로 컴파일, **24시나리오230검사 통과**. 기존155 재실행 포함·신규75개 assertion |
| 중간 체크포인트 | 회복/arm 추가 당시23시나리오221검사 통과 기록 보존 |
| 수정 중 실패 | hard expiry 계약 추가 후 오래된 fixture가 종료시각10ms 뒤 contact를 기대해 실패. 기대를 정확한 deadline으로 고치고 별도 만료·경계 검사를 통과. 실패 로그 보존 |
| 이번 UE 회귀 | contact suite23개 + authoring parser1개 = **24개 작성·정적 리뷰, 컴파일/실행0** |
| 실제 animation/3P | 새 package 생성0, 실제 재생·ready/contact/recovery 캡처·무기 궤적·VFX/물리입력·풀런0 |

신규 UE 회귀는 같은 실행의 old arm replay/consume 후 재호출·취소 후 새ID, 회복 입력 gate·승인값 후편집·피격/Stop 해제·미발생 타격 hard expiry, asset RateScale2·instance/asset rate 변경 취소, 잘못된 lifetime/회복/rate 비용 전 거절, 직접 편집 property flags를 포함한다. 기존 montage 합성 fixture와 private scheduling 경계가 중심이며 실제 `Montage_Play`·natural blend-out·렌더링을 증명하지 않는다.

독립 Sol6.1/ultra 검토를 병행했다. 도구가 Fast 플래그를 제공하지 않아 Fast 적용은 확인하지 않았다. 코드/API·timer/map 수명 검토에 확정 차단은 없었지만 실제 UE 빌드/실행 승인은 아니다.

기존 12개 animation `.uasset`의 SHA256/길이/mtime는 현재와 같고 읽기 안정성을 확인했다. 현재 원본 DA·skeleton의 해시도 기록했으나 과거 해시가 없어 11일 대비 동일성을 주장하지 않는다. 계정 소유/라이선스·실제 런타임 사용과 구분한다. 근거: `Feature/doc/evidence/contact-recovery-safety-20261113/`와 `DevLog/20261113_G08Timing/AnimationRecord.md`.

## 통합 전제

공유 편집 사용권을 가진 월드맵/통합 담당자만 반영한다. full patch는 현재 shared baseline, delta는 정확한 V1 G08 패킷 baseline 전용으로 제공한다. user가 바꾼 3개 파일을 포함하지 않는다. 오래된 11일 DevLog/Sprint 패치는 사용하지 않고 이번 한국어 추가문만 별도 합친다. 실제 커밋/브랜치 전환/푸시는 하지 않았다.

담당자는 UHT/Editor build·재시작→새24개 및 기존 무기/피격/자원/특성/카탈로그 회귀→소유한 Editor에서 source 관찰/수치 입력 plan→새 후보 apply/save/cold reload→실제3P ready/세 접촉/회복·시한 근처 저FPS/hitch·피격/중단·늦은 callback·VFX를 검증해야 한다. 수치 통과와 시각 승인, 프로그램 입력과 물리입력/풀런, Editor 결과와 Windows cook/package를 분리한다. G05/G06/G10 고유 기능·최종20개 콘텐츠/성장/UI/저장 이관은 여전히 별도다.
