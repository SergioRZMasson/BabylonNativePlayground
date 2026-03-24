#!/usr/bin/env python3
"""
Babylon Native Agent CLI
========================
A Python CLI that starts a WebSocket server and waits for Babylon Native
Playground to connect. Once connected, you can send commands to inspect
and manipulate the current Babylon.js scene.

Usage:
    python agent_cli.py [--host HOST] [--port PORT]

Requires:
    pip install websockets
"""

import argparse
import asyncio
import base64
import json
import sys
import os
import textwrap
from typing import Optional, Dict, Any

try:
    import websockets
    from websockets.asyncio.server import serve
except ImportError:
    print("Error: 'websockets' package is required.")
    print("Install it with: pip install websockets")
    sys.exit(1)


class BabylonAgentCLI:
    """WebSocket server + interactive CLI for Babylon Native scene manipulation."""

    def __init__(self, host: str = "127.0.0.1", port: int = 8765):
        self.host = host
        self.port = port
        self.ws: Optional[websockets.ServerConnection] = None
        self._request_id = 0
        self._pending: Dict[str, asyncio.Future] = {}
        self._running = True

    # ------------------------------------------------------------------
    # WebSocket server
    # ------------------------------------------------------------------
    async def _handler(self, websocket: websockets.ServerConnection):
        """Handle a single WebSocket connection from Babylon Native."""
        if self.ws is not None:
            print("\n⚠  Another client tried to connect (only one allowed). Rejected.")
            await websocket.close()
            return

        self.ws = websocket
        print("\n✅ Babylon Native connected!")
        print("Type 'help' for available commands.\n")

        try:
            async for message in websocket:
                try:
                    data = json.loads(message)
                    rid = data.get("id")
                    if rid and rid in self._pending:
                        self._pending[rid].set_result(data)
                except json.JSONDecodeError:
                    pass
        except websockets.ConnectionClosed:
            pass
        finally:
            print("\n❌ Babylon Native disconnected.")
            self.ws = None
            # Cancel all pending requests
            for fut in self._pending.values():
                if not fut.done():
                    fut.cancel()
            self._pending.clear()

    # ------------------------------------------------------------------
    # Command sending
    # ------------------------------------------------------------------
    def _next_id(self) -> str:
        self._request_id += 1
        return str(self._request_id)

    async def send_command(
        self, command: str, params: Optional[Dict[str, Any]] = None, timeout: float = 10.0
    ) -> Optional[Dict[str, Any]]:
        """Send a JSON command to Babylon Native and wait for response."""
        if not self.ws:
            print("Error: No Babylon Native connection. Start the Playground app.")
            return None

        rid = self._next_id()
        loop = asyncio.get_event_loop()
        fut = loop.create_future()
        self._pending[rid] = fut

        msg = json.dumps({"id": rid, "command": command, "params": params or {}})
        try:
            await self.ws.send(msg)
        except websockets.ConnectionClosed:
            self._pending.pop(rid, None)
            print("Error: Connection lost while sending command.")
            return None

        try:
            result = await asyncio.wait_for(fut, timeout=timeout)
            return result
        except asyncio.TimeoutError:
            print(f"Error: Command '{command}' timed out after {timeout}s.")
            return None
        except asyncio.CancelledError:
            return None
        finally:
            self._pending.pop(rid, None)

    # ------------------------------------------------------------------
    # CLI loop
    # ------------------------------------------------------------------
    async def _read_input(self, prompt: str) -> str:
        """Read a line of input without blocking the event loop."""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, lambda: input(prompt))

    async def cli_loop(self):
        """Main interactive CLI loop."""
        while self._running:
            try:
                line = await self._read_input("babylon> ")
            except (EOFError, KeyboardInterrupt):
                print("\nExiting...")
                self._running = False
                break

            line = line.strip()
            if not line:
                continue

            try:
                await self._dispatch(line)
            except SystemExit:
                self._running = False
                break
            except Exception as e:
                print(f"Error: {e}")

    async def _dispatch(self, line: str):
        """Parse and dispatch a CLI command."""
        # Handle quoted arguments for names with spaces
        parts = line.split(None, 1)
        cmd = parts[0].lower()
        args = parts[1].strip() if len(parts) > 1 else ""

        handlers = {
            "help": self._cmd_help,
            "quit": self._cmd_quit,
            "exit": self._cmd_quit,
            "scene": self._cmd_scene,
            "meshes": self._cmd_meshes,
            "materials": self._cmd_materials,
            "lights": self._cmd_lights,
            "cameras": self._cmd_cameras,
            "nodes": self._cmd_nodes,
            "animations": self._cmd_animations,
            "textures": self._cmd_textures,
            "position": self._cmd_position,
            "rotation": self._cmd_rotation,
            "scale": self._cmd_scale,
            "color": self._cmd_color,
            "metallic": self._cmd_metallic,
            "roughness": self._cmd_roughness,
            "alpha": self._cmd_alpha,
            "visible": self._cmd_visible,
            "enabled": self._cmd_enabled,
            "intensity": self._cmd_intensity,
            "eval": self._cmd_eval,
            "playground": self._cmd_playground,
            "loadglb": self._cmd_load_glb,
            "load_glb": self._cmd_load_glb,
            "sphere": self._cmd_create_sphere,
            "box": self._cmd_create_box,
            "plane": self._cmd_create_plane,
            "ground": self._cmd_create_ground,
            "cylinder": self._cmd_create_cylinder,
            "torus": self._cmd_create_torus,
            "loadsbsar": self._cmd_load_sbsar,
            "load_sbsar": self._cmd_load_sbsar,
            "applysubstance": self._cmd_apply_substance,
            "apply_substance": self._cmd_apply_substance,
            "ping": self._cmd_ping,
            "status": self._cmd_status,
        }

        handler = handlers.get(cmd)
        if handler:
            await handler(args)
        else:
            print(f"Unknown command: '{cmd}'. Type 'help' for available commands.")

    # ------------------------------------------------------------------
    # Response formatting
    # ------------------------------------------------------------------
    @staticmethod
    def _print_json(data: Any, indent: int = 2):
        """Pretty-print JSON data."""
        print(json.dumps(data, indent=indent))

    @staticmethod
    def _fmt_vec3(v: dict) -> str:
        return f"({v.get('x', 0):.3f}, {v.get('y', 0):.3f}, {v.get('z', 0):.3f})"

    @staticmethod
    def _fmt_color(c: dict) -> str:
        return f"({c.get('r', 0):.3f}, {c.get('g', 0):.3f}, {c.get('b', 0):.3f})"

    def _check_response(self, resp: Optional[dict]) -> Optional[dict]:
        """Check response and return data or print error."""
        if resp is None:
            return None
        if not resp.get("success"):
            print(f"Error from Babylon: {resp.get('error', 'Unknown error')}")
            return None
        return resp.get("data")

    # ------------------------------------------------------------------
    # Command implementations
    # ------------------------------------------------------------------
    async def _cmd_help(self, _args: str):
        print(textwrap.dedent("""\
        ╔══════════════════════════════════════════════════════════════════╗
        ║                   Babylon Native Agent CLI                      ║
        ╠══════════════════════════════════════════════════════════════════╣
        ║ SCENE QUERY                                                     ║
        ║   scene              Full scene structure (JSON)                ║
        ║   meshes             List all meshes                            ║
        ║   materials          List all materials                         ║
        ║   lights             List all lights                            ║
        ║   cameras            List all cameras                           ║
        ║   nodes              List all transform nodes                   ║
        ║   animations         List all animation groups                  ║
        ║   textures           List all textures                          ║
        ║                                                                 ║
        ║ TRANSFORM (rotation in degrees)                                 ║
        ║   position <name> <x> <y> <z>                                  ║
        ║   rotation <name> <x> <y> <z>                                  ║
        ║   scale <name> <x> <y> <z>                                     ║
        ║                                                                 ║
        ║ MATERIAL                                                        ║
        ║   color <material> <r> <g> <b>        Set diffuse/albedo color ║
        ║   metallic <material> <value>         Set PBR metallic (0-1)   ║
        ║   roughness <material> <value>        Set PBR roughness (0-1)  ║
        ║   alpha <material> <value>            Set alpha (0-1)          ║
        ║                                                                 ║
        ║ NODE PROPERTIES                                                 ║
        ║   visible <name> true|false           Set mesh visibility      ║
        ║   enabled <name> true|false           Set node enabled         ║
        ║                                                                 ║
        ║ LIGHT                                                           ║
        ║   intensity <light> <value>           Set light intensity      ║
        ║                                                                 ║
        ║ CREATE PRIMITIVES                                               ║
        ║   sphere [name] [diameter]            Create sphere             ║
        ║   box [name] [size]                   Create box                ║
        ║   plane [name] [size]                 Create plane              ║
        ║   ground [name] [width] [height]      Create ground             ║
        ║   cylinder [name] [height] [diameter] Create cylinder           ║
        ║   torus [name] [diameter] [thickness] Create torus              ║
        ║                                                                 ║
        ║ LOAD ASSETS                                                     ║
        ║   loadglb <path>                      Append GLB into scene    ║
        ║     Path is relative to the executable (app:/// prefix)         ║
        ║                                                                 ║
        ║ SUBSTANCE (requires HAS_SUBSTANCE_SDK build)                    ║
        ║   loadsbsar <path>                    Load .sbsar material     ║
        ║   applysubstance <material>           Apply substance to mat   ║
        ║                                                                 ║
        ║ ADVANCED                                                        ║
        ║   eval <js code>                      Execute JS on scene      ║
        ║   playground <hash>                   Load playground by hash  ║
        ║                                                                 ║
        ║ SYSTEM                                                          ║
        ║   ping               Test connection                            ║
        ║   status             Show connection status                     ║
        ║   help               Show this help                             ║
        ║   quit / exit        Exit CLI                                   ║
        ╚══════════════════════════════════════════════════════════════════╝

        Entity names with spaces: use quotes, e.g. position "my mesh" 1 2 3
        Use #<id> for uniqueId lookup, e.g. position #42 1 2 3
        """))

    async def _cmd_quit(self, _args: str):
        print("Goodbye!")
        raise SystemExit

    async def _cmd_status(self, _args: str):
        if self.ws:
            print("✅ Connected to Babylon Native")
        else:
            print("❌ No connection. Start the Babylon Native Playground app.")

    async def _cmd_ping(self, _args: str):
        resp = await self.send_command("ping")
        data = self._check_response(resp)
        if data:
            print(f"✅ {data}")

    async def _cmd_scene(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if data:
            self._print_json(data)

    async def _cmd_meshes(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        meshes = data.get("meshes", [])
        if not meshes:
            print("No meshes in scene.")
            return
        print(f"{'Name':<30} {'Type':<20} {'Position':<30} {'Visible':<8} {'Material'}")
        print("─" * 120)
        for m in meshes:
            pos = self._fmt_vec3(m.get("position", {}))
            vis = "✓" if m.get("isVisible") else "✗"
            mat_name = m.get("materialName") or "—"
            print(f"{m.get('name', '?'):<30} {m.get('type', '?'):<20} {pos:<30} {vis:<8} {mat_name}")

    async def _cmd_materials(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        materials = data.get("materials", [])
        if not materials:
            print("No materials in scene.")
            return
        print(f"{'Name':<30} {'Type':<30} {'Alpha':<8} {'Diffuse Color'}")
        print("─" * 100)
        for m in materials:
            dc = self._fmt_color(m.get("diffuseColor", {}))
            print(f"{m.get('name', '?'):<30} {m.get('type', '?'):<30} {m.get('alpha', 1.0):<8.2f} {dc}")

    async def _cmd_lights(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        lights = data.get("lights", [])
        if not lights:
            print("No lights in scene.")
            return
        print(f"{'Name':<30} {'Type':<25} {'Intensity':<12} {'Enabled':<10} {'Diffuse'}")
        print("─" * 110)
        for l in lights:
            en = "✓" if l.get("isEnabled") else "✗"
            dc = self._fmt_color(l.get("diffuse", {}))
            print(f"{l.get('name', '?'):<30} {l.get('type', '?'):<25} {l.get('intensity', 0):<12.3f} {en:<10} {dc}")

    async def _cmd_cameras(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        cameras = data.get("cameras", [])
        if not cameras:
            print("No cameras in scene.")
            return
        print(f"{'Name':<30} {'Type':<25} {'Position':<30} {'Active'}")
        print("─" * 95)
        for c in cameras:
            pos = self._fmt_vec3(c.get("position", {}))
            act = "✓" if c.get("isActive") else "✗"
            print(f"{c.get('name', '?'):<30} {c.get('type', '?'):<25} {pos:<30} {act}")

    async def _cmd_nodes(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        nodes = data.get("transformNodes", [])
        if not nodes:
            print("No transform nodes in scene.")
            return
        print(f"{'Name':<30} {'Type':<25} {'Position':<30} {'Enabled'}")
        print("─" * 95)
        for n in nodes:
            pos = self._fmt_vec3(n.get("position", {}))
            en = "✓" if n.get("isEnabled") else "✗"
            print(f"{n.get('name', '?'):<30} {n.get('type', '?'):<25} {pos:<30} {en}")

    async def _cmd_animations(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        anims = data.get("animationGroups", [])
        if not anims:
            print("No animation groups in scene.")
            return
        print(f"{'#':<5} {'Name':<30} {'Playing':<10} {'Speed':<10} {'Loop':<8} {'Range'}")
        print("─" * 85)
        for a in anims:
            playing = "▶" if a.get("isPlaying") else "⏸"
            loop = "✓" if a.get("loopAnimation") else "✗"
            rng = f"{a.get('from', 0):.0f}–{a.get('to', 0):.0f}"
            print(f"{a.get('index', '?'):<5} {a.get('name', '?'):<30} {playing:<10} {a.get('speedRatio', 1):<10.2f} {loop:<8} {rng}")

    async def _cmd_textures(self, _args: str):
        resp = await self.send_command("query_scene")
        data = self._check_response(resp)
        if not data:
            return
        textures = data.get("textures", [])
        if not textures:
            print("No textures in scene.")
            return
        print(f"{'Name':<35} {'Type':<20} {'Size':<15} {'URL'}")
        print("─" * 110)
        for t in textures:
            w = t.get("width", "?")
            h = t.get("height", "?")
            size = f"{w}x{h}" if w and h else "—"
            url = t.get("url", "")
            if len(url) > 40:
                url = "..." + url[-37:]
            print(f"{t.get('name', '?'):<35} {t.get('type', '?'):<20} {size:<15} {url}")

    # -- Transform commands --

    def _parse_name_and_floats(self, args: str, count: int):
        """Parse '<name> <float1> <float2> ...' handling quoted names."""
        args = args.strip()
        if not args:
            raise ValueError(f"Expected: <name> {' '.join(f'<v{i}>' for i in range(count))}")

        # Handle quoted names
        if args.startswith('"'):
            end_quote = args.index('"', 1)
            name = args[1:end_quote]
            rest = args[end_quote + 1:].strip()
        elif args.startswith("'"):
            end_quote = args.index("'", 1)
            name = args[1:end_quote]
            rest = args[end_quote + 1:].strip()
        else:
            parts = args.split()
            name = parts[0]
            rest = " ".join(parts[1:])

        values = [float(x) for x in rest.split()]
        if len(values) != count:
            raise ValueError(f"Expected {count} values, got {len(values)}")
        return name, values

    def _parse_name_and_bool(self, args: str):
        """Parse '<name> true|false' handling quoted names."""
        args = args.strip()
        if not args:
            raise ValueError("Expected: <name> true|false")

        if args.startswith('"'):
            end_quote = args.index('"', 1)
            name = args[1:end_quote]
            rest = args[end_quote + 1:].strip()
        elif args.startswith("'"):
            end_quote = args.index("'", 1)
            name = args[1:end_quote]
            rest = args[end_quote + 1:].strip()
        else:
            parts = args.split()
            name = parts[0]
            rest = " ".join(parts[1:])

        val = rest.lower() in ("true", "1", "yes", "on")
        return name, val

    async def _cmd_position(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 3)
        except ValueError as e:
            print(f"Usage: position <name> <x> <y> <z>\n{e}")
            return
        resp = await self.send_command("set_transform", {
            "target": name, "property": "position", "value": vals
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.position = ({vals[0]}, {vals[1]}, {vals[2]})")

    async def _cmd_rotation(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 3)
        except ValueError as e:
            print(f"Usage: rotation <name> <x°> <y°> <z°>\n{e}")
            return
        resp = await self.send_command("set_transform", {
            "target": name, "property": "rotation", "value": vals
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.rotation = ({vals[0]}°, {vals[1]}°, {vals[2]}°)")

    async def _cmd_scale(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 3)
        except ValueError as e:
            print(f"Usage: scale <name> <x> <y> <z>\n{e}")
            return
        resp = await self.send_command("set_transform", {
            "target": name, "property": "scaling", "value": vals
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.scaling = ({vals[0]}, {vals[1]}, {vals[2]})")

    async def _cmd_color(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 3)
        except ValueError as e:
            print(f"Usage: color <material> <r> <g> <b>  (values 0-1)\n{e}")
            return
        resp = await self.send_command("set_material", {
            "target": name, "property": "diffuseColor", "value": vals
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.diffuseColor = ({vals[0]}, {vals[1]}, {vals[2]})")

    async def _cmd_metallic(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 1)
        except ValueError as e:
            print(f"Usage: metallic <material> <value>  (0-1)\n{e}")
            return
        resp = await self.send_command("set_material", {
            "target": name, "property": "metallic", "value": vals[0]
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.metallic = {vals[0]}")

    async def _cmd_roughness(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 1)
        except ValueError as e:
            print(f"Usage: roughness <material> <value>  (0-1)\n{e}")
            return
        resp = await self.send_command("set_material", {
            "target": name, "property": "roughness", "value": vals[0]
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.roughness = {vals[0]}")

    async def _cmd_alpha(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 1)
        except ValueError as e:
            print(f"Usage: alpha <material> <value>  (0-1)\n{e}")
            return
        resp = await self.send_command("set_material", {
            "target": name, "property": "alpha", "value": vals[0]
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.alpha = {vals[0]}")

    async def _cmd_visible(self, args: str):
        try:
            name, val = self._parse_name_and_bool(args)
        except ValueError as e:
            print(f"Usage: visible <name> true|false\n{e}")
            return
        resp = await self.send_command("set_visibility", {
            "target": name, "visible": val
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.isVisible = {val}")

    async def _cmd_enabled(self, args: str):
        try:
            name, val = self._parse_name_and_bool(args)
        except ValueError as e:
            print(f"Usage: enabled <name> true|false\n{e}")
            return
        resp = await self.send_command("set_enabled", {
            "target": name, "enabled": val
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.isEnabled = {val}")

    async def _cmd_intensity(self, args: str):
        try:
            name, vals = self._parse_name_and_floats(args, 1)
        except ValueError as e:
            print(f"Usage: intensity <light> <value>\n{e}")
            return
        resp = await self.send_command("set_light", {
            "target": name, "property": "intensity", "value": vals[0]
        })
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ {name}.intensity = {vals[0]}")

    async def _cmd_eval(self, args: str):
        if not args:
            print("Usage: eval <javascript code>")
            return
        resp = await self.send_command("eval", {"code": args})
        data = self._check_response(resp)
        if data is not None:
            if isinstance(data, (dict, list)):
                self._print_json(data)
            else:
                print(f"Result: {data}")
        elif resp and resp.get("success"):
            print("✅ Executed (no return value)")

    async def _cmd_playground(self, args: str):
        if not args:
            print("Usage: playground <hash>  (e.g. playground #ABC123)")
            return
        resp = await self.send_command("load_playground", {"hash": args.strip()})
        if self._check_response(resp) is not None or (resp and resp.get("success")):
            print(f"✅ Loading playground: {args.strip()}")

    # -- Load GLB --

    async def _cmd_load_glb(self, args: str):
        if not args:
            print("Usage: loadglb <path>")
            print("  Absolute path:  loadglb E:\\Models\\myModel.glb")
            print("  Relative to exe: loadglb Models/myModel.glb")
            return
        path = args.strip().strip('"').strip("'")

        # Check if it's an absolute path that exists on disk
        if os.path.isabs(path) or os.path.exists(path):
            abs_path = os.path.abspath(path)
            if not os.path.isfile(abs_path):
                print(f"Error: File not found: {abs_path}")
                return
            file_size = os.path.getsize(abs_path)
            file_name = os.path.basename(abs_path)
            print(f"Loading GLB: {abs_path} ({file_size:,} bytes) ...")
            with open(abs_path, "rb") as f:
                data_bytes = f.read()
            b64 = base64.b64encode(data_bytes).decode("ascii")
            resp = await self.send_command(
                "load_glb_data", {"data": b64, "fileName": file_name}, timeout=60.0
            )
        else:
            # Relative path — use app:/// protocol on the JS side
            print(f"Loading GLB: {path} (relative to executable) ...")
            resp = await self.send_command("load_glb", {"path": path}, timeout=30.0)

        data = self._check_response(resp)
        if data:
            print(f"✅ GLB loaded: {data.get('loaded', path)}")
        elif resp and resp.get("success"):
            print(f"✅ GLB loaded: {path}")

    # -- Create primitives --

    async def _create_primitive(self, prim_type: str, args: str, default_opts: dict):
        """Generic primitive creation helper."""
        parts = args.split() if args else []
        name = parts[0] if parts else None
        params = {"type": prim_type, "options": {}}
        if name:
            params["name"] = name

        # Parse type-specific optional numeric args
        if prim_type == "sphere" and len(parts) >= 2:
            params["options"]["diameter"] = float(parts[1])
        elif prim_type == "box" and len(parts) >= 2:
            params["options"]["size"] = float(parts[1])
        elif prim_type == "plane" and len(parts) >= 2:
            params["options"]["size"] = float(parts[1])
        elif prim_type == "ground":
            if len(parts) >= 2:
                params["options"]["width"] = float(parts[1])
            if len(parts) >= 3:
                params["options"]["height"] = float(parts[2])
        elif prim_type == "cylinder":
            if len(parts) >= 2:
                params["options"]["height"] = float(parts[1])
            if len(parts) >= 3:
                params["options"]["diameter"] = float(parts[2])
        elif prim_type == "torus":
            if len(parts) >= 2:
                params["options"]["diameter"] = float(parts[1])
            if len(parts) >= 3:
                params["options"]["thickness"] = float(parts[2])

        resp = await self.send_command("create_primitive", params)
        data = self._check_response(resp)
        if data:
            pos = data.get("position", {})
            print(f"✅ Created {prim_type} '{data.get('name')}' "
                  f"(id: {data.get('uniqueId')}) at "
                  f"({pos.get('x', 0):.1f}, {pos.get('y', 0):.1f}, {pos.get('z', 0):.1f})")

    async def _cmd_create_sphere(self, args: str):
        await self._create_primitive("sphere", args, {"diameter": 1})

    async def _cmd_create_box(self, args: str):
        await self._create_primitive("box", args, {"size": 1})

    async def _cmd_create_plane(self, args: str):
        await self._create_primitive("plane", args, {"size": 1})

    async def _cmd_create_ground(self, args: str):
        await self._create_primitive("ground", args, {"width": 6, "height": 6})

    async def _cmd_create_cylinder(self, args: str):
        await self._create_primitive("cylinder", args, {"height": 2, "diameter": 1})

    async def _cmd_create_torus(self, args: str):
        await self._create_primitive("torus", args, {"diameter": 1, "thickness": 0.3})

    # -- Substance commands --

    async def _cmd_load_sbsar(self, args: str):
        if not args:
            print("Usage: loadsbsar <path>")
            print("  Example: loadsbsar E:\\Materials\\Wood.sbsar")
            return
        path = args.strip().strip('"').strip("'")
        abs_path = os.path.abspath(path)
        if not os.path.isfile(abs_path):
            print(f"Error: File not found: {abs_path}")
            return
        print(f"Loading sbsar: {abs_path} ...")
        resp = await self.send_command("load_sbsar", {"path": abs_path}, timeout=60.0)
        data = self._check_response(resp)
        if data:
            print(f"✅ Substance material loaded: {os.path.basename(abs_path)}")
        elif resp and resp.get("success"):
            print(f"✅ Substance material loaded: {os.path.basename(abs_path)}")

    async def _cmd_apply_substance(self, args: str):
        if not args:
            print("Usage: applysubstance <material_name>")
            print("  Applies the most recently loaded substance to the named material.")
            print("  The target material must be a PBRMaterial.")
            print("  Example: applysubstance myPBRMaterial")
            return
        target = args.strip().strip('"').strip("'")
        print(f"Applying substance to material: {target} ...")
        resp = await self.send_command("apply_substance", {"target": target}, timeout=30.0)
        data = self._check_response(resp)
        if data:
            print(f"✅ Substance textures applied to '{target}'")
        elif resp and resp.get("success"):
            print(f"✅ Substance textures applied to '{target}'")

    # ------------------------------------------------------------------
    # Main entry point
    # ------------------------------------------------------------------
    async def run(self):
        """Start the WebSocket server and CLI loop."""
        async with serve(self._handler, self.host, self.port):
            print("╔══════════════════════════════════════════════════════╗")
            print("║         Babylon Native Agent CLI Server              ║")
            print("╠══════════════════════════════════════════════════════╣")
            print(f"║  WebSocket: ws://{self.host}:{self.port:<26}║")
            print("║  Waiting for Babylon Native to connect...            ║")
            print("║  Press Ctrl+C to exit.                               ║")
            print("╚══════════════════════════════════════════════════════╝")
            print()
            await self.cli_loop()


def main():
    parser = argparse.ArgumentParser(
        description="Babylon Native Agent CLI - Remote scene manipulation via WebSocket"
    )
    parser.add_argument(
        "--host", default="127.0.0.1", help="Host to bind WebSocket server (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=8765, help="Port for WebSocket server (default: 8765)"
    )
    args = parser.parse_args()

    cli = BabylonAgentCLI(host=args.host, port=args.port)

    try:
        asyncio.run(cli.run())
    except KeyboardInterrupt:
        print("\nExiting...")


if __name__ == "__main__":
    main()
