"""Art-only service alcoves outside the original StartHub preparation pads."""

from functools import partial


VILLAGE = "/Game/Stylized_Village/Assets/"
FURNITURE = VILLAGE + "Props/Furniture/"
TARPS = VILLAGE + "Props/Tarps/"
POTS = VILLAGE + "Props/Pots/"
LANTERNS = VILLAGE + "Props/Lanterns/"
TIMBER = VILLAGE + "Architecture/Wood/Pieces/"
MATERIALS = "/Game/Rogue10m/Characters/Arin/Blender/Materials/"
BEAM = TIMBER + "SM_Beam_Long_1"
PLANK = TIMBER + "SM_Plank_Long_2"
CRATE = FURNITURE + "SM_Crate_01"
BARREL = FURNITURE + "SM_SM_Barrel_01"
FLOOR = VILLAGE + "Architecture/StoneFloor/SM_StoneFloor_01_Full"
LAMP = LANTERNS + "SM_Lantern_Box"
PAPER = MATERIALS + "M_Arin_cloth_ivory"
LEATHER = MATERIALS + "M_Arin_leather"
STEEL = "/Game/Weapons/Starter20261110/M_WeaponSteel"
BRASS = "/Game/Weapons/Starter20261110/M_WeaponBrass"


def equipment(detail):
    group = "Equipment"
    place = partial(detail.mesh, group)
    place("StoneApron", FLOOR, -1265, -823, 1, 366, 432, 26, blocking=True)
    for name, x, y in (
        ("SouthOuter", -1400, -969), ("SouthInner", -1143, -969),
        ("NorthOuter", -1400, -672), ("NorthInner", -1143, -672),
    ):
        place(name + "Post", BEAM, x, y, 25, 17, 17, 274, blocking=True)
    place("FrontRail", BEAM, -1271, -968, 286, 279, 20, 22)
    place("BackRail", BEAM, -1271, -674, 285, 279, 20, 22)
    place("CanvasCanopy", TARPS + "SM_Tarp_BigSquare", -1270, -815, 297,
          325, 340, 27, material=PAPER)
    place("OchreShopPennant", TARPS + "SM_Tarp_Banner", -1380, -995, 178,
          44, 14, 96, material="ochre")

    place("CounterBase", CRATE, -1278, -896, 27, 195, 59, 77, blocking=True)
    place("CounterTop", PLANK, -1278, -896, 103, 212, 70, 10, blocking=True)
    for index, (x, y, width, height) in enumerate(((-1304, -904, 15, 24), (-1286, -898, 18, 29), (-1263, -901, 14, 19))):
        place(f"Potion{index + 1}", POTS + "SM_ClayPot_RidgeSmall", x, y, 113,
              width, width, height, material=MATERIALS + "M_Arin_potion_red")
        place(f"Potion{index + 1}Stopper", "cylinder", x, y, 112 + height,
              width * 0.45, width * 0.45, 4, material=LEATHER)
    place("RolledBedroll", BARREL, -1217, -891, 113, 52, 28, 27, yaw=-12, material=LEATHER)
    place("NorthRack", FURNITURE + "SM_Shelf_01", -1265, -697, 27, 213, 42, 210, blocking=True)
    place("DisplaySword", "/Game/Weapons/Starter20261110/SM_StarterGreatSword",
          -1316, -724, 42, 34, 6, 170)
    place("SecondDisplaySword", "/Game/Weapons/Starter20261110/SM_StarterGreatSword",
          -1361, -724, 42, 32, 6, 157, yaw=7)
    place("KnuckleDisplayLeft", "/Game/Weapons/Starter20261110/SM_StarterKnuckle",
          -1215, -903, 114, 8, 14, 5, yaw=80)
    place("KnuckleDisplayRight", "/Game/Weapons/Starter20261110/SM_StarterKnuckle",
          -1199, -904, 114, 8, 14, 5, yaw=97)
    place("OpenSupplyCrate", CRATE, -1395, -748, 27, 57, 54, 51, yaw=8)
    place("FrontCrate", CRATE, -1395, -940, 27, 60, 52, 47, yaw=-11)
    place("Lamp", LAMP, -1145, -972, 203, 27, 26, 44)
    detail.light(group, "CanvasLantern", -1157, -978, 224, 85, 285)


def forge(detail):
    group = "Forge"
    place = partial(detail.mesh, group)
    place("StoneApron", FLOOR, 1371, -829, 1, 440, 472, 26, blocking=True)
    # The fire reads through the opening. Bodies behind it keep the silhouette solid.
    place("HearthFoundation", FLOOR, 1432, -701, 26, 193, 144, 24, blocking=True)
    place("FireboxBack", VILLAGE + "Architecture/Brick/Walls/SM_Brick_Wall_HalfXHalf",
          1432, -646, 50, 175, 40, 148, blocking=True)
    place("FurnaceArch", VILLAGE + "Architecture/Brick/Walls/SM_Brick_Wall_Arch_Full",
          1432, -747, 48, 200, 62, 172, blocking=True)
    place("SideMasonry", VILLAGE + "Architecture/Flagstone/Walls/SM_Flagstone_HalfXHalf",
          1513, -693, 49, 38, 110, 141, blocking=True)
    place("Chimney", VILLAGE + "Architecture/Plaster/SM_Plaster_Chimney_Thick",
          1438, -691, 174, 126, 99, 178)
    place("ChimneyCrown", VILLAGE + "Architecture/Plaster/SM_Plaster_Chimney_Crown",
          1438, -691, 347, 139, 111, 29)
    place("CoalBrazier", LANTERNS + "SM_Lantern_Brazier", 1440, -733, 49, 78, 67, 29)
    for index, (x, y, width, depth) in enumerate(((1420, -754, 24, 15), (1444, -750, 22, 17), (1433, -767, 19, 14))):
        place(f"HotCoals{index + 1}", "cube", x, y, 70, width, depth, 8,
              yaw=18 * index, material="glow")
    detail.light(group, "HearthLight", 1432, -802, 99, 205, 325, (255, 128, 52))

    place("AnvilStump", "cylinder", 1235, -847, 26, 76, 71, 75, material="timber", blocking=True)
    place("AnvilFoot", "cube", 1234, -847, 100, 63, 39, 13, yaw=10, material=STEEL)
    place("AnvilWaist", "cube", 1234, -847, 111, 47, 28, 20, yaw=10, material=STEEL)
    place("AnvilFace", "cube", 1231, -848, 129, 68, 39, 13, yaw=10, material=STEEL, blocking=True)
    place("AnvilHorn", "cube", 1270, -841, 129, 25, 22, 8, yaw=10, material=STEEL)
    place("StockCrate", CRATE, 1497, -954, 26, 120, 63, 66, yaw=-6, blocking=True)
    place("WorkbenchTop", PLANK, 1497, -953, 92, 136, 70, 11, yaw=-6, blocking=True)
    for index, (x, y, yaw) in enumerate(((1469, -960, 10), (1502, -937, 27), (1525, -959, -17))):
        place(f"IronStock{index + 1}", "cube", x, y, 103, 29, 10, 7, yaw=yaw, material=STEEL)
    place("HammerHandle", TIMBER + "SM_Rod_Short", 1499, -971, 104, 41, 5, 5, yaw=13)
    place("HammerHead", "cube", 1518, -967, 105, 11, 21, 11, yaw=13, material=STEEL)
    place("QuenchBarrel", BARREL, 1560, -793, 26, 65, 65, 80, blocking=True)
    place("QuenchWater", "cylinder", 1560, -793, 101, 52, 52, 4,
          material="/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_WellWater")
    place("ForgePennant", TARPS + "SM_Tarp_Banner", 1537, -749, 164, 52, 12, 100,
          material="burgundy")
    place("ForgeLamp", LAMP, 1526, -779, 169, 23, 23, 38)


def expedition(detail):
    group = "Expedition"
    place = partial(detail.mesh, group)
    place("StoneApron", FLOOR, -1405, 869, 1, 366, 444, 26, blocking=True)
    for name, x, y in (
        ("SouthOuter", -1537, 720), ("SouthInner", -1278, 720),
        ("NorthOuter", -1537, 1045), ("NorthInner", -1278, 1045),
    ):
        place(name + "Post", BEAM, x, y, 25, 16, 16, 254, blocking=True)
    place("ShelterRail", BEAM, -1411, 720, 266, 284, 19, 18)
    place("ShelterCanopy", TARPS + "SM_Tarp_BigSquare", -1410, 880, 273,
          325, 358, 33, material="ochre")
    place("DispatchFlag", TARPS + "SM_Tarp_Banner", -1295, 699, 160,
          53, 15, 102, material="burgundy")

    place("MapTableSupport", CRATE, -1413, 850, 27, 149, 71, 73, blocking=True)
    place("MapDeskTop", PLANK, -1413, 850, 98, 187, 84, 10, blocking=True)
    place("UnfurledRouteMap", "cube", -1425, 842, 108, 98, 58, 2, yaw=9, material=PAPER)
    place("MapRouteEast", "cube", -1410, 842, 110, 47, 3, 1, yaw=27, material="burgundy")
    place("MapRouteWest", "cube", -1441, 839, 110, 33, 3, 1, yaw=-17, material="burgundy")
    for index, (x, y) in enumerate(((-1457, 844), (-1422, 835), (-1390, 852))):
        place(f"MapPin{index + 1}", "cylinder", x, y, 110, 7, 7, 5, material=BRASS)
    place("RouteCompass", "cylinder", -1343, 841, 108, 23, 23, 8, material=BRASS)
    place("RouteCompassFace", "cylinder", -1343, 841, 116, 17, 17, 2, material=PAPER)
    place("RouteCompassNeedle", "cube", -1343, 841, 118, 12, 2, 1, yaw=42, material="burgundy")
    for index, (x, y, yaw) in enumerate(((-1485, 974, 8), (-1411, 990, -8), (-1338, 1001, 14))):
        place(f"ExpeditionChest{index + 1}", CRATE, x, y, 27, 59, 58, 61, yaw=yaw, blocking=True)
        place(f"Bedroll{index + 1}", TIMBER + "SM_Rod_Short", x, y, 87, 50, 27, 27,
              yaw=yaw + 9, material=LEATHER if index != 1 else PAPER)
    place("TentLamp", LAMP, -1290, 723, 210, 24, 24, 43)
    detail.light(group, "DispatchLantern", -1280, 710, 230, 75, 250)


def archive(detail):
    group = "Archive"
    place = partial(detail.mesh, group)
    place("StoneApron", FLOOR, 1397, 904, 1, 408, 435, 26, blocking=True)
    for name, x, y in (
        ("SouthInner", 1270, 753), ("SouthOuter", 1534, 753),
        ("NorthInner", 1270, 1048), ("NorthOuter", 1534, 1048),
    ):
        place(name + "Post", BEAM, x, y, 25, 16, 16, 267, blocking=True)
    place("ShelfCanopyRail", BEAM, 1402, 753, 278, 294, 19, 19)
    place("TealAwning", TARPS + "SM_Tarp_BigSquare", 1401, 898, 292,
          338, 325, 25, material="teal")
    place("ArchivePennant", TARPS + "SM_Tarp_Banner", 1297, 735, 169,
          53, 14, 105, material="blue")
    place("ManuscriptShelves", FURNITURE + "SM_Shelf_02", 1399, 998, 26, 221, 45, 240, blocking=True)
    place("ReadingDeskSupport", CRATE, 1383, 828, 26, 147, 58, 80, blocking=True)
    place("ReadingDeskTop", PLANK, 1383, 828, 103, 181, 71, 10, blocking=True)
    place("OpenAtlasCover", "cube", 1366, 820, 113, 56, 38, 4, yaw=-12, material=LEATHER)
    place("OpenAtlasLeftPage", "cube", 1354, 822, 117, 27, 34, 2, yaw=-12, material=PAPER)
    place("OpenAtlasRightPage", "cube", 1380, 818, 117, 26, 34, 2, yaw=-12, material=PAPER)
    place("ReferenceFolio", "cube", 1438, 833, 113, 31, 43, 7, yaw=19, material="burgundy")
    place("ReferenceFolioPages", "cube", 1438, 833, 119, 27, 39, 5, yaw=19, material=PAPER)
    place("ReferenceFolioTop", "cube", 1438, 833, 124, 31, 43, 2, yaw=19, material="burgundy")
    place("MapScrollJar", POTS + "SM_ClayPot_Big", 1518, 870, 27, 67, 66, 83)
    for index, (x, y, height) in enumerate(((1518, 855, 62), (1508, 874, 54), (1530, 869, 76), (1527, 881, 47))):
        place(f"RolledParchment{index + 1}", "cylinder", x, y, 99, 9, 9, height, material=PAPER)
        place(f"ScrollTie{index + 1}", "cylinder", x, y, 105 + height * 0.53,
              10, 10, 3, material="ochre")
    place("RecordChest", CRATE, 1296, 950, 27, 52, 51, 49, yaw=-10)
    place("ShelfLamp", LAMP, 1275, 754, 212, 24, 24, 42)
    detail.light(group, "ArchiveLantern", 1260, 750, 232, 85, 250)


def author(detail):
    equipment(detail)
    forge(detail)
    expedition(detail)
    archive(detail)
