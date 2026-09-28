"""Open the integrated Gothic UI showcase in the running Unreal Editor."""

import unreal


SHOWCASE_PATH = "/Game/Widget/Debug/WBP_GothicUIShowcase"


def main() -> None:
    asset = unreal.EditorAssetLibrary.load_asset(SHOWCASE_PATH)
    if not asset:
        raise RuntimeError(f"Missing showcase asset: {SHOWCASE_PATH}")
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
    unreal.log("[Rogue10mGothicUI] Opened Gothic UI showcase")


if __name__ == "__main__":
    main()
