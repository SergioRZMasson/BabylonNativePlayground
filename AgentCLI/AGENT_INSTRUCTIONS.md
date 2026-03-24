# Babylon Native Agent CLI — Instructions for AI Agents

## Overview

The **Babylon Native Agent CLI** is a Python command-line tool that connects to a running
Babylon Native Playground application over WebSocket. It allows you to **inspect** the 3D
scene and **manipulate** objects, materials, lights, and cameras in real time. You can also
**load GLB models** from disk and **create primitives** (spheres, boxes, planes, etc.)
directly from the CLI.

## Architecture

```
┌──────────────────────┐   WebSocket (JSON)   ┌──────────────────────────┐
│   Python Agent CLI   │ ◄──────────────────► │  Babylon Native          │
│   (ws://127.0.0.1:   │                      │  Playground              │
│        8765)          │                      │  (agent_bridge.js)       │
│                       │                      │                          │
│  You type commands    │   ← responses ──     │  Executes on scene       │
│  here                 │   ── commands →       │  Returns JSON            │
└──────────────────────┘                       └──────────────────────────┘
```

The JS bridge (`Scripts/agent_bridge.js`) running inside Babylon Native automatically
attempts to connect to the Python WebSocket server every **3 seconds**. If the server
isn't running or the connection drops, it silently retries until a connection is
established.

## Setup

```bash
cd AgentCLI
pip install -r requirements.txt
```

## Starting

1. **Start the CLI first** (it runs the WebSocket server):
   ```bash
   python agent_cli.py
   # Or with custom host/port:
   python agent_cli.py --host 127.0.0.1 --port 8765
   ```

2. **Start Babylon Native Playground** — it will auto-connect within 3 seconds.
   You'll see `✅ Babylon Native connected!` when the connection is established.
   The order doesn't strictly matter — the JS bridge retries every 3s, so you
   can also start the Playground first and the CLI later.

## Commands Reference

### Scene Inspection

| Command | Description | Example |
|---------|-------------|---------|
| `scene` | Get full scene as JSON (all meshes, materials, lights, cameras, etc.) | `scene` |
| `meshes` | List all meshes with position, visibility, material | `meshes` |
| `materials` | List all materials with type, alpha, diffuse color | `materials` |
| `lights` | List all lights with type, intensity, color | `lights` |
| `cameras` | List all cameras with type, position, active status | `cameras` |
| `nodes` | List all transform nodes | `nodes` |
| `animations` | List all animation groups | `animations` |
| `textures` | List all textures | `textures` |

### Transform Manipulation

Rotations are in **degrees**. Entity lookup is by **name** (or `#<uniqueId>`).

| Command | Description | Example |
|---------|-------------|---------|
| `position <name> <x> <y> <z>` | Set node position | `position sphere 0 3 0` |
| `rotation <name> <x°> <y°> <z°>` | Set node rotation in degrees | `rotation sphere 0 45 0` |
| `scale <name> <x> <y> <z>` | Set node scaling | `scale sphere 2 2 2` |

### Material Manipulation

Color values are **0 to 1** (not 0–255).

| Command | Description | Example |
|---------|-------------|---------|
| `color <material> <r> <g> <b>` | Set diffuse/albedo color | `color "default material" 1 0 0` |
| `metallic <material> <value>` | Set PBR metallic (0–1) | `metallic myMat 0.8` |
| `roughness <material> <value>` | Set PBR roughness (0–1) | `roughness myMat 0.2` |
| `alpha <material> <value>` | Set material alpha (0–1) | `alpha myMat 0.5` |

### Node Properties

| Command | Description | Example |
|---------|-------------|---------|
| `visible <name> true\|false` | Set mesh visibility | `visible sphere false` |
| `enabled <name> true\|false` | Set node enabled state | `enabled sphere true` |

### Light Properties

| Command | Description | Example |
|---------|-------------|---------|
| `intensity <light> <value>` | Set light intensity | `intensity light 1.5` |

### Load GLB Models

Load `.glb` files into the current scene using `BABYLON.SceneLoader.AppendAsync`.

| Command | Description | Example |
|---------|-------------|---------|
| `loadglb <absolute_path>` | Load GLB from disk (reads file, sends as base64) | `loadglb E:\Models\Avocado.glb` |
| `loadglb <relative_path>` | Load GLB relative to executable (`app:///` prefix) | `loadglb Models/myModel.glb` |

**How it works:**
- **Absolute paths** (e.g. `E:\AI\Avocado.glb`): Python reads the file from disk,
  base64-encodes it, and sends it over WebSocket. The JS side decodes and appends to scene.
- **Relative paths** (e.g. `Models/myModel.glb`): The JS side loads using Babylon Native's
  `app:///` protocol, relative to the executable directory.

**Important:** GLB models loaded via `AppendAsync` are **added to the current scene**
(not replacing it). The GLB's root node is typically named `__root__`. Use
`scale __root__ 10 10 10` to resize imported models.

### Create Primitives

Create basic 3D shapes in the scene. All parameters after the name are optional.

| Command | Description | Example |
|---------|-------------|---------|
| `sphere [name] [diameter]` | Create sphere | `sphere mySphere 2` |
| `box [name] [size]` | Create box | `box myBox 1.5` |
| `plane [name] [size]` | Create plane | `plane myPlane 5` |
| `ground [name] [width] [height]` | Create ground | `ground floor 10 10` |
| `cylinder [name] [height] [diameter]` | Create cylinder | `cylinder col 3 0.5` |
| `torus [name] [diameter] [thickness]` | Create torus | `torus ring 2 0.4` |

### Advanced

| Command | Description | Example |
|---------|-------------|---------|
| `eval <js code>` | Execute arbitrary JavaScript | `eval scene.meshes.length` |
| `playground <hash>` | Load a Babylon.js playground | `playground #ABC123` |

### System

| Command | Description |
|---------|-------------|
| `ping` | Test connection |
| `status` | Show connection status |
| `help` | Show all commands |
| `quit` / `exit` | Exit the CLI |

## Entity Naming

- **By name**: `position sphere 0 5 0` — finds the first entity named "sphere"
- **By uniqueId**: `position #42 0 5 0` — finds the entity with uniqueId 42
- **Quoted names** (for names with spaces): `color "default material" 1 0 0`

## JSON Protocol (for programmatic use)

The CLI communicates with Babylon Native via JSON over WebSocket.

### Request format (CLI → Babylon):
```json
{
  "id": "1",
  "command": "query_scene",
  "params": {}
}
```

### Response format (Babylon → CLI):
```json
{
  "id": "1",
  "success": true,
  "data": { ... }
}
```

### Available commands:

| Command | Params | Returns |
|---------|--------|---------|
| `query_scene` | `{}` | Full scene JSON |
| `set_transform` | `{ "target": "name", "property": "position"\|"rotation"\|"scaling", "value": [x,y,z] }` | `{}` |
| `set_material` | `{ "target": "name", "property": "diffuseColor"\|"metallic"\|..., "value": ... }` | `{}` |
| `set_light` | `{ "target": "name", "property": "intensity"\|"diffuse"\|..., "value": ... }` | `{}` |
| `set_visibility` | `{ "target": "name", "visible": true\|false }` | `{}` |
| `set_enabled` | `{ "target": "name", "enabled": true\|false }` | `{}` |
| `load_glb` | `{ "path": "relative/path.glb" }` | `{ "loaded": "filename" }` |
| `load_glb_data` | `{ "data": "<base64>", "fileName": "model.glb" }` | `{ "loaded": "filename" }` |
| `create_primitive` | `{ "type": "sphere"\|"box"\|"plane"\|"ground"\|"cylinder"\|"torus", "name": "...", "options": {...}, "position": [x,y,z], "color": [r,g,b] }` | `{ "name", "uniqueId", "type", "position" }` |
| `eval` | `{ "code": "javascript code" }` | Eval result |
| `load_playground` | `{ "hash": "#ABCDEF" }` | `{}` |
| `run_code` | `{ "code": "function createScene(engine){...}" }` | `{}` |
| `ping` | `{}` | `"pong"` |

### `create_primitive` options by type:

| Type | Options |
|------|---------|
| `sphere` | `diameter` (default 1), `segments` (default 32) |
| `box` | `size` (default 1), or `width`, `height`, `depth` individually |
| `plane` | `size` (default 1), or `width`, `height` |
| `ground` | `width` (default 6), `height` (default 6), `subdivisions` (default 2) |
| `cylinder` | `height` (default 2), `diameter` (default 1), `diameterTop`, `diameterBottom`, `tessellation` |
| `torus` | `diameter` (default 1), `thickness` (default 0.3), `tessellation` |

All primitive commands also accept optional `position: [x, y, z]` and `color: [r, g, b]`
to place the mesh and assign a material on creation.

## Scene JSON Structure

The `query_scene` command returns JSON with this structure:

```json
{
  "scene": {
    "clearColor": { "r": 0.2, "g": 0.2, "b": 0.3, "a": 1.0 },
    "ambientColor": { "r": 0, "g": 0, "b": 0 },
    "fogMode": 0,
    "gravity": { "x": 0, "y": -9.81, "z": 0 }
  },
  "cameras": [
    {
      "uniqueId": 1,
      "name": "camera",
      "type": "ArcRotateCamera",
      "position": { "x": 0, "y": 5, "z": -10 },
      "target": { "x": 0, "y": 0, "z": 0 },
      "fov": 0.8,
      "isActive": true
    }
  ],
  "meshes": [
    {
      "uniqueId": 3,
      "name": "sphere",
      "type": "Mesh",
      "position": { "x": 0, "y": 1, "z": 0 },
      "rotation": { "x": 0, "y": 0, "z": 0 },
      "scaling": { "x": 1, "y": 1, "z": 1 },
      "isVisible": true,
      "isEnabled": true,
      "vertices": 1089,
      "indices": 6144,
      "materialName": "default material",
      "materialId": 5
    }
  ],
  "materials": [
    {
      "uniqueId": 5,
      "name": "default material",
      "type": "StandardMaterial",
      "alpha": 1.0,
      "diffuseColor": { "r": 0.8, "g": 0.8, "b": 0.8 },
      "isPBR": false,
      "textures": {}
    }
  ],
  "lights": [
    {
      "uniqueId": 2,
      "name": "light",
      "type": "HemisphericLight",
      "intensity": 0.7,
      "diffuse": { "r": 1, "g": 1, "b": 1 },
      "isEnabled": true
    }
  ],
  "transformNodes": [],
  "animationGroups": [],
  "textures": [],
  "skeletons": [],
  "stats": {
    "fps": 60,
    "totalVertices": 1113,
    "activeMeshes": 2
  }
}
```

## Typical Agent Workflow

1. **Inspect the scene** to understand what's in it:
   ```
   scene
   ```

2. **List specific entity types**:
   ```
   meshes
   materials
   ```

3. **Manipulate entities by name**:
   ```
   position sphere 0 5 0
   color "default material" 1 0 0
   scale ground 2 1 2
   ```

4. **Load a 3D model from disk**:
   ```
   loadglb E:\Models\Avocado.glb
   meshes
   scale __root__ 10 10 10
   ```

5. **Create primitives**:
   ```
   sphere mySphere 2
   position mySphere 3 1 0
   box myBox
   position myBox -3 0.5 0
   ```

6. **For complex operations, use eval**:
   ```
   eval var mat = new BABYLON.StandardMaterial("redMat", scene); mat.diffuseColor = new BABYLON.Color3(1,0,0); scene.getMeshByName("sphere").material = mat;
   ```

7. **Verify changes**:
   ```
   meshes
   materials
   ```

## Notes

- The JS bridge retries the WebSocket connection every **3 seconds** — start order
  doesn't matter (CLI first or Playground first both work).
- The `eval` command has access to `scene` (the current scene) and all `BABYLON` APIs.
- Material property `diffuseColor` works for StandardMaterial; for PBR materials, it
  sets `albedoColor` automatically.
- Only one Babylon Native instance can connect at a time.
- The default port is **8765**. Change with `--port` if needed.
- GLB files loaded with `loadglb` are **appended** to the current scene (not replacing it).
  The imported model's root node is usually named `__root__`.
