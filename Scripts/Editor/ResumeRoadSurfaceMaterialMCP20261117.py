"""Resume only the observed five-node, unsaved owned material via MCP tools.

The original batch and failure evidence are preserved. No material recreation,
node deletion, save, actor/terrain edits, automatic undo or retry is performed.
"""
import json

TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
PACKAGE = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface"
MATERIAL = {"refPath": PACKAGE + ".M_RoadSurface"}
TEXTURE = {"refPath": "/Game/StarterContent/Textures/T_Ground_Gravel_D.T_Ground_Gravel_D"}
TINT = {"r": 0.8600000143051147, "g": 0.7799999713897705, "b": 0.6499999761581421, "a": 1.0}
MAT = "editor_toolset.toolsets.material.MaterialTools."
OBJ = "editor_toolset.toolsets.object.ObjectTools."


def current_level():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level", "{}")["returnValue"]


def pie_running():
    return execute_tool("EditorToolset.EditorAppToolset.IsPIERunning", "{}")["returnValue"]


def expressions():
    return execute_tool(MAT + "get_expressions", json.dumps({"material_or_function": MATERIAL}))["returnValue"]


def object_class(obj):
    return execute_tool(OBJ + "get_class", json.dumps({"instance": obj}))["returnValue"]


def props(obj, keys):
    return json.loads(execute_tool(OBJ + "get_properties", json.dumps({"instance": obj, "properties": keys}))["returnValue"])


def property_schema(obj):
    return json.loads(execute_tool(OBJ + "list_properties", json.dumps({"instance": obj}))["returnValue"])


def setprops(obj, values):
    return execute_tool(OBJ + "set_properties", json.dumps({"instance": obj, "values": json.dumps(values, allow_nan=False)}))["returnValue"]


def add_node(kind, x, y):
    return execute_tool(MAT + "add_expression", json.dumps({"material_or_function": MATERIAL,
        "expression_class": {"refPath": "/Script/Engine." + kind}, "x": x, "y": y}))["returnValue"]


def inputs(obj):
    return execute_tool(MAT + "get_expression_input_names", json.dumps({"expression": obj}))["returnValue"]


def connect(source, output, destination, pin):
    execute_tool(MAT + "connect_expressions", json.dumps({"from_expression": source,
        "from_output_name": output, "to_expression": destination, "to_input_name": pin}))


def connect_output(node, prop):
    execute_tool(MAT + "connect_to_output", json.dumps({"expression": node,
        "output_name": "", "material_property": prop}))


def output_input(prop):
    return execute_tool(MAT + "get_property_input", json.dumps({"material": MATERIAL, "material_property": prop}))["returnValue"]


def graph_inputs(obj):
    return execute_tool(MAT + "get_expression_inputs", json.dumps({"material_or_function": MATERIAL, "expression": obj}))["returnValue"]


def recompile():
    execute_tool(MAT + "recompile", json.dumps({"material_or_function": MATERIAL}))


def require(value, message):
    if not value:
        raise RuntimeError(message)


def matches(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(k in actual and matches(actual[k], v) for k, v in expected.items())
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < 0.000001
    return actual == expected


def configure(obj, values):
    schema = property_schema(obj)
    for key, value in values.items():
        require(key in schema, "Unadvertised property: " + key)
        if "enum" in schema[key]:
            require(value in schema[key]["enum"], "Unadvertised enum: " + key)
    require(setprops(obj, values) is True, "Property set returned false")
    read = props(obj, list(values))
    require(matches(read, values), "Property readback differs: " + json.dumps(read))
    return read


def run():
    report = {"status": "started_exact_partial_resume", "success": False, "step": "guards",
              "material": MATERIAL, "new_nodes": {}, "original_failure_preserved": True,
              "packages_saved": [], "actors_changed": [], "terrain_changed": False,
              "automatic_rollback": False, "node_deletions": []}
    try:
        require(current_level() == TARGET and pie_running() is False, "Exact idle Crop editor required")
        require(props(MATERIAL, ["materialDomain", "blendMode"]) ==
                {"materialDomain": "MD_DeferredDecal", "blendMode": "BLEND_Translucent"}, "Partial material settings differ")
        kinds = {"world": "MaterialExpressionWorldPosition", "xy": "MaterialExpressionComponentMask",
                 "scale": "MaterialExpressionScalarParameter", "uv": "MaterialExpressionMultiply",
                 "diffuse": "MaterialExpressionTextureSample"}
        nodes = {key: {"refPath": MATERIAL["refPath"] + ":" + kind + "_0"} for key, kind in kinds.items()}
        found = expressions()
        require(len(found) == 5 and {r["refPath"] for r in found} == {r["refPath"] for r in nodes.values()}, "Partial node identities/count differ; no retry")
        for key, kind in kinds.items():
            require(object_class(nodes[key])["refPath"] == "/Script/Engine." + kind, "Partial node class differs")
        require(props(nodes["xy"], ["r", "g", "b", "a"]) == {"r": True, "g": True, "b": False, "a": False}, "Partial mask differs")
        require(matches(props(nodes["scale"], ["parameterName", "defaultValue"]),
                        {"parameterName": "WorldUVScale", "defaultValue": 0.002}), "Partial scale differs")
        require(props(nodes["diffuse"], ["texture", "samplerType", "samplerSource"]) ==
                {"texture": "None", "samplerType": "SAMPLERTYPE_Color", "samplerSource": "SSM_Wrap_WorldGroupSettings"}, "Observed partial texture state changed")
        report["existing_nodes"] = nodes.copy()
        report["step"] = "set only missing texture using full object path"
        report["texture_readback"] = configure(nodes["diffuse"], {"texture": TEXTURE})
        definitions = [
            ("tint", "MaterialExpressionVectorParameter", -430, 150, {"parameterName": "RoadTint", "defaultValue": TINT}),
            ("tinted", "MaterialExpressionMultiply", -180, -100, {}),
            ("opacity", "MaterialExpressionScalarParameter", -180, 190, {"parameterName": "RoadOpacity", "defaultValue": 0.9}),
            ("roughness", "MaterialExpressionScalarParameter", -180, 350, {"parameterName": "RoadRoughness", "defaultValue": 0.9}),
        ]
        for key, kind, x, y, values in definitions:
            report["step"] = "create/configure " + key
            node = add_node(kind, x, y)
            nodes[key] = node
            report["new_nodes"][key] = node
            require(object_class(node)["refPath"] == "/Script/Engine." + kind, "New node class differs")
            if values:
                configure(node, values)
        require(inputs(nodes["xy"]) in ([""], ["None"]), "Anonymous mask input changed")
        for source, output, destination, pin in (("world", "", "xy", ""), ("xy", "", "uv", "A"),
                ("scale", "", "uv", "B"), ("uv", "", "diffuse", "UVs"),
                ("diffuse", "RGB", "tinted", "A"), ("tint", "RGB", "tinted", "B")):
            report["step"] = "connect " + source + " to " + destination
            connect(nodes[source], output, nodes[destination], pin)
        for key, prop in (("tinted", "MP_BaseColor"), ("opacity", "MP_Opacity"), ("roughness", "MP_Roughness")):
            report["step"] = "connect output " + prop
            connect_output(nodes[key], prop)
            require(output_input(prop)["expression"]["refPath"] == nodes[key]["refPath"], "Material output differs")
        report["step"] = "compile"
        recompile()  # Void wrapper raises on the underlying shader error array.
        found = expressions()
        require(len(found) == 9 and {r["refPath"] for r in found} == {r["refPath"] for r in nodes.values()}, "Nine-node identity check failed")
        report["graph_readback"] = {key: graph_inputs(nodes[key]) for key in ("xy", "uv", "diffuse", "tinted")}
        require(current_level() == TARGET and pie_running() is False, "Editor context changed")
        report.update(status="exact_partial_material_resumed_unsaved_compiled", success=True, compile_passed=True, step="complete")
        return report
    except Exception as error:
        report.update(status="partial_resume_failure_preserved_no_automatic_retry", error=str(error))
        return report
