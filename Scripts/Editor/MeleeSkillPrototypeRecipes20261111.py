"""Canonical data recipes for isolated melee-skill production.

These are draft production identities, not released save IDs or completed skills.
The six/common-two/chord-two/charge candidate split is a design proposal. The
runtime equipment limit remains four common, one chord, and one right-charge.
This module is pure Python and importing it never starts Unreal or changes files.
"""

from dataclasses import dataclass
import math
import re


DESTINATION_ROOT = "/Game/Rogue10m/Skills/MeleeCatalog20261111"
AUTHOR_VERSION = "20261111.prototype.1"
PROFILE_SOURCES = {
    "Knuckle": "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle",
    "GreatSword": "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_GreatSword",
}
KNUCKLE = "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_"
STONE = "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_"
SWORD = "/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_"
HYBRID = "/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_"


@dataclass(frozen=True)
class AdvancePrototype:
    distance_cm: float
    duration_seconds: float


@dataclass(frozen=True)
class SkillPrototypeRecipe:
    document_id: str
    weapon: str
    name: str
    source_package: str
    slot_policy: str
    pending: tuple[str, ...]
    advance: AdvancePrototype | None = None

    @property
    def technique_id(self) -> str:
        return f"Prototype.{self.weapon}.{self.document_id}"

    @property
    def destination_package(self) -> str:
        return f"{DESTINATION_ROOT}/{self.weapon}/DA_Prototype_{self.document_id}"

    @property
    def description(self) -> str:
        return (f"{self.name} 제작 프로토타입. 기존 동작·비용·피해·타격시점을 재사용한다. "
                "Production 미연결, 완료 아님. 남은 작업: " + "; ".join(self.pending))


def recipes() -> tuple[SkillPrototypeRecipe, ...]:
    common = "COMMON"
    chord = "MOUSE_CHORD_ONLY"
    charge = "RIGHT_CHARGE_ONLY"
    return (
        SkillPrototypeRecipe("K01", "Knuckle", "연환권", KNUCKLE + "Primary01", common,
                             ("기본공격과 독립된 DA/연계 검수", "별도 연타 모션·타수·VFX 미완료")),
        SkillPrototypeRecipe("K02", "Knuckle", "붕권", KNUCKLE + "Primary02", common,
                             ("기존 BodyStraight 재사용", "새 전진·경직·VFX 미완료")),
        SkillPrototypeRecipe("K03", "Knuckle", "금강쇄", STONE + "Straight", common,
                             ("기존 마무리 단발 재사용", "어퍼컷·띄움·보스 대안 미구현")),
        SkillPrototypeRecipe("K04", "Knuckle", "유운수", STONE + "Jab", common,
                             ("기존 훅 재사용", "파리 성공/실패·반격 판정 미구현")),
        SkillPrototypeRecipe("K05", "Knuckle", "철산고", KNUCKLE + "Charged", common,
                             ("기존 ShortBlow의 임시 데이터 재사용", "어깨충돌·실이동 모션/판정 미구현")),
        SkillPrototypeRecipe("K06", "Knuckle", "파진각", KNUCKLE + "Special", common,
                             ("기존 안전 공격 재사용", "킥·지지발·부채꼴 판정 미검증")),
        SkillPrototypeRecipe("K07", "Knuckle", "백열난무", KNUCKLE + "Primary03", chord,
                             ("기존 HookFlurry 타수/타이밍 보존", "동시 전용 재입력·3P/VFX 검수 필요")),
        SkillPrototypeRecipe("K08", "Knuckle", "무극패왕권", STONE + "ChargedShockwave", chord,
                             ("기존 차지권의 release 공격 재사용", "동시 전용 전환·부채 판정 미검증")),
        SkillPrototypeRecipe("K09", "Knuckle", "태산압정", STONE + "JumpSlam", charge,
                             ("legacy 공중 입력 의미 보존", "도약·실제 착지슬램·지상 차지 전환 미구현")),
        SkillPrototypeRecipe("K10", "Knuckle", "천붕권", KNUCKLE + "Charged", charge,
                             ("기존 ShortBlow 기반 독립 우차징 데이터", "전용 차지포즈·집중 판정·VFX 미완료")),
        SkillPrototypeRecipe("G01", "GreatSword", "분쇄참", HYBRID + "CrushingSlash", common,
                             ("기존 Hybrid 강타 보존", "독립 모션·접촉·VFX 검수 필요")),
        SkillPrototypeRecipe("G02", "GreatSword", "회전참", HYBRID + "Whirlwind", common,
                             ("기존 Hybrid 광역 공격 보존", "실제 회전 폭·벽/동일대상 상한 검수 필요")),
        SkillPrototypeRecipe("G03", "GreatSword", "파쇄참", HYBRID + "GuardBreak", common,
                             ("기존 Hybrid 공격 보존", "고유 방어붕괴·자세 누적 미구현")),
        SkillPrototypeRecipe("G04", "GreatSword", "돌진참", SWORD + "Primary02", common,
                             ("기존 slash fallback이며 slide 모션 아님", "전진 후 종점단타 코드의 UE/3P 검증 필요",
                              "거리150cm/시간0.25초는 임시 실험값"), AdvancePrototype(150.0, 0.25)),
        SkillPrototypeRecipe("G05", "GreatSword", "반격참", HYBRID + "GuardBreak", common,
                             ("기존 공격 fallback만 존재", "방어창·성공/실패·반격 토큰 미구현")),
        SkillPrototypeRecipe("G06", "GreatSword", "올려베기", SWORD + "Primary03", common,
                             ("기존 slash fallback이며 상승 모션 아님", "띄움·보스 대안·양손 그립 미구현/미검증")),
        SkillPrototypeRecipe("G07", "GreatSword", "대지파쇄", HYBRID + "EarthSplitter", chord,
                             ("기존 heavy-cut fallback 보존", "도약슬램·지면파·지형 대응 미구현")),
        SkillPrototypeRecipe("G08", "GreatSword", "섬멸연참", SWORD + "Primary01", chord,
                             ("기존 단발 slash fallback만 존재", "독립3연격·비균등 접촉·복합 몽타주 미구현")),
        SkillPrototypeRecipe("G09", "GreatSword", "패왕참", HYBRID + "Ultimate", charge,
                             ("기존 Hybrid 차징 타격 보존", "전용 완충포즈·고유 강화·VFX 검수 필요")),
        SkillPrototypeRecipe("G10", "GreatSword", "단공참", SWORD + "Charged", charge,
                             ("기존 heavy-cut fallback이며 slide 모션 아님", "우차징 후 전진 종점단타만 구현 시작",
                              "이동경로 관통 피해 미구현", "거리250cm/시간0.35초는 임시 실험값"),
                             AdvancePrototype(250.0, 0.35)),
    )


def validate_recipes(rows: tuple[SkillPrototypeRecipe, ...]) -> None:
    if len(rows) != 20:
        raise ValueError("Expected exactly twenty Knuckle/GreatSword draft techniques")
    for weapon, prefix in (("Knuckle", "K"), ("GreatSword", "G")):
        selected = [row for row in rows if row.weapon == weapon]
        if {row.document_id for row in selected} != {f"{prefix}{i:02}" for i in range(1, 11)} or len(selected) != 10:
            raise ValueError(f"Missing or duplicate canonical IDs for {weapon}")
    for attribute in ("document_id", "technique_id", "destination_package"):
        if len({getattr(row, attribute) for row in rows}) != 20:
            raise ValueError(f"Duplicate {attribute}")
    for row in rows:
        if not re.fullmatch(r"/Game/[A-Za-z0-9_/]+", row.source_package):
            raise ValueError(f"Invalid source package: {row.source_package}")
        if not row.destination_package.startswith(DESTINATION_ROOT + "/") or row.destination_package == row.source_package:
            raise ValueError("Destination must be a separate project-owned prototype")
        if row.slot_policy not in {"COMMON", "MOUSE_CHORD_ONLY", "RIGHT_CHARGE_ONLY"}:
            raise ValueError("Explicit melee slot policy required")
        if not row.name or not row.pending:
            raise ValueError("A prototype must identify its unfinished work")
        if row.advance:
            if row.document_id not in {"G04", "G10"}:
                raise ValueError("Advance is scoped to the two new sword prototypes")
            distance, duration = row.advance.distance_cm, row.advance.duration_seconds
            if not math.isfinite(distance) or not 0 < distance <= 600 or not math.isfinite(duration) or not 0.05 <= duration <= 2:
                raise ValueError("Advance prototype is outside the runtime acceptance bounds")


validate_recipes(recipes())
