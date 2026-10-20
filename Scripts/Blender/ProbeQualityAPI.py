"""Read-only installed Blender API probe before an expensive bake."""
import json
import bpy

rna=bpy.ops.uv.smart_project.get_rna_type()
print(json.dumps({"blender":bpy.app.version_string,"smart_uv_properties":list(rna.properties.keys()),
 "margin_methods":[item.identifier for item in rna.properties["margin_method"].enum_items],
 "texture_nodes":[name for name in ("ShaderNodeVertexColor","ShaderNodeEmission","ShaderNodeBump") if hasattr(bpy.types,name)]}),flush=True)
