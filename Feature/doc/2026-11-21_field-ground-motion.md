# 필드 달리기 착지의 오른발 회복

`OpenRunMap`의 Q 동굴에서 620cm/s로 달리다 착지하면, 오른발이 공중에서 앞으로 나오는 중간에 발목과 종아리가 급하게 꺾였다. Blender 원본의 16~17프레임에서 오른발 위치가 연속으로 32.85cm와 31.89cm 바뀌었다. 같은 Blender 프로젝트에서 오른발의 궤적을 여러 프레임에 걸쳐 보간하고, 허벅지와 종아리 길이를 유지하며 무릎 방향을 맞췄다.

게임에서 실제 쓰는 `A_CommonForwardRunLanding_L`의 오른쪽 허벅지·종아리·발목과 발 IK 목표 키만 Unreal Editor에서 반영했다. 원본은 이전 전방 착지 작업의 Manny 낙하, 정지 착지, Jog에서 가져온 프로젝트 파생 동작이다. 원본 89개 본 중 나머지 85개는 키 위치·회전·스케일이 일치했다. 60Hz 키 28개와 0.45초 재생 길이, 왼발 `L` 0.133333초 및 오른발 `R` 0.416667초 동기 마커를 보존했다. Blender FBX를 새 프로세스에서 다시 열었을 때 본 위치의 최대 차이는 0.00026cm였다. 프레임 26의 Jog 기증 키와 뼈대 길이도 기존 허용 오차 안이었다. [Unreal 반입 검사](evidence/field-ground-motion-20261121/unreal-import.json) · [Blender 재가져오기](evidence/field-ground-motion-20261121/fresh-blender-reimport.json)

## 실제 OpenRunMap 60Hz 검증

기본 HumanMale을 프로필로 선택해 W와 Space를 컨트롤러 입력 경로로 보냈다. 실제 월드 파티션의 Q 동굴 바닥을 사용했다. 접지 후 11~27프레임에서 표시 외형의 종아리 대비 오른 발목과 허벅지 대비 종아리의 최대 프레임 회전을 비교했다.

| 코스 | 접지 충돌면 | 오른 발목 | 오른 종아리 |
| --- | ---: | ---: | ---: |
| 동굴 앞 달리기 착지 | 0° | 38.40 → 11.94° | 35.10 → 14.28° |
| 내리막 이륙 후 동굴 바닥 착지 | 0° | 38.40 → 11.94° | 35.10 → 14.28° |
| 동굴 경사면 오르막 착지 | 14.04° | 40.13 → 11.73° | 36.75 → 7.46° |

점프 없이 동굴 입구를 달려 들어가는 코스도 포함했다. 세 점프의 이륙, 접지 프레임과 620cm/s 접지 속도, 모든 코스의 위치와 이동 속도, 착지 그래프 가중치, Jog 인계 값이 수치상 일치했다. 오르막에서 왼발 지지 위치의 최대 차이는 0.028cm였고 오른발 발볼 본의 최저 바닥 여유는 5.30→5.18cm였다. 발볼 본은 신발 밑창의 최저점과 다른 측정점이다. 평지의 기존 StartHub 검수는 전방 두 속도, 정지, 옆 방향을 포함해 9,640개 조건을 통과했으며 기존 격리 세이브 34개를 보존했다. 필드·평지 검사와 에셋 반입의 최종 Unreal 오류는 0건이었다. [필드 수치 비교](evidence/field-ground-motion-20261121/field-ground-comparison.json) · [평지 회귀](evidence/field-ground-motion-20261121/flat-regression.txt)

같은 최종 에셋으로 720p 실제 필드의 왼발 지지, 오른발 회복, Jog 인계를 확인했다. 소프트웨어 렌더는 캡처 시점만 그렸으며 입력과 애니메이션은 매 프레임 평가됐다. 접지와 이동 상태는 NullRHI 측정값과 일치했다. 첫 촬영 명령에 공백이 들어가 콘솔 검수가 시작되지 않아 해당 테스트 프로세스를 종료했다. 재실행에서는 장치 탐색 오류 한 건을 기록했고 Microsoft Basic Render Driver 선택 이후 새 Unreal 오류 없이 13장을 촬영했다. 기본 후방 카메라가 일부 프레임의 왼발 끝을 화면 아래로 잘라 지면 여유는 본 수치도 함께 사용했다. [왼발 지지](evidence/field-ground-motion-20261121/uphill-left-support-720p.png) · [오른발 회복](evidence/field-ground-motion-20261121/uphill-right-swing-720p.png) · [Jog 인계](evidence/field-ground-motion-20261121/uphill-into-jog-720p.png)

## 재현 범위

독립 Editor Development 빌드로 `OpenRunMap`을 `-game -UseFixedTimeStep -FPS=60`으로 실행하고, 프로젝트 `tmp` 아래에 새 절대 `-UserDir`을 지정한 뒤 `-ExecCmds=Rogue10m.TestFieldGroundMotion`을 준다. 기존 클립을 비교할 때는 `-RogueFieldGroundMotionBaseline`, 렌더 단계에서는 `-RogueFieldGroundMotionCapture`를 추가한다. 적용본에서는 오른 발목의 21° 상한과 실제 경사 상태, 전방 착지 및 Jog 인계를 검사한다. NullRHI 수치는 프로그램 입력이며 전체 필드 런과 물리 키보드 검사, 전체 외형의 경사 착지는 별도 범위다.

첫 계측은 테스트 캐릭터 이름이 최대 12자를 넘어서 시작 전에 종료됐다. 첫 두 임포트 명령은 UE Python의 비공개 트랙 및 마커 속성에 접근해 저장 전에 종료됐다. 이후 축약한 프로필명과 공개 포즈·마커 API로 해당 단계를 다시 검증했다. [설계](../architect/2026-11-21_field-ground-motion.md)
