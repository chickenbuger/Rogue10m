# 상체 시선 연동 검증 중 진행 세이브 원본 복원 증거

작성일: 2026-10-11

초기 QA 실행에서 진행 세이브 접두사에 하이픈이 포함되어 테스트 슬롯 검사가 실패했다. `Rogue10mProgressionSubsystem.cpp`의 접두사 허용 문자는 영문·숫자·밑줄이며, 유효하지 않은 접두사는 생산 슬롯 `Rogue10mProgression`으로 돌아간다. 그 결과 현재 선택된 프로필의 진행 파일에 QA 공격 사용 횟수와 해금 항목이 기록되었다. 캐릭터 프로필 파일과 기존 스킬 키 파일의 원래 해시는 유지되었다.

이 분석은 생산 세이브에 쓰지 않았다. 변경된 파일과 관련 소스·증거를 읽고, 메모리에서 후보를 재구성하여 **작업 시작 전 기록된 전체 파일 SHA256와 정확히 일치한 후보만** 임시 폴더에 저장했다. 실제 슬롯 복원은 루트 작업자가 Unreal의 `SaveDataToSlot`을 통해 별도로 수행한다.

## 정확히 재구성된 파일

| 항목 | 값 |
| --- | --- |
| 대상 생산 슬롯 | `Rogue10mProgression_F896E7AD4CDCF783285888A597914739` |
| 원본 해시 기록 | `production-saves-before.json` |
| 원본 SHA256 | `B39F2FCD80C499C50928C39082714FF3920B82A419F775C8FC02E192718DE35D` |
| QA 실행 후 SHA256 | `19D203F982BAE1F54FC064A98E3B77AB880D0C4C0A2F8D29B4A0F9568FEB6236` |
| QA 실행 후 길이 | 140,277 bytes |
| 정확 복원 후보 길이 | 138,536 bytes |
| 정확 복원 후보 | `D:/Project/Rogue10m/tmp/view-aligned-upper-body/production-recovered-exact.sav` |
| 복원 후보 SHA256 | `B39F2FCD80C499C50928C39082714FF3920B82A419F775C8FC02E192718DE35D` |

Python `hashlib.sha256`과 PowerShell `Get-FileHash -Algorithm SHA256`로 독립 확인했다. 파일 전체 해시가 원본 기록과 같으므로, 특정 필드에 대한 추정만으로 복원 후보를 채택한 것이 아니다.

## 원래 진행 상태와 QA가 추가한 항목

원래 무기는 대검이며 사용 횟수는 `DA_Attack_GreatSword_Primary01=12`, `DA_Attack_GreatSword_Special=1`, `DA_Attack_GreatSword_Charged=2`이다. 해금 목록은 대검의 Charged, Primary01, Primary02, Primary03, Special 다섯 개다. 기본 공격·특수 공격·차징 기본 공격·차징 특수 공격의 대검 바인딩 네 개도 원본과 같았다.

QA가 추가한 사용 횟수는 왼손 잽 4회, 오른손 잽 1회, 오른손 스트레이트 1회, 오른손 훅 2회, 너클 기본 공격 1회, 너클 특수 공격 1회였다. 해금에는 기본 권사 6개, 기존 Unarmed 프로필 6개, 너클 2개가 추가되어 총 19개가 되었다. 정확 복원 후보에서는 이 여섯 개 사용 기록과 추가 해금 14개가 제거된다.

프로필 GUID, 인벤토리 payload, 대검 바인딩 payload는 QA 실행 후 파일의 바이트를 그대로 보존하여 원본 전체 해시를 맞췄다. 인벤토리 payload는 134,231 bytes이며 SHA256는 `6E2C21F7F766C3B4735D6865A06DB71C0C6CBD3C20D77BF85AA4377D0B20A04D`이다. QA 후 파일과 복원 후보의 값·필드 해시는 `production-save-recovery-data.json`에 기록했다.

## 재구성 방법

1. `Saved`, `tmp`, `Feature`의 100~150 KB 파일 1,395개를 해시 확인했지만 원본 복사본을 찾지 못했다.
2. UE 5.8의 `CoreUObject/Private/UObject/PropertyTag.cpp`에 정의된 complete type name 직렬화 형식을 사용해 GVAS의 태그, payload, 길이를 읽었다. 생산 세이브를 Unreal 객체로 다시 저장하거나 수정하지 않았다.
3. 새 공격 기록을 제거한 뒤 해금 부분집합 16,384개, 기존 대검 사용 횟수 맵 순서 6개, 대검 바인딩 맵 순서 24개를 검사했다. 큰 인벤토리 영역은 `sha256.copy()`로 재사용했다.
4. GVAS의 CustomVersion 등록 순서가 실행마다 달라지는 것을 확인했다. 현재 헤더를 그대로 둔 후보는 원래 데이터라도 전체 파일 해시가 달라질 수 있으므로, 보관된 SaveGame 및 증거의 실제 헤더 24종도 검사했다.
5. 실제 헤더에 관측된 모듈 등록 블록과 선후관계를 통해 추가 헤더를 만들었다. 고정 영역 6종 × 모듈 등록 순서 5,040개로 생성한 30,240개 헤더를 대상으로 해금 그룹 8조합, 대검 사용 기록 순서 6개, 대검 바인딩 순서 24개를 검사했다.
6. `Saved/SaveGames/Rogue10m_Town_QA_Run_integratedrun01.sav`에서 확보한 고정 헤더 영역과 모듈 순서 `(80, 77, 78, 79, 81, 82, 83, 84, 85, 86, 87, 76)`의 조합에서 원본 해시가 일치했다. 숫자는 분석 시작 당시 생산 파일의 CustomVersion 항목 인덱스다. 성장 맵과 바인딩은 QA 후 파일과 같은 순서이며, 해금 목록은 대검 다섯 개만 유지하는 조합이다.

재현 도구는 `D:/Project/Rogue10m/tmp/view-aligned-upper-body/recover_save_exact.py`다. `--generated-headers` 실행은 메모리에서 후보를 검사하고, 지정된 원본 SHA256가 일치할 때만 `production-recovered-exact.sav`를 만든다. 생산 슬롯에 대한 쓰기 기능은 없다.

## 실제 복원 확인 범위

이 보고서 작성 시점에는 임시 후보의 전체 파일 해시가 원본과 일치한 것까지 확인했다. 생산 슬롯의 실제 복원과 복원 후 재검증 결과는 루트 작업자가 별도 증거로 기록한다. `SaveGameToSlot(LoadGameFromMemory(...))`로 재직렬화하면 CustomVersion 헤더 순서가 바뀔 수 있으므로, 원본 바이트 복원에는 `SaveDataToSlot`을 사용한다.

### 루트 작업자의 실제 복원 완료

임시 Editor 전용 C++ 도구에서 현재 파일과 복원 후보의 지정 해시·프로필 GUID·저장 버전을 확인한 뒤 `UGameplayStatics::SaveDataToSlot`로 원본 바이트를 저장했다. 다시 `LoadDataFromSlot`로 읽은 바이트 배열의 완전 일치와 외부 SHA256를 확인했다. 실제 생산 파일의 SHA256는 작업 전 값 `B39F2FCD80C499C50928C39082714FF3920B82A419F775C8FC02E192718DE35D`와 같다.

`production-save-restore.log`의 `RESULT=VIEW_AIM_EXACT_SAVE_RESTORE_PASSED bytes=138536`와 `production-save-restore-wrapper.log`를 증거로 남겼다. 일회 복구 도구는 사용 후 Source에서 제거했으며 재현용 사본만 `tmp/view-aim`에 보존했다. 이후 QA는 허용되는 문자로 정규화한 독립 skill/progression 슬롯을 사용한다. 일반 프로필·진행도·기존 스킬 키의 세 파일은 최종 보존 검사에서 다시 비교한다.
