"""Landmark dressing for the existing 60-metre StartHub."""

VILLAGE = "/Game/Stylized_Village/Assets"
BEAM = f"{VILLAGE}/Architecture/Wood/Pieces/SM_Beam_Long_1"
SHORT_BEAM = f"{VILLAGE}/Architecture/Wood/Pieces/SM_Beam_Short_1"
BRICK_WALL = f"{VILLAGE}/Architecture/Brick/Walls/SM_Brick_Wall_HalfXHalf"
STONE_PAVERS = f"{VILLAGE}/Architecture/StoneFloor/SM_StoneFloor_01_Full"
LANTERN = f"{VILLAGE}/Props/Lanterns/SM_Lantern_Box"
BARREL = f"{VILLAGE}/Props/Furniture/SM_SM_Barrel_01"
CRATE = f"{VILLAGE}/Props/Furniture/SM_Crate_01"
POT = f"{VILLAGE}/Props/Pots/SM_ClayPot_WideSmall"
FLOWERS = f"{VILLAGE}/Foliage/Flowers/BabyBlueEyesAndPoppies/SM_BabyBlueEyes_Medium_01"
LANTERN_GLOW = "/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_LanternGlow"


def lantern(detail, group, name, x, y, bottom, height, intensity, radius):
    detail.mesh(group, name + "Box", LANTERN, x, y, bottom, 26, 26, height)
    detail.mesh(group, name + "Flame", "cube", x, y, bottom + 9, 8, 8, 14,
                material=LANTERN_GLOW)
    detail.light(group, name + "Light", x, y - 24, bottom + 16, intensity, radius)


def arrangement(detail, group, name, x, y, z, size):
    detail.mesh(group, name + "Pot", POT, x, y, z, size, size, size * 0.65)
    detail.mesh(group, name + "Flowers", FLOWERS, x, y, z + size * 0.47,
                size * 0.83, size * 0.79, size * 0.88, yaw=17)


def arrival(detail):
    group = "Arrival"
    # The visual threshold is inside the perimeter fence, behind the spawn.
    # The narrowest solid inner edge is 335 cm from the 640 cm arrival lane's centerline.
    for side, sign in (("West", -1), ("East", 1)):
        x = sign * 405
        detail.mesh(group, side + "Footing", BRICK_WALL, x, -2190, 0, 140, 143, 116,
                    blocking=True)
        detail.mesh(group, side + "Cap", STONE_PAVERS, x, -2190, 111, 146, 151, 14)
        detail.mesh(group, side + "LanternPost", BEAM, x, -2190, 123, 24, 27, 136)
        detail.mesh(group, side + "LanternArm", SHORT_BEAM, x, -2209, 242, 21, 61, 14)
        lantern(detail, group, side + "Lantern", x, -2235, 203, 43, 205, 330)
        detail.mesh(group, side + "FieldstoneWing", BRICK_WALL, sign * 635, -2190, 0,
                    334, 106, 63, blocking=True)
        detail.mesh(group, side + "FieldstoneCap", STONE_PAVERS, sign * 635, -2190, 58,
                    338, 119, 11)
        for post, offset in enumerate((530, 733), 1):
            detail.mesh(group, f"{side}RailPost{post}", SHORT_BEAM, sign * offset, -2190, 58,
                        24, 25, 89)
        detail.mesh(group, side + "Rail", BEAM, sign * 635, -2190, 102, 222, 21, 26)
        arrangement(detail, group, side + "ThresholdFlowers", sign * 580, -2270, 0, 56)


def plaza(detail):
    group = "Plaza"
    # Keep the additions inside the 555 cm width of the existing ridge.
    detail.mesh(group, "Windlass", BEAM, 0, 0, 313, 440, 31, 32)
    detail.mesh(group, "RopeDrum", SHORT_BEAM, 5, 0, 308, 103, 51, 49)
    for index, offset in enumerate((-18, -6, 6, 18), 1):
        detail.mesh(group, f"RopeWrap{index}", "cube", offset, -3, 304, 7, 54, 52,
                    material="ochre")
    detail.mesh(group, "BucketRope", "cylinder", 15, -30, 204, 5, 5, 146, material="ochre")
    detail.mesh(group, "DrawBucket", BARREL, 15, -30, 170, 51, 49, 45, yaw=17)
    detail.mesh(group, "CrankPin", SHORT_BEAM, 237, 0, 314, 42, 28, 26)
    detail.mesh(group, "CrankLever", SHORT_BEAM, 252, 0, 256, 14, 16, 83)
    detail.mesh(group, "CrankGrip", SHORT_BEAM, 266, -9, 257, 36, 23, 20)
    for side, sign in (("West", -1), ("East", 1)):
        x = sign * 222
        detail.mesh(group, side + "EaveHook", SHORT_BEAM, x, -27, 335, 17, 64, 14)
        lantern(detail, group, side + "EaveLantern", x, -55, 286, 45, 150, 270)
        arrangement(detail, group, side + "WellFlowers", sign * 235, -116, 12, 69)
    detail.mesh(group, "RinseBarrel", BARREL, 245, 84, 12, 66, 65, 68, yaw=-15)


def departure(detail):
    group = "Departure"
    # The arch occupies X ±250 at Y 2135; the original pad starts at Y 1775.
    # The stone shoulders frame the run portal and leave the crossing/trigger untouched.
    detail.mesh(group, "ApproachPaving", STONE_PAVERS, 0, 1671, 5, 538, 197, 7)
    for side, sign in (("West", -1), ("East", 1)):
        x = sign * 434
        detail.mesh(group, side + "OuterButtress", BRICK_WALL, x, 2228, 0, 350, 167, 157,
                    yaw=90, blocking=True)
        detail.mesh(group, side + "ButtressCap", STONE_PAVERS, x, 2228, 152, 176, 356, 14)
        detail.mesh(group, side + "LanternPlinth", BRICK_WALL, sign * 409, 2034, 4,
                    111, 106, 91, blocking=True)
        detail.mesh(group, side + "LanternPlinthCap", STONE_PAVERS, sign * 409, 2034, 90,
                    115, 110, 13)
        detail.mesh(group, side + "LanternPost", SHORT_BEAM, sign * 409, 2034, 102,
                    20, 20, 101)
        detail.mesh(group, side + "LanternArm", SHORT_BEAM, sign * 409, 2018, 190,
                    20, 52, 13)
        lantern(detail, group, side + "PortalLantern", sign * 409, 1997, 148, 43, 240, 350)
        arrangement(detail, group, side + "PortalFlowers", sign * 478, 1959, 5, 71)


def practice_dummy(detail, name, x, y):
    group = "Training"
    detail.mesh(group, name + "Footing", STONE_PAVERS, x, y, 3, 92, 81, 13, yaw=7)
    detail.mesh(group, name + "Post", SHORT_BEAM, x, y, 14, 23, 22, 118, blocking=True)
    detail.mesh(group, name + "Arms", SHORT_BEAM, x, y, 112, 134, 28, 28, blocking=True)
    detail.mesh(group, name + "Body", "cylinder", x, y, 75, 63, 53, 72,
                material="ochre", blocking=True)
    for index, bottom in enumerate((83, 133), 1):
        detail.mesh(group, f"{name}Binding{index}", "cylinder", x, y, bottom,
                    65, 55, 7, material="timber")
    detail.mesh(group, name + "Head", "cylinder", x, y, 143, 42, 39, 43, material="ochre")
    detail.mesh(group, name + "ChestPatch", "cube", x, y - 27, 99, 32, 6, 32,
                yaw=2, material="burgundy")
    detail.mesh(group, name + "StrikeCenter", "cube", x, y - 31, 108, 14, 5, 14,
                material="stone")


def training(detail):
    group = "Training"
    # Move north of the expedition desk so the west village doors remain usable.
    left, right, south, north = -1590, -970, 1280, 1640
    middle = (left + right) / 2
    detail.mesh(group, "PackedEarth", "cube", middle, (south + north) / 2, -3,
                right - left, north - south, 7, material="earth")
    for name, x, y in (("SouthWest", left, south), ("SouthEast", right, south),
                       ("NorthWest", left, north), ("NorthEast", right, north),
                       ("EntryWest", middle - 111, south), ("EntryEast", middle + 111, south)):
        detail.mesh(group, name + "FencePost", SHORT_BEAM, x, y, 4, 22, 23, 102,
                    blocking=True)
    east_wing = (right + middle + 111) / 2
    west_wing = (left + middle - 111) / 2
    for name, x, y, length, yaw in (("West", left, (south + north) / 2, 360, 90),
                                    ("East", right, (south + north) / 2, 360, 90),
                                    ("Back", middle, north, 620, 0),
                                    ("FrontWest", west_wing, south, 199, 0),
                                    ("FrontEast", east_wing, south, 199, 0)):
        for rail, height in (("Lower", 32), ("Upper", 72)):
            detail.mesh(group, name + rail + "FenceRail", BEAM, x, y, height,
                        length, 17, 16, yaw=yaw, blocking=True)
    practice_dummy(detail, "WestDummy", middle - 119, 1550)
    practice_dummy(detail, "EastDummy", middle + 112, 1546)
    detail.mesh(group, "PracticeGearCrate", CRATE, left + 78, 1512, 4, 76, 79, 68,
                yaw=-8, blocking=True)
    detail.mesh(group, "PracticeGearBarrel", BARREL, right - 62, 1522, 4, 59, 59, 78,
                yaw=20, blocking=True)


def author(detail):
    arrival(detail)
    plaza(detail)
    departure(detail)
    training(detail)
