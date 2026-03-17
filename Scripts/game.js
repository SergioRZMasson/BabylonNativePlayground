var engine = new BABYLON.NativeEngine();
var currentScene = null;

engine.getRenderingCanvas = function () {
    return window;
};

engine.getInputElement = function () {
    return 0;
};

// Start with a default scene
function createDefaultScene() {
    var scene = new BABYLON.Scene(engine);
    scene.createDefaultCamera(true, true, true);
    var camera = scene.activeCamera;
    camera.setTarget(BABYLON.Vector3.Zero());
    camera.position = new BABYLON.Vector3(0, 5, -10);

    var light = new BABYLON.HemisphericLight("light", new BABYLON.Vector3(0, 1, 0), scene);
    light.intensity = 0.7;

    var sphere = BABYLON.MeshBuilder.CreateSphere("sphere", { diameter: 2, segments: 32 }, scene);
    sphere.position.y = 1;

    BABYLON.MeshBuilder.CreateGround("ground", { width: 6, height: 6 }, scene);
    return scene;
}

function setCurrentScene(scene) {
    if (currentScene) {
        currentScene.dispose();
        currentScene = null;
        engine.releaseEffects();
    }
    currentScene = scene;
}

// Shared logic: eval playground code and set the resulting scene
function RunPlaygroundCode(code) {
    if (!code) return;

    var pgRoot = "https://playground.babylonjs.com";

    try {
        // Fix asset URLs to point to the playground CDN
        code = code
            .replace(/"\/textures\//g, '"' + pgRoot + "/textures/")
            .replace(/"textures\//g, '"' + pgRoot + "/textures/")
            .replace(/\/scenes\//g, pgRoot + "/scenes/")
            .replace(/"scenes\//g, '"' + pgRoot + "/scenes/")
            .replace(/"\.\.\/\.\.https/g, '"https')
            .replace("http://", "https://");

        var canvas = window;
        var newScene = eval(code + "\r\ncreateScene(engine)");

        if (newScene && newScene.then) {
            newScene.then(function (scene) {
                setCurrentScene(scene);
                console.log("Playground code executed successfully.");
            }).catch(function (e) {
                console.error("createScene promise rejected:");
                console.error(e);
            });
        } else if (newScene) {
            setCurrentScene(newScene);
            console.log("Playground code executed successfully.");
        } else {
            console.error("createScene did not return a scene.");
        }
    } catch (e) {
        console.error("Error evaluating playground code:");
        console.error(e);
    }
}

// Global function called from C++ via runtime->Dispatch
function LoadPlayground(hash) {
    if (!hash) return;

    // Normalize hash: ensure it starts with '#' and has a version
    if (hash[0] !== "#") {
        hash = "#" + hash;
    }
    if (hash.indexOf("#", 1) === -1) {
        hash += "#0";
    }

    var snippetUrl = "https://snippet.babylonjs.com";

    var retryTime = 500;
    var maxRetry = 5;
    var retry = 0;

    var onError = function (e) {
        if (e) console.error(e);
        retry++;
        if (retry < maxRetry) {
            setTimeout(function () { loadPG(); }, retryTime);
        } else {
            console.error("Failed to load playground after " + maxRetry + " retries.");
        }
    };

    var loadPG = function () {
        var xmlHttp = new XMLHttpRequest();
        xmlHttp.addEventListener("readystatechange", function () {
            if (xmlHttp.readyState === 4) {
                try {
                    xmlHttp.onreadystatechange = null;
                    var snippet = JSON.parse(xmlHttp.responseText);
                    var code = JSON.parse(snippet.jsonPayload).code.toString();

                    // Handle v2 manifest format
                    try {
                        var manifestPayload = JSON.parse(code);
                        if (manifestPayload.v === 2) {
                            code = manifestPayload.files[manifestPayload.entry]
                                .replace(/export +default +/g, "")
                                .replace(/export +/g, "");
                        }
                    } catch (e) {
                        // Not a manifest, proceed as usual
                    }

                    RunPlaygroundCode(code);
                    console.log("Playground loaded: " + hash);
                } catch (e) {
                    console.error("Error loading playground:");
                    onError(e);
                }
            }
        }, false);
        xmlHttp.onerror = function () {
            console.error("Network error loading playground.");
            onError();
        };
        xmlHttp.open("GET", snippetUrl + hash.replace(/#/g, "/"));
        xmlHttp.send();
    };

    loadPG();
}

// ---------------------------------------------------------------------------
// Scene data serialization for the C++ scene inspector
// ---------------------------------------------------------------------------
function serializeSceneData(scene) {
    if (!scene) return null;

    var cameras = scene.cameras || [];
    var transformNodes = scene.transformNodes || [];
    var meshes = scene.meshes || [];
    var materials = scene.materials || [];
    var textures = scene.textures || [];
    var lights = scene.lights || [];

    var buffer = new ArrayBuffer(512 * 1024);
    var view = new DataView(buffer);
    var offset = 0;

    function writeU8(v)  { view.setUint8(offset, v);                 offset += 1; }
    function writeU16(v) { view.setUint16(offset, v, true);           offset += 2; }
    function writeU32(v) { view.setUint32(offset, v >>> 0, true);     offset += 4; }
    function writeI32(v) { view.setInt32(offset, v | 0, true);        offset += 4; }
    function writeF32(v) { view.setFloat32(offset, v || 0, true);     offset += 4; }

    function writeString(s) {
        s = (s != null) ? String(s) : "";
        var len = s.length > 65535 ? 65535 : s.length;
        writeU16(len);
        for (var i = 0; i < len; i++) writeU8(s.charCodeAt(i) & 0xFF);
    }

    function writeVec3(v) {
        if (v) { writeF32(v.x); writeF32(v.y); writeF32(v.z); }
        else   { writeF32(0);   writeF32(0);   writeF32(0);   }
    }

    function writeVec4(v) {
        if (v) { writeF32(v.x); writeF32(v.y); writeF32(v.z); writeF32(v.w); }
        else   { writeF32(0);   writeF32(0);   writeF32(0);   writeF32(1);   }
    }

    function writeColor3(c) {
        if (c) { writeF32(c.r); writeF32(c.g); writeF32(c.b); }
        else   { writeF32(0);   writeF32(0);   writeF32(0);   }
    }

    // --- Header ---
    writeU32(0x42534E44); // magic "BSND"
    writeU32(1);          // version
    writeU32(cameras.length);
    writeU32(transformNodes.length);
    writeU32(meshes.length);
    writeU32(materials.length);
    writeU32(textures.length);
    writeU32(lights.length);

    // --- Cameras ---
    for (var ci = 0; ci < cameras.length; ci++) {
        var cam = cameras[ci];
        writeString(cam.name);

        var camType = 3;
        if (cam.getClassName) {
            var cn = cam.getClassName();
            if (cn === "FreeCamera" || cn === "UniversalCamera") camType = 0;
            else if (cn === "ArcRotateCamera") camType = 1;
            else if (cn === "FollowCamera") camType = 2;
        }
        writeU8(camType);
        writeVec3(cam.position);

        var camTarget = cam.target;
        if (!camTarget && cam.getTarget) {
            try { camTarget = cam.getTarget(); } catch (e) { camTarget = null; }
        }
        writeVec3(camTarget);
        writeF32(cam.fov);
        writeF32(cam.minZ);
        writeF32(cam.maxZ);
        writeU8(cam === scene.activeCamera ? 1 : 0);
    }

    // --- Transform Nodes ---
    for (var ni = 0; ni < transformNodes.length; ni++) {
        var node = transformNodes[ni];
        writeString(node.name);
        writeVec3(node.position);
        if (node.rotationQuaternion) {
            writeU8(1);
            writeVec4(node.rotationQuaternion);
        } else {
            writeU8(0);
            writeVec3(node.rotation);
        }
        writeVec3(node.scaling);
        var nodeEnabled = true;
        try { nodeEnabled = node.isEnabled(); } catch (e) {}
        writeU8(nodeEnabled ? 1 : 0);
        writeString(node.getClassName ? node.getClassName() : "TransformNode");
    }

    // --- Meshes ---
    for (var mi = 0; mi < meshes.length; mi++) {
        var mesh = meshes[mi];
        writeString(mesh.name);
        writeU32(mesh.getTotalVertices ? mesh.getTotalVertices() : 0);
        writeU32(mesh.getTotalIndices ? mesh.getTotalIndices() : 0);

        var hasVD = mesh.isVerticesDataPresent
            ? mesh.isVerticesDataPresent.bind(mesh)
            : function () { return false; };
        writeU8(hasVD(BABYLON.VertexBuffer.PositionKind) ? 1 : 0);
        writeU8(hasVD(BABYLON.VertexBuffer.NormalKind)   ? 1 : 0);
        writeU8(hasVD(BABYLON.VertexBuffer.TangentKind)  ? 1 : 0);
        writeU8(hasVD(BABYLON.VertexBuffer.UVKind)       ? 1 : 0);
        writeU8(hasVD(BABYLON.VertexBuffer.UV2Kind)      ? 1 : 0);
        writeU8(hasVD(BABYLON.VertexBuffer.ColorKind)    ? 1 : 0);

        writeU8(mesh.isVisible ? 1 : 0);
        var meshEnabled = true;
        try { meshEnabled = mesh.isEnabled(); } catch (e) {}
        writeU8(meshEnabled ? 1 : 0);

        writeVec3(mesh.position);
        if (mesh.rotationQuaternion) {
            writeU8(1);
            writeVec4(mesh.rotationQuaternion);
        } else {
            writeU8(0);
            writeVec3(mesh.rotation);
        }
        writeVec3(mesh.scaling);

        // Bounding box
        try {
            var bb = mesh.getBoundingInfo ? mesh.getBoundingInfo() : null;
            if (bb && bb.boundingBox) {
                writeU8(1);
                writeVec3(bb.boundingBox.minimumWorld);
                writeVec3(bb.boundingBox.maximumWorld);
            } else {
                writeU8(0);
                writeF32(0); writeF32(0); writeF32(0);
                writeF32(0); writeF32(0); writeF32(0);
            }
        } catch (e) {
            writeU8(0);
            writeF32(0); writeF32(0); writeF32(0);
            writeF32(0); writeF32(0); writeF32(0);
        }

        // Material index
        var matIdx = -1;
        if (mesh.material) {
            for (var j = 0; j < materials.length; j++) {
                if (materials[j] === mesh.material) { matIdx = j; break; }
            }
        }
        writeI32(matIdx);
        writeString(mesh.getClassName ? mesh.getClassName() : "Mesh");
    }

    // --- Materials ---
    var textureSlotNames = [
        "diffuseTexture", "specularTexture", "emissiveTexture", "ambientTexture",
        "bumpTexture", "opacityTexture", "reflectionTexture", "lightmapTexture",
        "albedoTexture", "metallicTexture", "reflectivityTexture", "microSurfaceTexture"
    ];

    for (var mti = 0; mti < materials.length; mti++) {
        var mat = materials[mti];
        writeString(mat.name);
        writeString(mat.getClassName ? mat.getClassName() : "Material");
        writeF32(mat.alpha !== undefined ? mat.alpha : 1.0);
        writeU8(mat.backFaceCulling ? 1 : 0);
        writeU8(mat.wireframe ? 1 : 0);

        writeColor3(mat.diffuseColor || mat.albedoColor);
        writeColor3(mat.specularColor || mat.reflectivityColor);
        writeColor3(mat.emissiveColor);
        writeColor3(mat.ambientColor);

        var slots = [];
        for (var si = 0; si < textureSlotNames.length; si++) {
            var slotName = textureSlotNames[si];
            var slotTex = mat[slotName];
            if (slotTex && slotTex.name) {
                slots.push({ slot: slotName, name: slotTex.name });
            }
        }
        writeU8(slots.length > 255 ? 255 : slots.length);
        for (var si = 0; si < slots.length && si < 255; si++) {
            writeString(slots[si].slot);
            writeString(slots[si].name);
        }
    }

    // --- Textures ---
    for (var ti = 0; ti < textures.length; ti++) {
        var tex = textures[ti];
        writeString(tex.name);
        writeString(tex.url || "");

        var sz = null;
        try { if (tex.getSize) sz = tex.getSize(); } catch (e) {}
        writeI32(sz && sz.width  ? sz.width  : -1);
        writeI32(sz && sz.height ? sz.height : -1);

        writeU8(tex.hasAlpha ? 1 : 0);
        writeU8(tex.isCube   ? 1 : 0);
        writeString(tex.getClassName ? tex.getClassName() : "BaseTexture");
    }

    // --- Lights ---
    for (var li = 0; li < lights.length; li++) {
        var light = lights[li];
        writeString(light.name);

        var lightType = 3;
        if (light.getClassName) {
            var lcn = light.getClassName();
            if (lcn === "PointLight")       lightType = 0;
            else if (lcn === "DirectionalLight") lightType = 1;
            else if (lcn === "SpotLight")        lightType = 2;
            else if (lcn === "HemisphericLight") lightType = 3;
        }
        writeU8(lightType);
        writeVec3(light.position);
        writeVec3(light.direction);
        writeF32(light.intensity);
        writeColor3(light.diffuse);
        writeColor3(light.specular);

        var lightEnabled = true;
        try { lightEnabled = light.isEnabled(); } catch (e) {}
        writeU8(lightEnabled ? 1 : 0);
        writeF32(light.range !== undefined ? light.range : 0);
    }

    return buffer.slice(0, offset);
}

// Initialize with default scene and start render loop
currentScene = createDefaultScene();

var _serializeCounter = 0;
var _SERIALIZE_INTERVAL = 30; // every ~30 frames

engine.runRenderLoop(function () {
    if (currentScene) {
        currentScene.render();

        _serializeCounter++;
        if (_serializeCounter >= _SERIALIZE_INTERVAL) {
            _serializeCounter = 0;
            try {
                var buf = serializeSceneData(currentScene);
                if (buf && typeof _nativeSetSceneData === "function") {
                    _nativeSetSceneData(buf);
                }
            } catch (e) {
                // silently ignore serialization errors
            }
        }
    }
});
