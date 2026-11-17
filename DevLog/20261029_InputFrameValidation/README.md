# 입력·첫 공격 프레임 검증

2026-10-29T15:23:53.906851+00:00, `D:\Project\Rogue10m`, `Sprint#9-12-input-validation`, 기준11a15011. 최종 커밋·소스 해시는 verification.json에 기록한다.

실제 Unreal D3D11 viewport 사진5장이다. native_settings_on은 설정 ON, native_abandon_modal은 포기 확인창, native_original_sprint/native_weighted_sprint는 격리 충돌 바닥의 OFF/ON 실제 프로그램 입력 이동, offscreen_settings_on은 OS 창 포커스가 분리된 렌더링의 설정 ON이다. 원본을 변경하지 않고 동일 SHA256으로 복사했다. 동영상·물리 입력·일반 필드 완주 증거가 아니다.

native569/Offscreen565항목 PASS, 첫 공격4프로세스/8공격142항목 PASS. 첫 사용의75~77ms 수락/63~68ms 첫 피해 프레임을 두 모드에서 재현했다. 원인은 미확정이며 촬영 없이 측정했다. 재사용50ms 초과0. 세이브226개 포함663파일 원본·복사본 해시 불변.

재현1: `Scripts/VerifyFirstPersonSettings.ps1 -Label <새라벨> -Rendered -InputTrace -FocusProbe` (native). 격리 렌더링은 FocusProbe 대신 Offscreen.
재현2: `Scripts/MeasureFirstAttackFrames.ps1 -Label <새라벨> -Profile Original -Rendered -Offscreen -MaxFPS 60`. ON은 Profile Weighted와 별도 새라벨. 기존 슬롯/라벨은 재사용하지 않는다.

상세: Feature/doc/2026-10-29_input-frame-validation.md. 미디어·로그·실제 세이브를 Git에 넣지 않는다. push/병합 없음.

최종 로컬 커밋: `4676fdadd95d16dbaca832c76e6c6a561837809e`. 검증 소스4파일 해시 일치, 추적파일 변경 없음, 원격 변경 없음.
