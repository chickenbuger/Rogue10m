"""Material-only batch for official ProgrammaticToolset.execute_tool_script.

Import is inert. run() uses registered tools only; no unreal import, filesystem
write, save, actor change, terrain edit, automatic undo or partial-asset cleanup.
The owner must verify project identity, clean/45 terrain and current source MI
appearance before dispatch. The literal below pins the successful fresh plan.
"""
import json


PLAN = {
    "target": "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A",
    "material": "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface",
    "source_material": "/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_EarthPath",
    "texture": "/Game/StarterContent/Textures/T_Ground_Gravel_D",
    "tint_rgba": [0.8600000143051147, 0.7799999713897705, 0.6499999761581421, 1.0],
    "preflight_evidence": "Feature/doc/evidence/landscape-road-surface20261117/preflight.json",
    "preflight_status": "read_only_preflight_passed_prototype_not_authored",
    "width_cm": 800.0,
    "direct_ray_count": 4998,
    "sphere_fallback_count": 2,
}
MAT = "editor_toolset.toolsets.material.MaterialTools."
OBJ = "editor_toolset.toolsets.object.ObjectTools."
ASSET = "editor_toolset.toolsets.asset.AssetTools."


def current_level():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level", "{}")["returnValue"]


def pie_running():
    return execute_tool("EditorToolset.EditorAppToolset.IsPIERunning", "{}")["returnValue"]


def exists(path):
    return execute_tool(ASSET + "exists", json.dumps({"path": path}))["returnValue"]


def create_material(folder, name):
    return execute_tool(MAT + "create_material", json.dumps({"folder_path": folder, "asset_name": name}))["returnValue"]


def add_node(material, kind, x, y):
    return execute_tool(MAT + "add_expression", json.dumps({
        "material_or_function": material, "expression_class": {"refPath": "/Script/Engine." + kind},
        "x": x, "y": y}))["returnValue"]


def property_schema(instance):
    return json.loads(execute_tool(OBJ + "list_properties", json.dumps({"instance": instance}))["returnValue"])


def set_properties(instance, values):
    # values is itself JSON text inside the outer tool argument JSON.
    return execute_tool(OBJ + "set_properties", json.dumps({
        "instance": instance, "values": json.dumps(values, allow_nan=False)}))["returnValue"]


def get_properties(instance, names):
    return json.loads(execute_tool(OBJ + "get_properties", json.dumps({
        "instance": instance, "properties": names}))["returnValue"])


def input_names(node):
    return execute_tool(MAT + "get_expression_input_names", json.dumps({"expression": node}))["returnValue"]


def output_names(node):
    return execute_tool(MAT + "get_expression_output_names", json.dumps({"expression": node}))["returnValue"]


def connect(source, output, destination, pin):
    # Void-return tools raise on failure; do not interpret their result as bool.
    execute_tool(MAT + "connect_expressions", json.dumps({"from_expression": source,
        "from_output_name": output, "to_expression": destination, "to_input_name": pin}))


def connect_output(node, prop):
    execute_tool(MAT + "connect_to_output", json.dumps({
        "expression": node, "output_name": "", "material_property": prop}))


def expression_list(material):
    return execute_tool(MAT + "get_expressions", json.dumps({"material_or_function": material}))["returnValue"]


def graph_inputs(material, node):
    return execute_tool(MAT + "get_expression_inputs", json.dumps({
        "material_or_function": material, "expression": node}))["returnValue"]


def material_output(material, prop):
    return execute_tool(MAT + "get_property_input", json.dumps({
        "material": material, "material_property": prop}))["returnValue"]


def recompile(material):
    execute_tool(MAT + "recompile", json.dumps({"material_or_function": material}))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def matches(actual, expected):
    if isinstance(expected, dict):
        if "refPath" in expected:
            return isinstance(actual, dict) and actual["refPath"].split(".")[0] == expected["refPath"].split(".")[0]
        return isinstance(actual, dict) and all(k in actual and matches(actual[k], value) for k, value in expected.items())
    if isinstance(expected, (float, int)) and not isinstance(expected, bool):
        return isinstance(actual, (float, int)) and abs(actual - expected) < 1e-6
    return actual == expected


def configure(instance, values):
    schema = property_schema(instance)
    for key, value in values.items():
        require(key in schema, "Property is not in the advertised editable schema: " + key)
        if "enum" in schema[key]:
            require(value in schema[key]["enum"], "Unadvertised enum value: " + key)
    require(set_properties(instance, values) is True, "set_properties returned false")
    observed = get_properties(instance, list(values))
    require(matches(observed, values), "Property readback differs: " + json.dumps(observed))
    return observed


def run():
    report = {"status": "started_material_only", "success": False, "material_path": PLAN["material"],
              "source_appearance": PLAN, "nodes": {}, "readbacks": {}, "step": "guards",
              "packages_saved": [], "actors_changed": [], "terrain_changed": False,
              "automatic_rollback": False, "partial_assets_deleted": False}
    try:
        require(current_level() == PLAN["target"] and pie_running() is False, "Exact idle Crop level required")
        with open(PLAN["preflight_evidence"], "r", encoding="utf-8") as stream:
            preflight = json.load(stream)
        require(preflight["status"] == PLAN["preflight_status"] and preflight["target"] == PLAN["target"], "Fresh preflight required")
        for source_key, plan_key in (("material_path", "material"), ("texture", "texture"),
                                     ("tint_rgba", "tint_rgba"), ("width_cm", "width_cm"),
                                     ("direct_ray_count", "direct_ray_count"), ("sphere_fallback_count", "sphere_fallback_count")):
            require(preflight[source_key] == PLAN[plan_key], "Pinned preflight differs: " + source_key)
        require(exists(PLAN["material"]) is False, "Material already exists; inspect partial work without retry")
        require(exists(PLAN["source_material"]) is True and exists(PLAN["texture"]) is True, "Existing appearance source unavailable")
        report["step"] = "create material"
        folder, name = PLAN["material"].rsplit("/", 1)
        material = create_material(folder, name)
        report["material"] = material
        require(material["refPath"].split(".")[0] == PLAN["material"], "Unexpected created material")
        report["readbacks"]["material"] = configure(material, {"materialDomain": "MD_DeferredDecal", "blendMode": "BLEND_Translucent"})
        definitions = [
            ("world", "MaterialExpressionWorldPosition", -1050, -100, {}),
            ("xy", "MaterialExpressionComponentMask", -850, -100, {"r": True, "g": True, "b": False, "a": False}),
            ("scale", "MaterialExpressionScalarParameter", -850, 120, {"parameterName": "WorldUVScale", "defaultValue": 0.002}),
            ("uv", "MaterialExpressionMultiply", -650, -100, {}),
            ("diffuse", "MaterialExpressionTextureSample", -430, -100, {"texture": {"refPath": PLAN["texture"]}, "samplerType": "SAMPLERTYPE_Color", "samplerSource": "SSM_Wrap_WorldGroupSettings"}),
            ("tint", "MaterialExpressionVectorParameter", -430, 150, {"parameterName": "RoadTint", "defaultValue": dict(zip(("r", "g", "b", "a"), PLAN["tint_rgba"]))}),
            ("tinted", "MaterialExpressionMultiply", -180, -100, {}),
            ("opacity", "MaterialExpressionScalarParameter", -180, 190, {"parameterName": "RoadOpacity", "defaultValue": 0.9}),
            ("roughness", "MaterialExpressionScalarParameter", -180, 350, {"parameterName": "RoadRoughness", "defaultValue": 0.9}),
        ]
        for key, kind, x, y, values in definitions:
            report["step"] = "create/configure node " + key
            node = add_node(material, kind, x, y)
            report["nodes"][key] = node
            if values:
                report["readbacks"][key] = configure(node, values)
        nodes = report["nodes"]
        require(input_names(nodes["xy"]) in ([""], ["None"]), "ComponentMask anonymous input changed")
        links = (("world", "", "xy", ""), ("xy", "", "uv", "A"), ("scale", "", "uv", "B"),
                 ("uv", "", "diffuse", "UVs"), ("diffuse", "RGB", "tinted", "A"), ("tint", "RGB", "tinted", "B"))
        for source, output, destination, pin in links:
            report["step"] = "connect " + source + " to " + destination + "/" + pin
            if output:
                require(output in output_names(nodes[source]), "Output pin missing: " + output)
            if pin:
                require(pin in input_names(nodes[destination]), "Input pin missing: " + pin)
            connect(nodes[source], output, nodes[destination], pin)
        for key, prop in (("tinted", "MP_BaseColor"), ("opacity", "MP_Opacity"), ("roughness", "MP_Roughness")):
            report["step"] = "material output " + prop
            connect_output(nodes[key], prop)
            observed = material_output(material, prop)
            require(observed["expression"]["refPath"] == nodes[key]["refPath"], "Output connection readback differs")
        report["step"] = "compile"
        # Installed tool internally checks RecompileMaterial's TArray<FString>.
        # The registered wrapper returns void and raises on shader errors.
        recompile(material)
        expressions = expression_list(material)
        require(len(expressions) == 9 and {r["refPath"] for r in expressions} == {r["refPath"] for r in nodes.values()}, "Graph is not the exact nine-node set")
        report["graph_readback"] = {key: graph_inputs(material, nodes[key]) for key in ("xy", "uv", "diffuse", "tinted")}
        require(current_level() == PLAN["target"] and pie_running() is False, "Editor context changed")
        report.update(status="unsaved_nine_node_material_compiled_requires_owner_review", success=True, compile_passed=True, step="complete")
        return report
    except Exception as error:
        report.update(status="partial_material_failure_preserved_no_automatic_retry", error=str(error))
        return report
