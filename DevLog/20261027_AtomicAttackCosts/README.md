# Atomic attack resource payment

검증 종류: 자동 테스트 결과 화면 사진. 실제 게임 플레이 영상이나 Unreal 에디터 플레이 화면이 아니다.

이 사진은 실제 Unreal Automation export의 테스트 이름/성공/오류/경고를 검사한 출력 파일을 같은 데스크톱 Edge에서 연 뒤 브라우저 캡처 API로 촬영한 것이다. 기기/계정 메타데이터는 표시 전에 제외했다. 연출 이미지, 게임 화면 합성, 이미지 생성 도구를 사용하지 않았다.

- 작업 사본: `C:\Users\PC\Documents\Codex\2026-10-27\task\Rogue10m-r11`
- 브랜치: `Sprint#9-5-weapon-switch-runtime`
- 구현 커밋: `1275bd54fa6b77a44da15ed255cb2c5b25425de5`
- 검증 시각: `automation-results.txt`의 Test report UTC, 촬영 시각은 verification.json 참조
- 결과: 4개 통과, 오류0, 경고0
- 범위: 중복 자원 비용 합산, 실패 원자성, 선택 마나 비용 및 수락 재진입 자동화 검증.
- 미실행: 렌더링 플레이, 실제 키 입력, 명중/교체 모션, 전체 사용자 프로필 로드
- 재현: 위 사본에서 `Scripts/VerifyAttackResourceCosts.ps1 -Suite AttackResources -RunLabel mediaRecheck -EvidencePacket media-recheck-20261027` 실행. 같은 증거 폴더가 이미 있으면 새 label/packet을 지정한다.
- 파일: automation-results.jpg, automation-results.txt, verification.json
- 열기/디코딩: JPEG 2552x1308, Pillow verify/load 및 복사 후 재검사 통과. 파일 해시는 verification.json 참조.

이 폴더는 사용자 요청에 따른 로컬 DevLog 증거다. 자동으로 Git에 추가하거나 외부 공개하지 않는다. 기존 기록 파일은 덮어쓰지 않았다.
