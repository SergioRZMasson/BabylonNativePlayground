// ===========================================================================
// Agent Bridge - WebSocket client for remote scene manipulation
// ===========================================================================
// Connects to a Python CLI WebSocket server and allows an external agent
// to inspect and manipulate the current Babylon.js scene via JSON commands.
//
// Globals used from playground.js:
//   currentScene, LoadPlayground, RunPlaygroundCode, _findEntityByUniqueId
// ===========================================================================

var _agentWs = null;
var _agentReconnectTimer = null;
var _agentConnected = false;
var _agentWsUrl = (typeof __nativeAgentWsUrl !== 'undefined') ? __nativeAgentWsUrl : "ws://127.0.0.1:8765";
var _agentReconnectInterval = 3000;

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------
function _agentConnect() {
    if (_agentConnected) return;

    try {
        _agentWs = new WebSocket(_agentWsUrl);

        _agentWs.onopen = function () {
            _agentConnected = true;
            console.log("[AgentBridge] Connected to agent server at " + _agentWsUrl);
            _agentSend({
                id: "handshake",
                command: "handshake",
                params: { type: "renderer", version: "1.0" }
            });
        };

        _agentWs.onmessage = function (evt) {
            try {
                var request = JSON.parse(evt.data);
                _agentHandleCommand(request);
            } catch (e) {
                console.error("[AgentBridge] Error handling message: " + e);
                _agentSend({
                    id: request ? request.id : null,
                    success: false,
                    error: "Parse error: " + String(e)
                });
            }
        };

        _agentWs.onclose = function () {
            _agentConnected = false;
            _agentWs = null;
            _agentScheduleReconnect();
        };

        _agentWs.onerror = function () {
            // onclose will fire after this
        };
    } catch (e) {
        _agentConnected = false;
        _agentWs = null;
        _agentScheduleReconnect();
    }
}

function _agentScheduleReconnect() {
    if (_agentReconnectTimer) return;
    _agentReconnectTimer = setTimeout(function () {
        _agentReconnectTimer = null;
        _agentConnect();
    }, _agentReconnectInterval);
}

function _agentSend(obj) {
    if (_agentWs && _agentConnected) {
        try {
            _agentWs.send(JSON.stringify(obj));
        } catch (e) {
            console.error("[AgentBridge] Send error: " + e);
        }
    }
}

// ---------------------------------------------------------------------------
// Command dispatcher
// ---------------------------------------------------------------------------
function _agentHandleCommand(request) {
    var id = request.id || null;
    var command = request.command;
    var params = request.params || {};

    try {
        switch (command) {
            case "query_scene":
                _agentSend({ id: id, success: true, data: _agentBuildSceneJSON() });
                break;
            case "set_transform":
                _agentCmdSetTransform(params);
                _agentSend({ id: id, success: true });
                break;
            case "set_material":
                _agentCmdSetMaterial(params);
                _agentSend({ id: id, success: true });
                break;
            case "set_light":
                _agentCmdSetLight(params);
                _agentSend({ id: id, success: true });
                break;
            case "set_visibility":
                _agentCmdSetVisibility(params);
                _agentSend({ id: id, success: true });
                break;
            case "set_enabled":
                _agentCmdSetEnabled(params);
                _agentSend({ id: id, success: true });
                break;
            case "eval":
                var evalResult = _agentCmdEval(params);
                _agentSend({ id: id, success: true, data: evalResult });
                break;
            case "load_playground":
                _agentCmdLoadPlayground(params);
                _agentSend({ id: id, success: true });
                break;
            case "run_code":
                _agentCmdRunCode(params);
                _agentSend({ id: id, success: true });
                break;
            case "load_glb":
                _agentCmdLoadGLB(id, params);
                // Response sent asynchronously after load completes
                break;
            case "create_primitive":
                var primResult = _agentCmdCreatePrimitive(params);
                _agentSend({ id: id, success: true, data: primResult });
                break;
            case "load_glb_data":
                _agentCmdLoadGLBData(id, params);
                // Response sent asynchronously after load completes
                break;
            case "load_sbsar":
                _agentCmdLoadSbsar(id, params);
                // Response sent asynchronously via C++ callback
                break;
            case "apply_substance":
                _agentCmdApplySubstance(id, params);
                // Response sent asynchronously via C++ callback
                break;
            case "ping":
                _agentSend({ id: id, success: true, data: "pong" });
                break;
            case "clear_scene":
                _agentCmdClearScene();
                _agentSend({ id: id, success: true });
                break;
            case "update_geometry":
                _agentCmdUpdateGeometry(params);
                _agentSend({ id: id, success: true });
                break;
            case "remove_node":
                var rmResult = _agentCmdRemoveNode(params);
                _agentSend({ id: id, success: true, data: rmResult });
                break;
            case "add_node":
                var addResult = _agentCmdCreatePrimitive(params);
                _agentSend({ id: id, success: true, data: addResult });
                break;
            case "handshake":
                _agentSend({ id: id, success: true });
                break;
            case "ensure_default_lights":
                _agentCmdEnsureDefaultLights();
                _agentSend({ id: id, success: true });
                break;
            case "set_world_matrix":
                _agentCmdSetWorldMatrix(params);
                _agentSend({ id: id, success: true });
                break;
            default:
                _agentSend({ id: id, success: false, error: "Unknown command: " + command });
        }
    } catch (e) {
        _agentSend({ id: id, success: false, error: String(e) });
    }
}

// ---------------------------------------------------------------------------
// Entity lookup helpers
// ---------------------------------------------------------------------------
function _agentFindNode(target) {
    if (!currentScene) return null;

    // If target is a number, treat as uniqueId
    if (typeof target === "number") {
        return _findEntityByUniqueId(currentScene, target);
    }

    // If target is a string starting with #, treat as uniqueId
    if (typeof target === "string" && target.charAt(0) === "#") {
        var uid = parseInt(target.substring(1), 10);
        if (!isNaN(uid)) return _findEntityByUniqueId(currentScene, uid);
    }

    // Otherwise search by name across all node types
    var lists = [
        currentScene.meshes,
        currentScene.transformNodes,
        currentScene.cameras,
        currentScene.lights
    ];
    for (var li = 0; li < lists.length; li++) {
        var list = lists[li] || [];
        for (var i = 0; i < list.length; i++) {
            if (list[i].name === target) return list[i];
        }
    }
    return null;
}

function _agentFindMaterial(target) {
    if (!currentScene) return null;

    if (typeof target === "number") {
        var mats = currentScene.materials || [];
        for (var i = 0; i < mats.length; i++) {
            if (mats[i].uniqueId === target) return mats[i];
        }
        return null;
    }

    if (typeof target === "string" && target.charAt(0) === "#") {
        var uid = parseInt(target.substring(1), 10);
        if (!isNaN(uid)) {
            var mats2 = currentScene.materials || [];
            for (var i2 = 0; i2 < mats2.length; i2++) {
                if (mats2[i2].uniqueId === uid) return mats2[i2];
            }
        }
        return null;
    }

    var materials = currentScene.materials || [];
    for (var mi = 0; mi < materials.length; mi++) {
        if (materials[mi].name === target) return materials[mi];
    }
    return null;
}

function _agentFindLight(target) {
    if (!currentScene) return null;

    if (typeof target === "number") {
        var lights = currentScene.lights || [];
        for (var i = 0; i < lights.length; i++) {
            if (lights[i].uniqueId === target) return lights[i];
        }
        return null;
    }

    if (typeof target === "string" && target.charAt(0) === "#") {
        var uid = parseInt(target.substring(1), 10);
        if (!isNaN(uid)) {
            var lights2 = currentScene.lights || [];
            for (var i2 = 0; i2 < lights2.length; i2++) {
                if (lights2[i2].uniqueId === uid) return lights2[i2];
            }
        }
        return null;
    }

    var allLights = currentScene.lights || [];
    for (var li = 0; li < allLights.length; li++) {
        if (allLights[li].name === target) return allLights[li];
    }
    return null;
}

// ---------------------------------------------------------------------------
// Command implementations
// ---------------------------------------------------------------------------

// set_transform: { target, property: "position"|"rotation"|"scaling", value: [x,y,z] }
function _agentCmdSetTransform(params) {
    var node = _agentFindNode(params.target);
    if (!node) throw new Error("Node not found: " + params.target);

    var prop = params.property || "position";
    var val = params.value;
    if (!val || val.length < 3) throw new Error("value must be [x, y, z]");

    var vec = new BABYLON.Vector3(val[0], val[1], val[2]);

    if (prop === "position") {
        node.position = vec;
    } else if (prop === "rotation") {
        // Accept degrees, convert to radians
        var rad = new BABYLON.Vector3(
            val[0] * Math.PI / 180,
            val[1] * Math.PI / 180,
            val[2] * Math.PI / 180
        );
        if (node.rotationQuaternion) {
            node.rotationQuaternion = BABYLON.Quaternion.FromEulerAngles(rad.x, rad.y, rad.z);
        } else {
            node.rotation = rad;
        }
    } else if (prop === "scaling") {
        node.scaling = vec;
    } else {
        throw new Error("Unknown transform property: " + prop);
    }
}

// set_material: { target, property, value }
// property: "diffuseColor"|"emissiveColor"|"specularColor"|"ambientColor"|
//           "albedoColor"|"reflectivityColor"|"alpha"|"metallic"|"roughness"|
//           "wireframe"|"backFaceCulling"|"environmentIntensity"|"alphaCutOff"|
//           "transparencyMode"|"zOffset"
function _agentCmdSetMaterial(params) {
    var mat = _agentFindMaterial(params.target);
    if (!mat) throw new Error("Material not found: " + params.target);

    var prop = params.property;
    var val = params.value;

    // Color properties
    var colorProps = [
        "diffuseColor", "emissiveColor", "specularColor", "ambientColor",
        "albedoColor", "reflectivityColor"
    ];
    for (var ci = 0; ci < colorProps.length; ci++) {
        if (prop === colorProps[ci]) {
            if (!val || val.length < 3) throw new Error("Color value must be [r, g, b] (0-1)");
            mat[prop] = new BABYLON.Color3(val[0], val[1], val[2]);
            return;
        }
    }

    // Float properties
    var floatProps = [
        "alpha", "metallic", "roughness", "environmentIntensity",
        "alphaCutOff", "zOffset", "zOffsetUnits"
    ];
    for (var fi = 0; fi < floatProps.length; fi++) {
        if (prop === floatProps[fi]) {
            mat[prop] = Number(val);
            return;
        }
    }

    // Bool properties
    var boolProps = ["wireframe", "backFaceCulling", "pointsCloud",
                     "disableDepthWrite", "forceDepthWrite"];
    for (var bi = 0; bi < boolProps.length; bi++) {
        if (prop === boolProps[bi]) {
            mat[prop] = !!val;
            return;
        }
    }

    // Int properties
    if (prop === "transparencyMode") {
        mat.transparencyMode = parseInt(val, 10);
        return;
    }

    throw new Error("Unknown material property: " + prop);
}

// set_light: { target, property, value }
function _agentCmdSetLight(params) {
    var light = _agentFindLight(params.target);
    if (!light) throw new Error("Light not found: " + params.target);

    var prop = params.property;
    var val = params.value;

    if (prop === "intensity") {
        light.intensity = Number(val);
    } else if (prop === "range") {
        light.range = Number(val);
    } else if (prop === "diffuse" || prop === "specular") {
        if (!val || val.length < 3) throw new Error("Color must be [r, g, b]");
        light[prop] = new BABYLON.Color3(val[0], val[1], val[2]);
    } else if (prop === "direction") {
        if (!val || val.length < 3) throw new Error("Direction must be [x, y, z]");
        light.direction = new BABYLON.Vector3(val[0], val[1], val[2]);
    } else if (prop === "position") {
        if (!val || val.length < 3) throw new Error("Position must be [x, y, z]");
        light.position = new BABYLON.Vector3(val[0], val[1], val[2]);
    } else if (prop === "angle") {
        light.angle = Number(val);
    } else if (prop === "exponent") {
        light.exponent = Number(val);
    } else if (prop === "enabled") {
        if (light.setEnabled) light.setEnabled(!!val);
    } else {
        throw new Error("Unknown light property: " + prop);
    }
}

// set_visibility: { target, visible: bool }
function _agentCmdSetVisibility(params) {
    var node = _agentFindNode(params.target);
    if (!node) throw new Error("Node not found: " + params.target);
    node.isVisible = !!params.visible;
}

// set_enabled: { target, enabled: bool }
function _agentCmdSetEnabled(params) {
    var node = _agentFindNode(params.target);
    if (!node) throw new Error("Node not found: " + params.target);
    if (node.setEnabled) node.setEnabled(!!params.enabled);
}

// eval: { code: "..." }
function _agentCmdEval(params) {
    if (!params.code) throw new Error("No code provided");
    var scene = currentScene;
    var result = eval(params.code);
    if (result === undefined) return null;
    if (typeof result === "object") {
        try {
            return JSON.parse(JSON.stringify(result));
        } catch (e) {
            return String(result);
        }
    }
    return result;
}

// load_playground: { hash: "#ABCDEF" }
function _agentCmdLoadPlayground(params) {
    if (!params.hash) throw new Error("No hash provided");
    LoadPlayground(params.hash);
}

// run_code: { code: "function createScene(engine) { ... }" }
function _agentCmdRunCode(params) {
    if (!params.code) throw new Error("No code provided");
    RunPlaygroundCode(params.code);
}

// load_glb: { path: "relative/path/to/file.glb" }
// Appends a GLB file into the current scene using BABYLON.SceneLoader.AppendAsync.
// The path is relative to the executable and gets prefixed with app:///
function _agentCmdLoadGLB(requestId, params) {
    if (!currentScene) throw new Error("No scene loaded");
    var filePath = params.path;
    if (!filePath) throw new Error("No path provided");

    // Build the root URL: app:///  + directory portion (with trailing slash)
    // and the filename separately for AppendAsync
    var normalizedPath = filePath.replace(/\\/g, "/");
    var lastSlash = normalizedPath.lastIndexOf("/");
    var rootUrl, fileName;
    if (lastSlash >= 0) {
        rootUrl = "app:///" + normalizedPath.substring(0, lastSlash + 1);
        fileName = normalizedPath.substring(lastSlash + 1);
    } else {
        rootUrl = "app:///";
        fileName = normalizedPath;
    }

    console.log("[AgentBridge] Loading GLB: " + rootUrl + fileName);

    BABYLON.SceneLoader.AppendAsync(rootUrl, fileName, currentScene, null, ".glb").then(function () {
        console.log("[AgentBridge] GLB loaded: " + fileName);
        _agentSend({ id: requestId, success: true, data: { loaded: fileName } });
    }).catch(function (err) {
        console.error("[AgentBridge] GLB load failed: " + err);
        _agentSend({ id: requestId, success: false, error: "GLB load failed: " + String(err) });
    });
}

// load_glb_data: { data: "<base64 string>", fileName: "model.glb" }
// Receives raw GLB bytes as base64, decodes, and appends into the current scene.
function _agentCmdLoadGLBData(requestId, params) {
    if (!currentScene) throw new Error("No scene loaded");
    var b64 = params.data;
    if (!b64) throw new Error("No data provided");
    var fileName = params.fileName || "model.glb";

    // Decode base64 to Uint8Array
    var raw = atob(b64);
    var bytes = new Uint8Array(raw.length);
    for (var i = 0; i < raw.length; i++) {
        bytes[i] = raw.charCodeAt(i);
    }

    console.log("[AgentBridge] Loading GLB data: " + fileName + " (" + bytes.length + " bytes)");

    BABYLON.SceneLoader.AppendAsync("", bytes, currentScene, null, ".glb").then(function () {
        console.log("[AgentBridge] GLB loaded: " + fileName);
        _agentSend({ id: requestId, success: true, data: { loaded: fileName } });
    }).catch(function (err) {
        console.error("[AgentBridge] GLB load failed: " + err);
        _agentSend({ id: requestId, success: false, error: "GLB load failed: " + String(err) });
    });
}

// create_primitive: { type: "sphere"|"box"|"plane"|"ground"|"cylinder"|"torus",
//                     name: "myMesh", options: { ... }, position: [x,y,z] }
function _agentCmdCreatePrimitive(params) {
    if (!currentScene) throw new Error("No scene loaded");
    var type = (params.type || "").toLowerCase();
    var name = params.name || (type + "_" + Date.now());
    var opts = params.options || {};
    var mesh;

    switch (type) {
        case "sphere":
            mesh = BABYLON.MeshBuilder.CreateSphere(name, {
                diameter: opts.diameter !== undefined ? opts.diameter : 1,
                segments: opts.segments !== undefined ? opts.segments : 32
            }, currentScene);
            break;
        case "box":
            mesh = BABYLON.MeshBuilder.CreateBox(name, {
                size: opts.size !== undefined ? opts.size : 1,
                width: opts.width,
                height: opts.height,
                depth: opts.depth
            }, currentScene);
            break;
        case "plane":
            mesh = BABYLON.MeshBuilder.CreatePlane(name, {
                size: opts.size !== undefined ? opts.size : 1,
                width: opts.width,
                height: opts.height
            }, currentScene);
            break;
        case "ground":
            mesh = BABYLON.MeshBuilder.CreateGround(name, {
                width: opts.width !== undefined ? opts.width : 6,
                height: opts.height !== undefined ? opts.height : 6,
                subdivisions: opts.subdivisions !== undefined ? opts.subdivisions : 2
            }, currentScene);
            break;
        case "cylinder":
            mesh = BABYLON.MeshBuilder.CreateCylinder(name, {
                height: opts.height !== undefined ? opts.height : 2,
                diameter: opts.diameter !== undefined ? opts.diameter : 1,
                diameterTop: opts.diameterTop,
                diameterBottom: opts.diameterBottom,
                tessellation: opts.tessellation !== undefined ? opts.tessellation : 24
            }, currentScene);
            break;
        case "torus":
            mesh = BABYLON.MeshBuilder.CreateTorus(name, {
                diameter: opts.diameter !== undefined ? opts.diameter : 1,
                thickness: opts.thickness !== undefined ? opts.thickness : 0.3,
                tessellation: opts.tessellation !== undefined ? opts.tessellation : 24
            }, currentScene);
            break;
        default:
            throw new Error("Unknown primitive type: " + type +
                ". Supported: sphere, box, plane, ground, cylinder, torus");
    }

    // Apply optional position
    if (params.position && params.position.length >= 3) {
        mesh.position = new BABYLON.Vector3(
            params.position[0], params.position[1], params.position[2]
        );
    }

    // Apply optional material color
    if (params.color && params.color.length >= 3) {
        var mat = new BABYLON.StandardMaterial(name + "_mat", currentScene);
        mat.diffuseColor = new BABYLON.Color3(
            params.color[0], params.color[1], params.color[2]
        );
        mesh.material = mat;
    }

    return {
        name: mesh.name,
        uniqueId: mesh.uniqueId,
        type: type,
        position: _agentVec3(mesh.position)
    };
}

// clear_scene: dispose all scene content and recreate a default camera
function _agentCmdClearScene() {
    if (!currentScene) throw new Error("No scene loaded");

    var i;
    var items;

    items = currentScene.animationGroups.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    items = currentScene.skeletons.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    items = currentScene.meshes.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    items = currentScene.transformNodes.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    items = currentScene.lights.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    items = currentScene.materials.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    items = currentScene.textures.slice();
    for (i = 0; i < items.length; i++) { try { items[i].dispose(); } catch (e) {} }

    currentScene.createDefaultCamera(true, true, true);
}

// update_geometry: { target, positions (required), normals, uvs, indices }
function _agentCmdUpdateGeometry(params) {
    if (!currentScene) throw new Error("No scene loaded");
    var mesh = _agentFindNode(params.target);
    if (!mesh) throw new Error("Node not found: " + params.target);
    if (!mesh.updateVerticesData) throw new Error("Target is not a mesh: " + params.target);

    if (!params.positions) throw new Error("positions array is required");
    mesh.updateVerticesData(BABYLON.VertexBuffer.PositionKind, new Float32Array(params.positions));

    if (params.normals && mesh.isVerticesDataPresent(BABYLON.VertexBuffer.NormalKind)) {
        mesh.updateVerticesData(BABYLON.VertexBuffer.NormalKind, new Float32Array(params.normals));
    }

    if (params.uvs && mesh.isVerticesDataPresent(BABYLON.VertexBuffer.UVKind)) {
        mesh.updateVerticesData(BABYLON.VertexBuffer.UVKind, new Float32Array(params.uvs));
    }

    if (params.indices) {
        mesh.setIndices(params.indices);
    }

    mesh.refreshBoundingInfo();
}

// remove_node: { target }
function _agentCmdRemoveNode(params) {
    if (!currentScene) throw new Error("No scene loaded");
    var node = _agentFindNode(params.target);
    if (!node) throw new Error("Node not found: " + params.target);
    var name = node.name;
    node.dispose();
    return { removed: name };
}

// ensure_default_lights: add a hemispheric light if scene has no lights
function _agentCmdEnsureDefaultLights() {
    if (!currentScene) throw new Error("No scene loaded");
    if (currentScene.lights && currentScene.lights.length > 0) return;

    var light = new BABYLON.HemisphericLight("_defaultLight", new BABYLON.Vector3(0, 1, 0), currentScene);
    light.intensity = 1.0;
}

// set_world_matrix: { target, matrix: [16 floats] }
// Receives a LOCAL transform matrix from the DCC tool.
// Decomposes into position/rotation/scale and applies to the Babylon node.
function _agentCmdSetWorldMatrix(params) {
    if (!currentScene) throw new Error("No scene loaded");
    var node = _agentFindNode(params.target);
    if (!node) throw new Error("Node not found: " + params.target);

    var m = params.matrix;
    if (!m || m.length < 16) throw new Error("matrix must be 16 floats");

    console.log("[SetWorldMatrix] Target: " + params.target);
    console.log("[SetWorldMatrix] Raw matrix from DCC: [" +
        m[0].toFixed(3) + ", " + m[1].toFixed(3) + ", " + m[2].toFixed(3) + ", " + m[3].toFixed(3) + ",  " +
        m[4].toFixed(3) + ", " + m[5].toFixed(3) + ", " + m[6].toFixed(3) + ", " + m[7].toFixed(3) + ",  " +
        m[8].toFixed(3) + ", " + m[9].toFixed(3) + ", " + m[10].toFixed(3) + ", " + m[11].toFixed(3) + ",  " +
        m[12].toFixed(3) + ", " + m[13].toFixed(3) + ", " + m[14].toFixed(3) + ", " + m[15].toFixed(3) + "]");

    // Log current node state before change
    console.log("[SetWorldMatrix] BEFORE - pos: " + _vecStr(node.position) +
        " rot: " + (node.rotationQuaternion ? _quatStr(node.rotationQuaternion) : _vecStr(node.rotation)) +
        " scale: " + _vecStr(node.scaling));
    console.log("[SetWorldMatrix] Parent: " + (node.parent ? node.parent.name : "none"));

    // Create matrix from the raw array
    var localMatrix = BABYLON.Matrix.FromArray(m);

    // Decompose into position, rotation (quaternion), scale
    var newScale = new BABYLON.Vector3();
    var newRotation = new BABYLON.Quaternion();
    var newPosition = new BABYLON.Vector3();

    var success = localMatrix.decompose(newScale, newRotation, newPosition);
    console.log("[SetWorldMatrix] Decompose success: " + success);
    console.log("[SetWorldMatrix] Decomposed pos: " + _vecStr(newPosition));
    console.log("[SetWorldMatrix] Decomposed rot (quat): " + _quatStr(newRotation));
    console.log("[SetWorldMatrix] Decomposed scale: " + _vecStr(newScale));

    // Apply decomposed values
    node.position.copyFrom(newPosition);
    node.scaling.copyFrom(newScale);
    if (!node.rotationQuaternion) {
        node.rotationQuaternion = new BABYLON.Quaternion();
    }
    node.rotationQuaternion.copyFrom(newRotation);

    console.log("[SetWorldMatrix] AFTER - pos: " + _vecStr(node.position) +
        " rot: " + _quatStr(node.rotationQuaternion) +
        " scale: " + _vecStr(node.scaling));
}

function _vecStr(v) {
    if (!v) return "(null)";
    return "(" + (v.x || 0).toFixed(3) + ", " + (v.y || 0).toFixed(3) + ", " + (v.z || 0).toFixed(3) + ")";
}

function _quatStr(q) {
    if (!q) return "(null)";
    return "(" + (q.x || 0).toFixed(3) + ", " + (q.y || 0).toFixed(3) + ", " + (q.z || 0).toFixed(3) + ", " + (q.w || 0).toFixed(3) + ")";
}

// ---------------------------------------------------------------------------
// Substance SDK integration (requires HAS_SUBSTANCE_SDK in C++ build)
// ---------------------------------------------------------------------------
var _substancePendingCallbacks = {};

// load_sbsar: { path: "E:\\path\\to\\file.sbsar" }
function _agentCmdLoadSbsar(requestId, params) {
    var path = params.path;
    if (!path) throw new Error("No path provided");

    if (typeof _nativeLoadSbsar !== "function") {
        throw new Error("Substance SDK not available (build without HAS_SUBSTANCE_SDK)");
    }

    _substancePendingCallbacks["load_" + requestId] = requestId;
    _nativeLoadSbsar(path, requestId);
}

// apply_substance: { target: "materialName" or "#uid" }
function _agentCmdApplySubstance(requestId, params) {
    var target = params.target;
    if (!target) throw new Error("No material target provided");

    if (typeof _nativeApplySubstance !== "function") {
        throw new Error("Substance SDK not available (build without HAS_SUBSTANCE_SDK)");
    }

    var mat = _agentFindMaterial(target);
    if (!mat) throw new Error("Material not found: " + target);

    _substancePendingCallbacks["apply_" + requestId] = requestId;
    _nativeApplySubstance(mat.uniqueId, requestId);
}

// Called from C++ when sbsar load completes
function _onSubstanceLoadComplete(requestId, success) {
    var agentReqId = _substancePendingCallbacks["load_" + requestId];
    if (agentReqId) {
        delete _substancePendingCallbacks["load_" + requestId];
        if (success) {
            _agentSend({ id: agentReqId, success: true, data: { loaded: true } });
        } else {
            _agentSend({ id: agentReqId, success: false, error: "Failed to load sbsar file" });
        }
    }
}

// Called from C++ when substance apply completes
function _onSubstanceApplyComplete(requestId) {
    var agentReqId = _substancePendingCallbacks["apply_" + requestId];
    if (agentReqId) {
        delete _substancePendingCallbacks["apply_" + requestId];
        _agentSend({ id: agentReqId, success: true, data: { applied: true } });
    }
}

// ---------------------------------------------------------------------------
// Scene JSON serialization
// ---------------------------------------------------------------------------
function _agentVec3(v) {
    if (!v) return { x: 0, y: 0, z: 0 };
    return { x: v.x || 0, y: v.y || 0, z: v.z || 0 };
}

function _agentColor3(c) {
    if (!c) return { r: 0, g: 0, b: 0 };
    return { r: c.r || 0, g: c.g || 0, b: c.b || 0 };
}

function _agentColor4(c) {
    if (!c) return { r: 0, g: 0, b: 0, a: 1 };
    return { r: c.r || 0, g: c.g || 0, b: c.b || 0, a: c.a !== undefined ? c.a : 1 };
}

function _agentSafeFloat(v) {
    return (v !== undefined && v !== null && !isNaN(v)) ? v : 0;
}

function _agentBuildSceneJSON() {
    if (!currentScene) return { error: "No scene loaded" };

    var scene = currentScene;
    var result = {};

    // Scene-level properties
    result.scene = {
        clearColor: _agentColor4(scene.clearColor),
        ambientColor: _agentColor3(scene.ambientColor),
        fogMode: scene.fogMode || 0,
        fogColor: _agentColor3(scene.fogColor),
        fogDensity: _agentSafeFloat(scene.fogDensity),
        fogStart: _agentSafeFloat(scene.fogStart),
        fogEnd: _agentSafeFloat(scene.fogEnd),
        forceWireframe: !!scene.forceWireframe,
        forcePointsCloud: !!scene.forcePointsCloud,
        autoClear: scene.autoClear !== undefined ? !!scene.autoClear : true,
        gravity: _agentVec3(scene.gravity)
    };

    // Image processing
    var ip = scene.imageProcessingConfiguration;
    if (ip) {
        result.scene.imageProcessing = {
            contrast: _agentSafeFloat(ip.contrast),
            exposure: _agentSafeFloat(ip.exposure),
            toneMappingEnabled: !!ip.toneMappingEnabled,
            toneMappingType: _agentSafeFloat(ip.toneMappingType)
        };
    }

    // Stats
    result.stats = {
        fps: engine.getFps ? engine.getFps() : 0,
        totalVertices: scene.getTotalVertices ? scene.getTotalVertices() : 0,
        activeIndices: scene.getActiveIndices ? scene.getActiveIndices() : 0,
        activeMeshes: 0,
        activeParticles: scene.getActiveParticles ? scene.getActiveParticles() : 0,
        activeBones: scene.getActiveBones ? scene.getActiveBones() : 0
    };
    try {
        result.stats.activeMeshes = scene.getActiveMeshes ? scene.getActiveMeshes().length : 0;
    } catch (e) {}

    // Cameras
    result.cameras = [];
    var cameras = scene.cameras || [];
    for (var ci = 0; ci < cameras.length; ci++) {
        var cam = cameras[ci];
        var camObj = {
            uniqueId: cam.uniqueId || 0,
            name: cam.name || "",
            type: cam.getClassName ? cam.getClassName() : "Camera",
            position: _agentVec3(cam.position),
            fov: _agentSafeFloat(cam.fov),
            minZ: _agentSafeFloat(cam.minZ),
            maxZ: _agentSafeFloat(cam.maxZ),
            isActive: cam === scene.activeCamera,
            inertia: _agentSafeFloat(cam.inertia),
            speed: _agentSafeFloat(cam.speed)
        };

        var camTarget = cam.target;
        if (!camTarget && cam.getTarget) {
            try { camTarget = cam.getTarget(); } catch (e) {}
        }
        camObj.target = _agentVec3(camTarget);

        // ArcRotate specific
        var cn = cam.getClassName ? cam.getClassName() : "";
        if (cn === "ArcRotateCamera") {
            camObj.alpha = _agentSafeFloat(cam.alpha);
            camObj.beta = _agentSafeFloat(cam.beta);
            camObj.radius = _agentSafeFloat(cam.radius);
        }

        result.cameras.push(camObj);
    }

    // Transform Nodes
    result.transformNodes = [];
    var tNodes = scene.transformNodes || [];
    for (var ni = 0; ni < tNodes.length; ni++) {
        var node = tNodes[ni];
        var nodeEnabled = true;
        try { nodeEnabled = node.isEnabled(); } catch (e) {}

        result.transformNodes.push({
            uniqueId: node.uniqueId || 0,
            name: node.name || "",
            type: node.getClassName ? node.getClassName() : "TransformNode",
            parentName: node.parent ? node.parent.name : null,
            position: _agentVec3(node.position),
            rotation: node.rotationQuaternion
                ? _agentQuatToEulerDeg(node.rotationQuaternion)
                : _agentRadToDeg3(node.rotation),
            scaling: _agentVec3(node.scaling),
            isEnabled: nodeEnabled
        });
    }

    // Meshes
    result.meshes = [];
    var meshes = scene.meshes || [];
    for (var mi = 0; mi < meshes.length; mi++) {
        var mesh = meshes[mi];
        var meshEnabled = true;
        try { meshEnabled = mesh.isEnabled(); } catch (e) {}

        var meshObj = {
            uniqueId: mesh.uniqueId || 0,
            name: mesh.name || "",
            type: mesh.getClassName ? mesh.getClassName() : "Mesh",
            parentName: mesh.parent ? mesh.parent.name : null,
            position: _agentVec3(mesh.position),
            rotation: mesh.rotationQuaternion
                ? _agentQuatToEulerDeg(mesh.rotationQuaternion)
                : _agentRadToDeg3(mesh.rotation),
            scaling: _agentVec3(mesh.scaling),
            isEnabled: meshEnabled,
            isVisible: !!mesh.isVisible,
            visibility: mesh.visibility !== undefined ? mesh.visibility : 1.0,
            receiveShadows: !!mesh.receiveShadows,
            vertices: mesh.getTotalVertices ? mesh.getTotalVertices() : 0,
            indices: mesh.getTotalIndices ? mesh.getTotalIndices() : 0,
            materialName: mesh.material ? mesh.material.name : null,
            materialId: mesh.material ? mesh.material.uniqueId : null
        };

        // Bounding box
        try {
            var bb = mesh.getBoundingInfo ? mesh.getBoundingInfo() : null;
            if (bb && bb.boundingBox) {
                meshObj.boundingBox = {
                    min: _agentVec3(bb.boundingBox.minimumWorld),
                    max: _agentVec3(bb.boundingBox.maximumWorld)
                };
            }
        } catch (e) {}

        // Morph targets
        if (mesh.morphTargetManager) {
            var numTargets = mesh.morphTargetManager.numTargets || 0;
            meshObj.morphTargets = [];
            for (var mti = 0; mti < numTargets; mti++) {
                var mt = mesh.morphTargetManager.getTarget(mti);
                meshObj.morphTargets.push({
                    name: mt ? (mt.name || "target_" + mti) : "target_" + mti,
                    influence: mt ? _agentSafeFloat(mt.influence) : 0
                });
            }
        }

        result.meshes.push(meshObj);
    }

    // Materials
    result.materials = [];
    var materials = scene.materials || [];
    var texSlotNames = [
        "diffuseTexture", "specularTexture", "emissiveTexture", "ambientTexture",
        "bumpTexture", "opacityTexture", "reflectionTexture", "lightmapTexture",
        "albedoTexture", "metallicTexture", "reflectivityTexture", "microSurfaceTexture"
    ];

    for (var mati = 0; mati < materials.length; mati++) {
        var mat = materials[mati];
        var matObj = {
            uniqueId: mat.uniqueId || 0,
            name: mat.name || "",
            type: mat.getClassName ? mat.getClassName() : "Material",
            alpha: mat.alpha !== undefined ? mat.alpha : 1.0,
            backFaceCulling: !!mat.backFaceCulling,
            wireframe: !!mat.wireframe,
            diffuseColor: _agentColor3(mat.diffuseColor || mat.albedoColor),
            specularColor: _agentColor3(mat.specularColor || mat.reflectivityColor),
            emissiveColor: _agentColor3(mat.emissiveColor),
            ambientColor: _agentColor3(mat.ambientColor)
        };

        // PBR
        var isPBR = mat.getClassName && (
            mat.getClassName() === "PBRMaterial" ||
            mat.getClassName() === "PBRMetallicRoughnessMaterial" ||
            mat.getClassName() === "PBRSpecularGlossinessMaterial"
        );
        if (isPBR) {
            matObj.isPBR = true;
            matObj.metallic = mat.metallic !== undefined ? mat.metallic : 0;
            matObj.roughness = mat.roughness !== undefined ? mat.roughness : 1;
            matObj.environmentIntensity = mat.environmentIntensity !== undefined ? mat.environmentIntensity : 1;
        }

        // Texture slots
        matObj.textures = {};
        for (var tsi = 0; tsi < texSlotNames.length; tsi++) {
            var slotName = texSlotNames[tsi];
            var slotTex = mat[slotName];
            if (slotTex && slotTex.name) {
                matObj.textures[slotName] = {
                    name: slotTex.name,
                    url: slotTex.url || ""
                };
            }
        }

        result.materials.push(matObj);
    }

    // Lights
    result.lights = [];
    var lights = scene.lights || [];
    for (var li = 0; li < lights.length; li++) {
        var light = lights[li];
        var lightEnabled = true;
        try { lightEnabled = light.isEnabled(); } catch (e) {}

        var lightObj = {
            uniqueId: light.uniqueId || 0,
            name: light.name || "",
            type: light.getClassName ? light.getClassName() : "Light",
            position: _agentVec3(light.position),
            direction: _agentVec3(light.direction),
            intensity: _agentSafeFloat(light.intensity),
            diffuse: _agentColor3(light.diffuse),
            specular: _agentColor3(light.specular),
            isEnabled: lightEnabled,
            range: _agentSafeFloat(light.range)
        };

        // Spot light specifics
        var lcn = light.getClassName ? light.getClassName() : "";
        if (lcn === "SpotLight") {
            lightObj.angle = _agentSafeFloat(light.angle);
            lightObj.innerAngle = _agentSafeFloat(light.innerAngle);
            lightObj.exponent = _agentSafeFloat(light.exponent);
        }

        // Shadow generator
        var sg = null;
        try { sg = light.getShadowGenerator ? light.getShadowGenerator() : null; } catch (e) {}
        if (sg) {
            lightObj.shadowGenerator = {
                bias: _agentSafeFloat(sg.bias),
                normalBias: _agentSafeFloat(sg.normalBias)
            };
        }

        result.lights.push(lightObj);
    }

    // Animation Groups
    result.animationGroups = [];
    var animGroups = scene.animationGroups || [];
    for (var ai = 0; ai < animGroups.length; ai++) {
        var ag = animGroups[ai];
        result.animationGroups.push({
            index: ai,
            name: ag.name || "AnimationGroup_" + ai,
            isPlaying: !!ag.isPlaying,
            speedRatio: ag.speedRatio !== undefined ? ag.speedRatio : 1,
            from: _agentSafeFloat(ag.from),
            to: _agentSafeFloat(ag.to),
            loopAnimation: !!ag.loopAnimation
        });
    }

    // Textures
    result.textures = [];
    var textures = scene.textures || [];
    for (var ti = 0; ti < textures.length; ti++) {
        var tex = textures[ti];
        var texObj = {
            uniqueId: tex.uniqueId || 0,
            name: tex.name || "",
            type: tex.getClassName ? tex.getClassName() : "BaseTexture",
            url: tex.url || "",
            hasAlpha: !!tex.hasAlpha,
            isCube: !!tex.isCube,
            level: tex.level !== undefined ? tex.level : 1
        };
        try {
            var sz = tex.getSize ? tex.getSize() : null;
            if (sz) {
                texObj.width = sz.width || 0;
                texObj.height = sz.height || 0;
            }
        } catch (e) {}
        result.textures.push(texObj);
    }

    // Skeletons
    result.skeletons = [];
    var skeletons = scene.skeletons || [];
    for (var ski = 0; ski < skeletons.length; ski++) {
        var skel = skeletons[ski];
        result.skeletons.push({
            index: ski,
            name: skel.name || "Skeleton_" + ski,
            boneCount: skel.bones ? skel.bones.length : 0
        });
    }

    return result;
}

// Convert rotation quaternion to euler angles in degrees
function _agentQuatToEulerDeg(q) {
    if (!q) return { x: 0, y: 0, z: 0 };
    var euler = q.toEulerAngles ? q.toEulerAngles() : { x: 0, y: 0, z: 0 };
    return {
        x: euler.x * 180 / Math.PI,
        y: euler.y * 180 / Math.PI,
        z: euler.z * 180 / Math.PI
    };
}

// Convert radians vec3 to degrees
function _agentRadToDeg3(v) {
    if (!v) return { x: 0, y: 0, z: 0 };
    return {
        x: (v.x || 0) * 180 / Math.PI,
        y: (v.y || 0) * 180 / Math.PI,
        z: (v.z || 0) * 180 / Math.PI
    };
}

// ---------------------------------------------------------------------------
// Auto-connect on load
// ---------------------------------------------------------------------------
_agentConnect();
console.log("[AgentBridge] Agent bridge initialized. Connecting to " + _agentWsUrl + "...");
