"""Distant woodland and small stretches of low stone along the existing fence."""

VILLAGE = "/Game/Stylized_Village/Assets"
TREES = f"{VILLAGE}/Foliage/Trees/Tree_"
ROCKS = f"{VILLAGE}/Stones/Boulder/SM_Boulder_"
WALL = f"{VILLAGE}/Architecture/Flagstone/Walls/SM_Flagstone_HalfXFull"
BUSH = f"{VILLAGE}/Foliage/Bushes/SM_Bush_01"
FLOWERS = f"{VILLAGE}/Foliage/Flowers/BabyBlueEyesAndPoppies/SM_BabyBlueEyes_Medium_01"


def author(detail):
    group = "Perimeter"
    # The deck is visual only. The original 60 m ground and collision still
    # determine where players can stand, including beside the outer fence.
    detail.mesh(group, "WoodlandGround", "plane", 0, 0, -26, 46000, 46000, 1,
                material="grass")
    woodland = (
        (-3450, -2350, 525, 735, "Medium_Green"), (-3830, -1320, 660, 900, "Large_Green"),
        (-3860, -210, 685, 805, "Large_Green"), (-3775, 840, 475, 695, "Medium_Green"),
        (-3680, 2030, 635, 865, "Large_Green"), (-4290, 2860, 640, 775, "Large_Yellow"),
        (3410, -2290, 620, 770, "Large_Green"), (3830, -1340, 590, 825, "Large_Green"),
        (3640, -220, 460, 683, "Medium_Yellow"), (3910, 880, 715, 868, "Large_Green"),
        (3810, 2320, 650, 828, "Large_Green"), (4360, 3070, 560, 783, "Large_Green"),
        (-2890, 3470, 660, 853, "Large_Green"), (-2080, 3820, 570, 786, "Large_Green"),
        (-1290, 3520, 475, 680, "Medium_Green"), (-590, 4050, 600, 902, "Large_Green"),
        (510, 4130, 740, 1040, "Large_Green"), (1550, 3430, 530, 738, "Medium_Green"),
        (2515, 3840, 684, 893, "Large_Green"), (3190, 3490, 517, 723, "Medium_Yellow"),
        (-2430, -3670, 657, 865, "Large_Green"), (-1240, -3830, 600, 808, "Large_Green"),
        (1190, -3900, 470, 684, "Medium_Green"), (2320, -3630, 620, 844, "Large_Green"),
        (-5340, 4510, 910, 1160, "Large_Green"), (-2250, 5360, 1030, 1290, "Large_Green"),
        (-760, 5570, 1040, 1150, "Large_Green"), (960, 5630, 925, 1310, "Large_Green"),
        (2400, 5280, 940, 1090, "Large_Green"), (5190, 4580, 920, 1220, "Large_Green"),
    )
    for index, (x, y, width, height, variant) in enumerate(woodland, 1):
        detail.mesh(group, f"Tree{index:02}", TREES + variant, x, y, -27,
                    width, width * 0.96, height, yaw=31 * index % 360)

    for index, (x, y, sx, sy, sz, yaw) in enumerate((
        (-3920, 3530, 1230, 840, 410, 18), (4140, 3620, 1120, 925, 435, -33),
        (-3310, 5610, 2100, 1060, 634, -16), (3470, 5650, 2200, 1210, 710, 14),
        (-5490, 690, 1600, 920, 525, 61), (5560, 660, 1650, 1130, 610, 106),
        (-4990, -3220, 1150, 920, 360, 47), (4950, -3170, 1125, 920, 380, 93),
    ), 1):
        detail.mesh(group, f"WoodlandBoulder{index:02}", ROCKS + "Large_01", x, y, -60,
                    sx, sy, sz, yaw=yaw)

    stone_sections = (
        (-1500, -2858, 1250, 0), (1485, -2858, 1200, 0),
        (-1875, 2858, 1100, 0), (1920, 2858, 1080, 0),
        (-2858, -1650, 1030, 90), (-2858, 1480, 1060, 90),
        (2858, -1690, 1020, 90), (2858, 1550, 1075, 90),
    )
    for index, (x, y, length, yaw) in enumerate(stone_sections, 1):
        for part, offset in enumerate((-length / 3, 0, length / 3), 1):
            offset_x, offset_y = (offset, 0) if yaw == 0 else (0, offset)
            detail.mesh(group, f"StoneFooting{index:02}_{part}", WALL,
                        x + offset_x, y + offset_y, -9,
                        length / 3 + 8, 51, 58 + (part % 2) * 8, yaw=yaw)

    for index, (x, y, size) in enumerate((
        (-2540, -2570, 125), (-965, -2715, 96), (973, -2670, 105), (1780, -2720, 120),
        (-2650, -1290, 110), (-2710, 165, 122), (-2700, 1940, 108),
        (2690, -985, 111), (2690, 250, 110), (2700, 1380, 128),
        (-1600, 2650, 109), (-1060, 2640, 115), (1170, 2610, 107), (2190, 2630, 116),
    ), 1):
        detail.mesh(group, f"FenceShrub{index:02}", BUSH, x, y, -3,
                    size, size * 0.84, size * 0.63, yaw=27 * index % 360)
        detail.mesh(group, f"FenceFlower{index:02}", FLOWERS, x + 41, y - 35, 1,
                    42, 43, 34, yaw=19 * index % 360)
