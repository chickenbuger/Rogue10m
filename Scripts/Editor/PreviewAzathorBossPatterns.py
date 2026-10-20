"""Hold three actual native windups in disposable slow-motion for image review.

This owns disposable PIE only. Invoke advance() after each captured image and
finish() after the third; a wall-clock timeout always releases the preview.
"""
import importlib.util
from pathlib import Path
import time
import unreal

ROOT = Path("D:/Project/Rogue10m")
SINGLETON = "_azathor_visual_preview"


def main():
    old = getattr(unreal, SINGLETON, None)
    if old and not old.state["finished"]:
        raise RuntimeError("Existing preview is still running")
    spec = importlib.util.spec_from_file_location("azathor_preview_qa", ROOT / "Scripts/Editor/ValidateAzathorBossPatterns.py")
    qa = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(qa)
    qa.SINGLETON = "_azathor_visual_qa_state"
    qa.REPORT = ROOT / "Feature/doc/evidence/azathor-boss-patterns-20261011/visual-preview.json"
    qa.CASES = (("visual_frontal", "FRONTAL_WAVE", "hit"),
                ("visual_zone", "ABYSS_ZONE", "hit"),
                ("visual_enrage", "FRONTAL_WAVE", "enrage"))
    qa.preview_paused_at = None
    qa.preview_original_dilation = 1.0
    original_tick = qa.tick_impl

    def observe(game, pawn):
        if unreal.GameplayStatics.get_time_seconds(game) - qa.state["attack_start"] < 0.35:
            return
        component = qa.state["component"]
        qa.check("native_visual_warning_is_registered", component.is_telegraph_visible())
        origin = qa.state["origin"]
        # Let a native camera update run after the pose change before pausing.
        if not qa.state["case"].get("camera_prepared"):
            qa.place(pawn, origin[0] + 425.0, origin[1] - 850.0, origin[2] + 600.0)
            qa.state["controller"].set_control_rotation(unreal.Rotator(pitch=-42.0, yaw=90.0, roll=0.0))
            qa.state["case"]["camera_prepared"] = True
            return
        qa.state["case"]["native_decal"] = [{"location": qa.vector_data(d.get_world_location()),
            "size": qa.vector_data(d.get_editor_property("decal_size")),
            "material": qa.path_of(d.get_decal_material())}
            for d in qa.state["monster"].get_components_by_class(unreal.DecalComponent)]
        qa.state["case"]["capture_pending"] = True
        qa.write_report()
        # Keep rendering/ticking alive instead of retaining a paused D3D12 view.
        # Timings are verified by the separate real-time QA, never this preview.
        qa.preview_original_dilation = unreal.GameplayStatics.get_global_time_dilation(game)
        unreal.GameplayStatics.set_global_time_dilation(game, 0.001)
        qa.state["case"]["visual_preview_time_dilation"] = 0.001
        qa.write_report()
        qa.preview_paused_at = time.monotonic()
        unreal.log("[AzathorPreview] Holding actual " + qa.state["case"]["name"] + "; capture Editor image then advance().")

    def finish():
        game = qa.world()
        if game:
            unreal.GameplayStatics.set_global_time_dilation(game, qa.preview_original_dilation)
        qa.preview_paused_at = None
        captured = sum(c.get("status") == "captured" for c in qa.state["report"]["cases"])
        qa.finish(f"{captured}/3 native warnings captured for pixel review.", captured != 3)

    def advance():
        if qa.preview_paused_at is None:
            raise RuntimeError("Preview is not waiting for capture")
        game = qa.world()
        qa.state["component"].cancel_attack()
        qa.state["case"].update(status="captured", capture_pending=False)
        unreal.GameplayStatics.set_global_time_dilation(game, qa.preview_original_dilation)
        qa.preview_paused_at = None
        qa.state["case_index"] += 1
        if qa.state["case_index"] == len(qa.CASES):
            finish()
        else:
            qa.phase("prepare_case")

    def tick(delta):
        if qa.preview_paused_at is not None:
            if time.monotonic() - qa.preview_paused_at > 180.0:
                game = qa.world()
                if game:
                    unreal.GameplayStatics.set_global_time_dilation(game, qa.preview_original_dilation)
                qa.preview_paused_at = None
                qa.finish("Visual capture timed out", True)
            return
        original_tick(delta)

    qa.observe_case = observe
    qa.tick_impl = tick
    qa.advance = advance
    qa.preview_finish = finish
    setattr(unreal, SINGLETON, qa)
    qa.main()


if __name__ == "__main__":
    main()
