"""Stone edging and material accents on the paths through StartHub."""

VILLAGE = "/Game/Stylized_Village/Assets"
FLOOR = VILLAGE + "/Architecture/StoneFloor/"
DECK_PAVERS = FLOOR + "SM_StoneFloor_01_Full"
PAVERS = FLOOR + "SM_StoneFloor_01_FullHalf"
SINGLE_PAVER = FLOOR + "SM_StoneFloor_01_SingleTile_01"
PILLAR = VILLAGE + "/Architecture/Flagstone/Structure/SM_FlagStone_Pillar_Full"
ARCH = VILLAGE + "/Architecture/Flagstone/Trims/SM_FlagStone_Trim_Arch_Flatter"
FLOWERS = VILLAGE + "/Foliage/Flowers/BabyBlueEyesAndPoppies/SM_BabyBlueEyes_Small_01"
WELL_EAVE = VILLAGE + "/Architecture/Wood/Roof/SM_Wood_CeilingPlanks_Full_LP"
CANVAS = VILLAGE + "/Props/Tarps/SM_Tarp_BigSquare"
IVORY = "/Game/Rogue10m/Characters/Arin/Blender/Materials/M_Arin_cloth_ivory"


def author(detail):
    for side, sign in (("South", -1), ("North", 1)):
        detail.mesh("Plaza", side + "WellRoofSoffit", WELL_EAVE, 0, sign * 90,
                    393, 526, 212, 8, roll=sign * 32)
    for index, (x, y) in enumerate(((304, -1515), (362, -1515), (410, -1510)), 1):
        detail.mesh("Arrival", f"GuideApproach{index}", SINGLE_PAVER, x, y, 0,
                    66, 96, 6, yaw=(index - 2) * 9)

    # The cloth materials used for readable color blocks are one-sided.
    for group, x, y, z, sx, sy, sz, material in (
        ("Equipment", -1270, -815, 294, 325, 340, 27, IVORY),
        ("Expedition", -1410, 880, 270, 325, 358, 33, "ochre"),
        ("Archive", 1401, 898, 289, 338, 325, 25, "teal"),
    ):
        detail.mesh(group, "CanopyUnderside", CANVAS, x, y, z, sx, sy, sz,
                    material=material, roll=180)

    for group, x, y in (
        ("Equipment", -925, -820), ("Forge", 1090, -825),
        ("Expedition", -1020, 850), ("Archive", 1070, 900),
    ):
        detail.mesh(group, "FlagstoneLanding", DECK_PAVERS, x, y, 0, 425, 425, 32,
                    blocking=True)
        sign = 1 if x > 0 else -1
        for index, offset in enumerate((222, 308, 389), 1):
            detail.mesh(group, f"FootpathStone{index}", SINGLE_PAVER, x - sign * offset,
                        y + (index % 2) * 13, 0, 91, 99 - 5 * index, 7, yaw=9 * (index - 2))

    detail.mesh("Departure", "FlagstonePortalDeck", DECK_PAVERS, 0, 2135, 0,
                842, 733, 30, blocking=True)
    for side, sign in (("West", -1), ("East", 1)):
        detail.mesh("Departure", side + "DressedPillar", PILLAR, sign * 205, 2097,
                    29, 105, 100, 402)
    detail.mesh("Departure", "CarvedStoneArch", ARCH, 0, 2071, 402, 510, 55, 231)
    for side, sign in (("West", -1), ("East", 1)):
        detail.mesh("Departure", side + "GateSignMount", "cube", sign * 163, 2070, 591,
                    22, 77, 105, material="timber")
    detail.mesh("Departure", "GateSignBacking", "cube", 0, 2038, 617, 474, 19, 108,
                material="timber")

    for group, sign_y, stations in (
        ("Arrival", -1, (792, 982, 1150, 1324, 1500, 1706, 1890, 2060)),
        ("Departure", 1, (735, 912, 1095, 1276, 1453)),
    ):
        for sign_x in (-1, 1):
            flank = "West" if sign_x < 0 else "East"
            for index, station in enumerate(stations, 1):
                detail.mesh(group, f"{flank}RoadShoulder{index}", PAVERS,
                            sign_x * (306 + (index % 3 - 1) * 11), sign_y * station, 0,
                            100 + (index % 3) * 11, 173 + (index % 2) * 19, 8,
                            yaw=(index % 4 - 2) * 4)
                if index in (2, 4, 7):
                    detail.mesh(group, f"{flank}WaysideFlowers{index}", FLOWERS,
                                sign_x * (379 + (index % 3) * 18), sign_y * station, 0,
                                43, 40, 34, yaw=index * 17)

    # The turning stones keep the old lantern on the outside of the approach.
    for index, (x, y) in enumerate((
        (-765, 1091), (-877, 1102), (-982, 1114), (-1090, 1125),
        (-1193, 1138), (-1280, 1150), (-1284, 1244),
    ), 1):
        detail.mesh("Training", f"ApproachStone{index}", SINGLE_PAVER, x, y, 0,
                    96, 101, 7, yaw=(index % 3 - 1) * 10)
