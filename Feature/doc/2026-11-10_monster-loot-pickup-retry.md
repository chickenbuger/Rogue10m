# 드롭 자동 재획득 통합 검증 완료

- 4파일 원본 패치를 백업 후 공유 프로젝트에 통합하고 Editor DLL 빌드를 통과했다. 현재공간 부족 실패 후 제자리에서 가방 공간 확보/확장 시 다음 tick에 재시도하며 거래 재진입을 피한다.
- LootAllA: 기존5개+신규3개 전체8검사 성공, 경고/오류0, 기존 저장 변경0, 예상 밖 신규 저장0.
- LootResumeC: 실제 처치·충돌·획득 Write26개와 재시작 Read9개 성공. 종료 코드0, Error/Fatal/ensure/Python 오류0, 기존 저장 변경·소실0, 예상 밖 신규 저장0.
- 새 빈 QA profile을 GI 초기화 전에 만들고 begun world의 정상 EndPlay를 수행하도록 fixture를 고쳤다. runtime 게임 검사에는 필요 없는 Editor Python을 제외하고 실제 Error:/Fatal: verbosity를 정확히 판정하며 전체 저장 해시를 대조하도록 runner를 강화했다. 기존 A/B 실패 근거는 보존했다.
- 물리 입력/전체 런 플레이나 정상 드롭 확률·경제 밸런스 검증은 아니다. 마지막 실패 플레이어1명 관측, 많은 드롭의 알림 비용, 별도 death/inventory replacement 잠복 테스트 미포함 한계는 남는다.
- 마법 담당이 만든8개 자산은 원래 final verification SHA256과 동일함을 추가 확인했다.

근거: Feature/doc/evidence/monster-loot-pickup-retry-20261110/integrated-verification.json.
