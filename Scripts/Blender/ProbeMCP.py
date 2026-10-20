import bpy
result = {"version": bpy.app.version_string,
          "addon_enabled": "codex_blender_mcp_bridge" in bpy.context.preferences.addons,
          "active_scene": bpy.context.scene.name, "object_count": len(bpy.data.objects)}
