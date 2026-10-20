"""Call the official local Unreal MCP server with serialized JSON-RPC requests.

Native MCP tools are preferred when exposed. This client uses the same discovery
and dispatch protocol for a desktop session that has not reloaded its MCP tools.
Only loopback endpoints are accepted; tool arguments can be read from a file.
"""
from __future__ import annotations

import argparse
import json
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8001/mcp")
    parser.add_argument("--tool", default="list_toolsets")
    parser.add_argument("--arguments-file", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    endpoint = urllib.parse.urlparse(args.url)
    if endpoint.hostname not in {"127.0.0.1", "localhost", "::1"}:
        parser.error("Only a loopback Unreal MCP endpoint is supported.")
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, response_headers, newurl):
            raise RuntimeError("Redirects are not accepted by this local MCP client.")

    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())

    def request(method, params=None, identifier=None):
        payload = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            payload["params"] = params
        if identifier is not None:
            payload["id"] = identifier
        req = urllib.request.Request(args.url, json.dumps(payload).encode("utf-8"), headers, method="POST")
        with opener.open(req, timeout=args.timeout) as response:
            session = response.headers.get("Mcp-Session-Id")
            if session:
                headers["Mcp-Session-Id"] = session
            raw = response.read().decode("utf-8")
            if not raw.strip():
                return {}
            if "text/event-stream" in response.headers.get("Content-Type", ""):
                for line in raw.splitlines():
                    if line.startswith("data:"):
                        event = json.loads(line[5:].strip())
                        if event.get("id") == identifier:
                            return event
                raise RuntimeError("SSE response did not contain the requested result.")
            decoded = json.loads(raw)
            if not isinstance(decoded, dict) or decoded.get("jsonrpc") != "2.0":
                raise RuntimeError("Invalid JSON-RPC response.")
            if identifier is not None and decoded.get("id") != identifier:
                raise RuntimeError("Mismatched JSON-RPC response id.")
            return decoded

    try:
        initial = request("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                          "clientInfo": {"name": "Rogue10mCodexLocal", "version": "1.0"}}, 1)
        if "error" in initial:
            raise RuntimeError(initial["error"])
        headers["MCP-Protocol-Version"] = initial.get("result", {}).get("protocolVersion", "2024-11-05")
        request("notifications/initialized")
        arguments = json.loads(args.arguments_file.read_text(encoding="utf-8-sig")) if args.arguments_file else {}
        if not isinstance(arguments, dict):
            raise ValueError("Tool arguments must be a JSON object.")
        if args.tool == "__tools_list":
            result = request("tools/list", {}, 2)
        else:
            result = request("tools/call", {"name": args.tool, "arguments": arguments}, 2)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        value = result.get("result", {})
        if not isinstance(value, dict):
            raise RuntimeError("MCP result must be an object.")
        safe_content = []
        def without_image_data(entry):
            if isinstance(entry, dict):
                if "mimeType" in entry and "data" in entry:
                    return {"mimeType": entry["mimeType"], "encodedLength": len(entry["data"])}
                return {key: without_image_data(child) for key, child in entry.items()}
            if isinstance(entry, list):
                return [without_image_data(child) for child in entry]
            return entry
        for item in value.get("content", []):
            if item.get("type") == "image":
                safe_content.append({"type": "image", "mimeType": item.get("mimeType"),
                                     "encodedLength": len(item.get("data", ""))})
            elif item.get("type") == "text":
                body = item.get("text", "")
                try:
                    body = json.dumps(without_image_data(json.loads(body)), ensure_ascii=False)
                except (ValueError, TypeError):
                    pass
                safe_content.append({"type": "text", "text": body[:12000]})
            else:
                safe_content.append({"type": item.get("type")})
        summary = {"server": initial.get("result", {}).get("serverInfo"), "tool": args.tool,
                   "isError": value.get("isError", False), "content": safe_content}
        if "error" in result:
            summary["error"] = result["error"]
        if "tools" in value:
            summary["tools"] = value["tools"]
        print(json.dumps(summary, ensure_ascii=False, indent=2))
        return 1 if "error" in result or value.get("isError") else 0
    except (OSError, ValueError, RuntimeError, urllib.error.URLError) as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
