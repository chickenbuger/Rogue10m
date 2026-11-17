# Shared attack cooldown across weapon changes

검증 종류: 자동 테스트 결과 화면 사진. 실제 게임 플레이 영상이나 Unreal 에디터 플레이 화면이 아니다.

이 사진은 실제 Unreal Automation export의 테스트 이름/성공/오류/경고를 검사한 출력 파일을 같은 데스크톱 Edge에서 연 뒤 브라우저 캡처 API로 촬영한 것이다. 기기/계정 메타데이터는 표시 전에 제외했다. 연출 이미지, 게임 화면 합성, 이미지 생성 도구를 사용하지 않았다.

- 작업 사본: `C:\Users\PC\Documents\Codex\2026-10-27\task\Rogue10m-r11`
- 브랜치: `Sprint#9-5-weapon-switch-runtime`
- 구현 커밋: `1275bd54fa6b77a44da15ed255cb2c5b25425de5`
- 검증 시각: `automation-results.txt`의 Test report UTC, 촬영 시각은 verification.json 참조
- 결과: 5개 통과, 오류0, 경고0
- 범위: 공유 공격 쿨타임의 긴 만료 시각 보존, max(남은 시간,0.5초), 복원/재알림 및 예약 콤보 자동화 검증. 모든 개별 스킬 타이머 완료가 아님.
- 미실행: 렌더링 플레이, 실제 키 입력, 명중/교체 모션, 전체 사용자 프로필 로드
- 재현: 위 사본에서 `Scripts/VerifyAttackResourceCosts.ps1 -Suite WeaponCooldown -RunLabel mediaRecheck -EvidencePacket media-recheck-20261027` 실행. 같은 증거 폴더가 이미 있으면 새 label/packet을 지정한다.
- 파일: automation-results.jpg, automation-results.txt, verification.json
- 열기/디코딩: JPEG 2552x1308, Pillow verify/load 및 복사 후 재검사 통과. 파일 해시는 verification.json 참조.

이 폴더는 사용자 요청에 따른 로컬 DevLog 증거다. 자동으로 Git에 추가하거나 외부 공개하지 않는다. 기존 기록 파일은 덮어쓰지 않았다.


## 대표 사진 보관 상태

사용자 요청에 따라 중복·이전·보조 사진을 정리했다. 현재 `20261027_SharedAttackCooldown` 묶음에는 대표 0장을 보관한다. 이 문서의 촬영 개수·파일명·시각 판정은 당시 실행 이력이며, 현재 사진 목록은 [대표 사진 목록](../README.md)을 따른다. 수치 결과·텍스트 실패 기록은 그대로 유지했다.
