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
    _debugState = {};
}

// Shared logic: eval playground code and set the resulting scene
function RunPlaygroundCode(code) {
    if (!code) return;

    // Send the code to C++ so the editor displays it
    if (typeof _nativeSetPlaygroundCode === "function") {
        _nativeSetPlaygroundCode(code);
    }

    var pgRoot = "https://playground.babylonjs.com";

    try {
        code = code
            .replace(/"\/textures\//g, '"' + pgRoot + "/textures/")
            .replace(/"textures\//g, '"' + pgRoot + "/textures/")
            .replace(/\/scenes\//g, pgRoot + "/scenes/")
            .replace(/"scenes\//g, '"' + pgRoot + "/scenes/")
            .replace(/"\.\.\/\.\.https/g, '"https')
            .replace("http://", "https://");

        var canvas = window;

        // Support both createScene and delayCreateScene patterns
        var evalSuffix;
        if (code.indexOf("delayCreateScene") !== -1) {
            evalSuffix = "\r\ndelayCreateScene()";
        } else {
            evalSuffix = "\r\ncreateScene(engine)";
        }
        var newScene = eval(code + evalSuffix);

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

function LoadPlayground(hash) {
    if (!hash) return;
    if (hash[0] !== "#") hash = "#" + hash;
    if (hash.indexOf("#", 1) === -1) hash += "#0";

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
                    try {
                        var manifestPayload = JSON.parse(code);
                        if (manifestPayload.v === 2) {
                            code = manifestPayload.files[manifestPayload.entry]
                                .replace(/export +default +/g, "")
                                .replace(/export +/g, "");
                        }
                    } catch (e) {}
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

// ===========================================================================
// Scene data serialization v2 for the C++ inspector
// ===========================================================================
var _SCENE_BUFFER_SIZE = 2 * 1024 * 1024;

function serializeSceneData(scene) {
    if (!scene) return null;

    var cameras = scene.cameras || [];
    var transformNodes = scene.transformNodes || [];
    var meshes = scene.meshes || [];
    var materials = scene.materials || [];
    var textures = scene.textures || [];
    var lights = scene.lights || [];
    var animationGroups = scene.animationGroups || [];
    var skeletons = scene.skeletons || [];

    var buffer = new ArrayBuffer(_SCENE_BUFFER_SIZE);
    var view = new DataView(buffer);
    var offset = 0;

    function writeU8(v)  { view.setUint8(offset, v);             offset += 1; }
    function writeU16(v) { view.setUint16(offset, v, true);       offset += 2; }
    function writeU32(v) { view.setUint32(offset, v >>> 0, true); offset += 4; }
    function writeI32(v) { view.setInt32(offset, v | 0, true);    offset += 4; }
    function writeF32(v) { view.setFloat32(offset, +v || 0, true); offset += 4; }

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

    function writeColor4(c) {
        if (c) { writeF32(c.r); writeF32(c.g); writeF32(c.b); writeF32(c.a !== undefined ? c.a : 1); }
        else   { writeF32(0);   writeF32(0);   writeF32(0);   writeF32(1); }
    }

    function safeFloat(v) { return (v !== undefined && v !== null && !isNaN(v)) ? v : 0; }
    function safeBool(v) { return v ? 1 : 0; }

    function getParentId(entity) {
        if (entity.parent && entity.parent.uniqueId !== undefined) return entity.parent.uniqueId;
        return 0;
    }

    // --- Header ---
    writeU32(0x42534E44); // magic "BSND"
    writeU32(2);          // version
    writeU32(cameras.length);
    writeU32(transformNodes.length);
    writeU32(meshes.length);
    writeU32(materials.length);
    writeU32(textures.length);
    writeU32(lights.length);
    writeU32(animationGroups.length);
    writeU32(skeletons.length);

    // --- Scene properties ---
    writeColor4(scene.clearColor);
    writeColor3(scene.ambientColor);
    writeU8(scene.fogMode !== undefined ? scene.fogMode : 0);
    writeColor3(scene.fogColor);
    writeF32(safeFloat(scene.fogDensity));
    writeF32(safeFloat(scene.fogStart));
    writeF32(safeFloat(scene.fogEnd));
    writeU8(safeBool(scene.forceWireframe));
    writeU8(safeBool(scene.forcePointsCloud));
    writeU8(scene.autoClear !== undefined ? safeBool(scene.autoClear) : 1);
    writeVec3(scene.gravity);

    // Image processing
    var ip = scene.imageProcessingConfiguration;
    writeU8(ip ? 1 : 0);
    if (ip) {
        writeF32(safeFloat(ip.contrast));
        writeF32(safeFloat(ip.exposure));
        writeU8(safeBool(ip.toneMappingEnabled));
        writeU8(safeFloat(ip.toneMappingType));
        writeU8(safeBool(ip.vignetteEnabled));
    }

    // --- Stats ---
    var perf = scene.getPerformanceCounter ? scene.getPerformanceCounter() : null;
    writeF32(engine.getFps ? engine.getFps() : 0);
    var ic = scene.getInstrumentation ? scene.getInstrumentation() : null;

    writeU32(scene.getTotalVertices ? scene.getTotalVertices() : 0);
    writeU32(scene.getActiveIndices ? scene.getActiveIndices() : 0);
    writeU32(scene.getActiveMeshes ? scene.getActiveMeshes().length : 0);
    writeU32(scene.getActiveParticles ? scene.getActiveParticles() : 0);
    writeU32(scene.getActiveBones ? scene.getActiveBones() : 0);

    var drawCalls = 0;
    try { drawCalls = engine._drawCalls ? engine._drawCalls.current : 0; } catch (e) {}
    writeU32(drawCalls);

    // Frame step durations (in ms)
    var getCounter = function(c) {
        if (c && c.current !== undefined) return c.current;
        return 0;
    };

    if (ic) {
        var counters = ic._capturedCounters || {};
        writeF32(getCounter(ic._frameTimeCounter));
        writeF32(getCounter(ic._interFrameTimeCounter));
        writeF32(getCounter(ic._renderTimeCounter));
        writeF32(getCounter(ic._cameraRenderTimeCounter));
        writeF32(getCounter(ic._activeMeshesEvaluationTimeCounter));
        writeF32(getCounter(ic._renderTargetsRenderTimeCounter));
        writeF32(getCounter(ic._particlesRenderTimeCounter));
        writeF32(getCounter(ic._spritesRenderTimeCounter));
        writeF32(getCounter(ic._animationsTimeCounter));
        writeF32(getCounter(ic._physicsTimeCounter));
    } else {
        for (var si = 0; si < 10; si++) writeF32(0);
    }

    // Engine info
    writeString(BABYLON.Engine ? (BABYLON.Engine.Version || "") : "");

    var hw = engine.getHardwareScalingLevel ? engine.getHardwareScalingLevel() : 1;
    writeF32(hw);

    var rw = 0, rh = 0;
    try { rw = engine.getRenderWidth(); rh = engine.getRenderHeight(); } catch (e) {}
    writeI32(rw);
    writeI32(rh);

    // --- Cameras ---
    for (var ci = 0; ci < cameras.length; ci++) {
        var cam = cameras[ci];
        writeU32(cam.uniqueId || 0);
        writeU32(getParentId(cam));
        writeString(cam.name);
        writeString(cam.getClassName ? cam.getClassName() : "Camera");

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
        writeF32(safeFloat(cam.inertia));
        writeF32(safeFloat(cam.speed));
        writeU8(cam.mode !== undefined ? cam.mode : 0);

        // ArcRotate specific
        var isArc = (camType === 1);
        writeU8(isArc ? 1 : 0);
        if (isArc) {
            writeF32(safeFloat(cam.alpha));
            writeF32(safeFloat(cam.beta));
            writeF32(safeFloat(cam.radius));
            writeF32(cam.lowerAlphaLimit !== undefined && cam.lowerAlphaLimit !== null ? cam.lowerAlphaLimit : -3.402823e+38);
            writeF32(cam.upperAlphaLimit !== undefined && cam.upperAlphaLimit !== null ? cam.upperAlphaLimit : 3.402823e+38);
            writeF32(cam.lowerBetaLimit !== undefined && cam.lowerBetaLimit !== null ? cam.lowerBetaLimit : 0.01);
            writeF32(cam.upperBetaLimit !== undefined && cam.upperBetaLimit !== null ? cam.upperBetaLimit : 3.1316);
            writeF32(cam.lowerRadiusLimit !== undefined && cam.lowerRadiusLimit !== null ? cam.lowerRadiusLimit : 0);
            writeF32(cam.upperRadiusLimit !== undefined && cam.upperRadiusLimit !== null ? cam.upperRadiusLimit : 3.402823e+38);
        }
    }

    // --- Transform Nodes ---
    for (var ni = 0; ni < transformNodes.length; ni++) {
        var node = transformNodes[ni];
        writeU32(node.uniqueId || 0);
        writeU32(getParentId(node));
        writeString(node.name);
        writeString(node.getClassName ? node.getClassName() : "TransformNode");
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
    }

    // --- Meshes ---
    for (var mi = 0; mi < meshes.length; mi++) {
        var mesh = meshes[mi];
        writeU32(mesh.uniqueId || 0);
        writeU32(getParentId(mesh));
        writeString(mesh.name);
        writeString(mesh.getClassName ? mesh.getClassName() : "Mesh");
        writeVec3(mesh.position);
        if (mesh.rotationQuaternion) {
            writeU8(1);
            writeVec4(mesh.rotationQuaternion);
        } else {
            writeU8(0);
            writeVec3(mesh.rotation);
        }
        writeVec3(mesh.scaling);
        var meshEnabled = true;
        try { meshEnabled = mesh.isEnabled(); } catch (e) {}
        writeU8(meshEnabled ? 1 : 0);
        writeU8(safeBool(mesh.isVisible));
        writeF32(mesh.visibility !== undefined ? mesh.visibility : 1.0);
        writeU8(safeBool(mesh.receiveShadows));
        writeU32(mesh.layerMask !== undefined ? mesh.layerMask : 0x0FFFFFFF);

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

        // Material uniqueId
        var matUid = -1;
        if (mesh.material && mesh.material.uniqueId !== undefined) {
            matUid = mesh.material.uniqueId;
        }
        writeI32(matUid);

        // Morph targets
        var morphManager = mesh.morphTargetManager;
        if (morphManager) {
            var numTargets = morphManager.numTargets || 0;
            writeU8(numTargets > 255 ? 255 : numTargets);
            for (var mti2 = 0; mti2 < numTargets && mti2 < 255; mti2++) {
                var mt = morphManager.getTarget(mti2);
                writeString(mt ? (mt.name || "target_" + mti2) : "target_" + mti2);
                writeF32(mt ? safeFloat(mt.influence) : 0);
            }
        } else {
            writeU8(0);
        }
    }

    // --- Materials ---
    var textureSlotNames = [
        "diffuseTexture", "specularTexture", "emissiveTexture", "ambientTexture",
        "bumpTexture", "opacityTexture", "reflectionTexture", "lightmapTexture",
        "albedoTexture", "metallicTexture", "reflectivityTexture", "microSurfaceTexture"
    ];

    for (var mti = 0; mti < materials.length; mti++) {
        var mat = materials[mti];
        writeU32(mat.uniqueId || 0);
        writeString(mat.name);
        writeString(mat.getClassName ? mat.getClassName() : "Material");
        writeF32(mat.alpha !== undefined ? mat.alpha : 1.0);
        writeU8(safeBool(mat.backFaceCulling));
        writeU8(safeBool(mat.wireframe));
        writeU8(safeBool(mat.pointsCloud));
        writeU8(safeBool(mat.disableDepthWrite));
        writeU8(safeBool(mat.forceDepthWrite));
        writeF32(safeFloat(mat.zOffset));
        writeF32(safeFloat(mat.zOffsetUnits));

        var tm = mat.transparencyMode;
        writeI32(tm !== undefined && tm !== null ? tm : -1);
        writeF32(mat.alphaCutOff !== undefined ? mat.alphaCutOff : 0.4);

        writeColor3(mat.diffuseColor || mat.albedoColor);
        writeColor3(mat.specularColor || mat.reflectivityColor);
        writeColor3(mat.emissiveColor);
        writeColor3(mat.ambientColor);

        // PBR
        var isPBR = mat.getClassName && (
            mat.getClassName() === "PBRMaterial" ||
            mat.getClassName() === "PBRMetallicRoughnessMaterial" ||
            mat.getClassName() === "PBRSpecularGlossinessMaterial");
        writeU8(isPBR ? 1 : 0);
        if (isPBR) {
            writeF32(mat.metallic !== undefined ? mat.metallic : 0);
            writeF32(mat.roughness !== undefined ? mat.roughness : 1);
            writeF32(mat.environmentIntensity !== undefined ? mat.environmentIntensity : 1);
        }

        // Texture slots
        var slots = [];
        for (var tsi = 0; tsi < textureSlotNames.length; tsi++) {
            var slotName = textureSlotNames[tsi];
            var slotTex = mat[slotName];
            if (slotTex && slotTex.name) {
                slots.push({
                    slot: slotName,
                    name: slotTex.name,
                    uid: slotTex.uniqueId !== undefined ? slotTex.uniqueId : -1
                });
            }
        }
        writeU8(slots.length > 255 ? 255 : slots.length);
        for (var tsi2 = 0; tsi2 < slots.length && tsi2 < 255; tsi2++) {
            writeString(slots[tsi2].slot);
            writeString(slots[tsi2].name);
            writeI32(slots[tsi2].uid);
        }
    }

    // --- Textures ---
    for (var ti = 0; ti < textures.length; ti++) {
        var tex = textures[ti];
        writeU32(tex.uniqueId || 0);
        writeString(tex.name);
        writeString(tex.getClassName ? tex.getClassName() : "BaseTexture");
        writeString(tex.url || "");

        var sz = null;
        try { if (tex.getSize) sz = tex.getSize(); } catch (e) {}
        writeI32(sz && sz.width  ? sz.width  : -1);
        writeI32(sz && sz.height ? sz.height : -1);

        writeU8(safeBool(tex.hasAlpha));
        writeU8(safeBool(tex.isCube));
        writeU8(safeBool(tex.gammaSpace));
        writeU8(tex.coordinatesMode !== undefined ? tex.coordinatesMode : 0);
        writeU8(tex.samplingMode !== undefined ? tex.samplingMode : 3);
        writeF32(tex.uOffset !== undefined ? tex.uOffset : 0);
        writeF32(tex.vOffset !== undefined ? tex.vOffset : 0);
        writeF32(tex.uScale !== undefined ? tex.uScale : 1);
        writeF32(tex.vScale !== undefined ? tex.vScale : 1);
        writeF32(tex.level !== undefined ? tex.level : 1);
    }

    // --- Lights ---
    for (var li = 0; li < lights.length; li++) {
        var light = lights[li];
        writeU32(light.uniqueId || 0);
        writeU32(getParentId(light));
        writeString(light.name);
        writeString(light.getClassName ? light.getClassName() : "Light");

        var lightType = 3;
        if (light.getClassName) {
            var lcn = light.getClassName();
            if (lcn === "PointLight")            lightType = 0;
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
        writeF32(safeFloat(light.range));

        // Spot specific
        var isSpot = (lightType === 2);
        writeU8(isSpot ? 1 : 0);
        if (isSpot) {
            writeF32(safeFloat(light.angle));
            writeF32(safeFloat(light.innerAngle));
            writeF32(safeFloat(light.exponent));
        }

        // Shadow generator
        var sg = light.getShadowGenerator ? light.getShadowGenerator() : null;
        writeU8(sg ? 1 : 0);
        if (sg) {
            writeF32(safeFloat(sg.getShadowMap() ? sg.getShadowMap().renderList ? sg.getShadowMap().renderList.length : 0 : 0));
            writeF32(safeFloat(sg.bias));
            writeF32(safeFloat(sg.normalBias));
        }
    }

    // --- Animation Groups ---
    for (var ai = 0; ai < animationGroups.length; ai++) {
        var ag = animationGroups[ai];
        writeU32(ai);
        writeString(ag.name || "AnimationGroup_" + ai);
        writeU8(safeBool(ag.isPlaying));
        writeF32(ag.speedRatio !== undefined ? ag.speedRatio : 1);
        writeF32(safeFloat(ag.from));
        writeF32(safeFloat(ag.to));
        writeU8(safeBool(ag.loopAnimation));
        var cf = 0;
        try {
            if (ag.isPlaying && ag.animatables && ag.animatables.length > 0) {
                cf = ag.animatables[0].masterFrame || 0;
            }
        } catch (e) {}
        writeF32(cf);
    }

    // --- Skeletons ---
    for (var ski = 0; ski < skeletons.length; ski++) {
        var skel = skeletons[ski];
        writeU32(ski);
        writeString(skel.name || "Skeleton_" + ski);
        writeU32(skel.bones ? skel.bones.length : 0);
        writeU8(safeBool(skel.useTextureToStoreBoneMatrices));
    }

    return buffer.slice(0, offset);
}

// ===========================================================================
// Apply inspector commands from C++ (C++→JS communication)
// ===========================================================================
// Command type constants (must match C++ InspectorCmd enum)
var CMD_SET_POSITION            = 0x01;
var CMD_SET_ROTATION            = 0x02;
var CMD_SET_SCALING             = 0x03;
var CMD_SET_ENABLED             = 0x10;
var CMD_SET_VISIBLE             = 0x11;
var CMD_SET_WIREFRAME           = 0x12;
var CMD_SET_BACK_FACE_CULLING   = 0x13;
var CMD_SET_RECEIVE_SHADOWS     = 0x14;
var CMD_SET_POINTS_CLOUD_MAT    = 0x15;
var CMD_SET_DISABLE_DEPTH_WRITE = 0x16;
var CMD_SET_FORCE_DEPTH_WRITE   = 0x17;
var CMD_SET_AUTO_CLEAR          = 0x18;
var CMD_SET_SHADOW_ENABLED      = 0x19;
var CMD_SET_ALPHA               = 0x20;
var CMD_SET_VISIBILITY          = 0x21;
var CMD_SET_INTENSITY           = 0x22;
var CMD_SET_FOV                 = 0x23;
var CMD_SET_MIN_Z               = 0x24;
var CMD_SET_MAX_Z               = 0x25;
var CMD_SET_INERTIA             = 0x26;
var CMD_SET_SPEED               = 0x27;
var CMD_SET_RANGE               = 0x28;
var CMD_SET_SPOT_ANGLE          = 0x29;
var CMD_SET_INNER_ANGLE         = 0x2A;
var CMD_SET_EXPONENT            = 0x2B;
var CMD_SET_ALPHA_CUTOFF        = 0x2C;
var CMD_SET_Z_OFFSET            = 0x2D;
var CMD_SET_Z_OFFSET_UNITS      = 0x2E;
var CMD_SET_METALLIC            = 0x2F;
var CMD_SET_ROUGHNESS           = 0x30;
var CMD_SET_ENV_INTENSITY       = 0x31;
var CMD_SET_CONTRAST            = 0x32;
var CMD_SET_EXPOSURE            = 0x33;
var CMD_SET_SPEED_RATIO         = 0x34;
var CMD_SET_ARC_ALPHA           = 0x35;
var CMD_SET_ARC_BETA            = 0x36;
var CMD_SET_ARC_RADIUS          = 0x37;
var CMD_SET_FOG_DENSITY         = 0x38;
var CMD_SET_FOG_START           = 0x39;
var CMD_SET_FOG_END             = 0x3A;
var CMD_SET_LAYER_MASK          = 0x3B;
var CMD_SET_TEXTURE_LEVEL       = 0x3C;
var CMD_SET_MORPH_INFLUENCE     = 0x3D;
var CMD_SET_SHADOW_BIAS         = 0x3E;
var CMD_SET_SHADOW_NORMAL_BIAS  = 0x3F;
var CMD_SET_DIFFUSE_COLOR       = 0x40;
var CMD_SET_SPECULAR_COLOR      = 0x41;
var CMD_SET_EMISSIVE_COLOR      = 0x42;
var CMD_SET_AMBIENT_COLOR       = 0x43;
var CMD_SET_LIGHT_DIFFUSE       = 0x44;
var CMD_SET_LIGHT_SPECULAR      = 0x45;
var CMD_SET_FOG_COLOR           = 0x46;
var CMD_SET_SCENE_AMBIENT       = 0x47;
var CMD_SET_CLEAR_COLOR         = 0x50;
var CMD_SET_DIRECTION           = 0x60;
var CMD_SET_TARGET              = 0x61;
var CMD_SET_GRAVITY             = 0x62;
var CMD_SET_TRANSPARENCY_MODE   = 0x70;
var CMD_SET_FOG_MODE            = 0x71;
var CMD_SET_TONE_MAPPING_TYPE   = 0x72;
var CMD_SET_CAMERA_MODE         = 0x73;
var CMD_SET_NAME                = 0x80;
var CMD_DISPOSE_ENTITY          = 0x90;
var CMD_ANIM_PLAY               = 0x91;
var CMD_ANIM_PAUSE              = 0x92;
var CMD_ANIM_STOP               = 0x93;
var CMD_SKELETON_RETURN_TO_REST = 0x94;
var CMD_SET_FORCE_WIREFRAME     = 0x95;
var CMD_SET_FORCE_POINTS_CLOUD  = 0x96;
var CMD_DEBUG_TOGGLE_GRID       = 0xA0;
var CMD_DEBUG_TOGGLE_NAMES      = 0xA1;
var CMD_DEBUG_TOGGLE_PHYSICS    = 0xA2;
var CMD_DEBUG_TOGGLE_BBOX       = 0xA4;
var CMD_DEBUG_TOGGLE_AXES       = 0xA5;
var CMD_DEBUG_SET_TEX_CHANNEL   = 0xA6;
var CMD_DEBUG_TOGGLE_ANIMATIONS = 0xA7;
var CMD_DEBUG_TOGGLE_PARTICLES  = 0xA8;
var CMD_DEBUG_TOGGLE_FOG        = 0xA9;
var CMD_DEBUG_TOGGLE_SHADOWS    = 0xAA;
var CMD_DEBUG_TOGGLE_LIGHTS     = 0xAB;
var CMD_DEBUG_TOGGLE_POSTPROC   = 0xAC;
var CMD_DEBUG_TOGGLE_SKELETONS  = 0xAD;
var CMD_SET_ANIM_FRAME          = 0xB0;
var CMD_SET_ANIM_LOOP           = 0xB1;

// Debug state tracking
var _debugState = {};

function _findEntityByUniqueId(scene, uid) {
    var node = scene.getNodeByUniqueId ? scene.getNodeByUniqueId(uid) : null;
    if (node) return node;

    var mats = scene.materials || [];
    for (var i = 0; i < mats.length; i++) {
        if (mats[i].uniqueId === uid) return mats[i];
    }

    var texs = scene.textures || [];
    for (var i = 0; i < texs.length; i++) {
        if (texs[i].uniqueId === uid) return texs[i];
    }

    return null;
}

function ApplyInspectorCommands(buf) {
    if (!currentScene || !buf) return;

    var view = new DataView(buf);
    var offset = 0;

    function readU8()  { var v = view.getUint8(offset);            offset += 1; return v; }
    function readU16() { var v = view.getUint16(offset, true);     offset += 2; return v; }
    function readU32() { var v = view.getUint32(offset, true);     offset += 4; return v; }
    function readI32() { var v = view.getInt32(offset, true);      offset += 4; return v; }
    function readF32() { var v = view.getFloat32(offset, true);    offset += 4; return v; }

    function readString() {
        var len = readU16();
        var s = "";
        for (var i = 0; i < len; i++) s += String.fromCharCode(readU8());
        return s;
    }

    function readVec3() { return { x: readF32(), y: readF32(), z: readF32() }; }
    function readColor3() { return { r: readF32(), g: readF32(), b: readF32() }; }
    function readColor4() { return { r: readF32(), g: readF32(), b: readF32(), a: readF32() }; }

    // Validate header
    var magic = readU32();
    var version = readU32();
    var cmdCount = readU32();
    if (magic !== 0x434D4432 || version !== 1) return;

    var scene = currentScene;

    for (var c = 0; c < cmdCount; c++) {
        var cmdType = readU8();
        var uid = readU32();

        try {
            // Scene-level commands (uid === 0)
            if (uid === 0) {
                switch (cmdType) {
                    case CMD_SET_CLEAR_COLOR:
                        var cc = readColor4();
                        scene.clearColor = new BABYLON.Color4(cc.r, cc.g, cc.b, cc.a);
                        break;
                    case CMD_SET_SCENE_AMBIENT:
                        var ac = readColor3();
                        scene.ambientColor = new BABYLON.Color3(ac.r, ac.g, ac.b);
                        break;
                    case CMD_SET_FOG_MODE:
                        scene.fogMode = readI32();
                        break;
                    case CMD_SET_FOG_COLOR:
                        var fc = readColor3();
                        scene.fogColor = new BABYLON.Color3(fc.r, fc.g, fc.b);
                        break;
                    case CMD_SET_FOG_DENSITY:
                        scene.fogDensity = readF32();
                        break;
                    case CMD_SET_FOG_START:
                        scene.fogStart = readF32();
                        break;
                    case CMD_SET_FOG_END:
                        scene.fogEnd = readF32();
                        break;
                    case CMD_SET_GRAVITY:
                        var g = readVec3();
                        scene.gravity = new BABYLON.Vector3(g.x, g.y, g.z);
                        break;
                    case CMD_SET_AUTO_CLEAR:
                        scene.autoClear = readU8() !== 0;
                        break;
                    case CMD_SET_FORCE_WIREFRAME:
                        scene.forceWireframe = readU8() !== 0;
                        break;
                    case CMD_SET_FORCE_POINTS_CLOUD:
                        scene.forcePointsCloud = readU8() !== 0;
                        break;
                    case CMD_SET_CONTRAST:
                        if (scene.imageProcessingConfiguration)
                            scene.imageProcessingConfiguration.contrast = readF32();
                        else readF32();
                        break;
                    case CMD_SET_EXPOSURE:
                        if (scene.imageProcessingConfiguration)
                            scene.imageProcessingConfiguration.exposure = readF32();
                        else readF32();
                        break;
                    case CMD_SET_TONE_MAPPING_TYPE:
                        if (scene.imageProcessingConfiguration)
                            scene.imageProcessingConfiguration.toneMappingType = readI32();
                        else readI32();
                        break;
                    case CMD_DEBUG_TOGGLE_GRID:
                        _toggleDebugGrid(scene);
                        break;
                    case CMD_DEBUG_TOGGLE_BBOX:
                        _toggleDebugBoundingBoxes(scene);
                        break;
                    case CMD_DEBUG_TOGGLE_AXES:
                        _toggleDebugAxes(scene);
                        break;
                    case CMD_DEBUG_TOGGLE_ANIMATIONS:
                        _debugState.animationsDisabled = !_debugState.animationsDisabled;
                        scene.animationsEnabled = !_debugState.animationsDisabled;
                        break;
                    case CMD_DEBUG_TOGGLE_PARTICLES:
                        _debugState.particlesDisabled = !_debugState.particlesDisabled;
                        scene.particlesEnabled = !_debugState.particlesDisabled;
                        break;
                    case CMD_DEBUG_TOGGLE_FOG:
                        if (!_debugState.fogDisabled) {
                            _debugState.savedFogMode = scene.fogMode;
                            scene.fogMode = 0;
                            _debugState.fogDisabled = true;
                        } else {
                            scene.fogMode = _debugState.savedFogMode || 0;
                            _debugState.fogDisabled = false;
                        }
                        break;
                    case CMD_DEBUG_TOGGLE_SHADOWS:
                        _debugState.shadowsDisabled = !_debugState.shadowsDisabled;
                        scene.shadowsEnabled = !_debugState.shadowsDisabled;
                        break;
                    case CMD_DEBUG_TOGGLE_LIGHTS:
                        _debugState.lightsDisabled = !_debugState.lightsDisabled;
                        scene.lightsEnabled = !_debugState.lightsDisabled;
                        break;
                    case CMD_DEBUG_TOGGLE_POSTPROC:
                        _debugState.postProcessesDisabled = !_debugState.postProcessesDisabled;
                        scene.postProcessesEnabled = !_debugState.postProcessesDisabled;
                        break;
                    case CMD_DEBUG_TOGGLE_SKELETONS:
                        _debugState.skeletonsDisabled = !_debugState.skeletonsDisabled;
                        scene.skeletonsEnabled = !_debugState.skeletonsDisabled;
                        break;
                    case CMD_DEBUG_SET_TEX_CHANNEL:
                        var channel = readU8();
                        var enabled = readU8();
                        _setTextureChannel(scene, channel, enabled !== 0);
                        break;
                    default:
                        break;
                }
                continue;
            }

            // Animation group commands (uid is group index for anim commands)
            if (cmdType >= CMD_ANIM_PLAY && cmdType <= CMD_ANIM_STOP) {
                var ags = scene.animationGroups || [];
                if (uid < ags.length) {
                    var ag = ags[uid];
                    if (cmdType === CMD_ANIM_PLAY) ag.play(ag.loopAnimation);
                    else if (cmdType === CMD_ANIM_PAUSE) ag.pause();
                    else if (cmdType === CMD_ANIM_STOP) ag.stop();
                }
                continue;
            }
            if (cmdType === CMD_SET_SPEED_RATIO) {
                var ags2 = scene.animationGroups || [];
                if (uid < ags2.length) ags2[uid].speedRatio = readF32();
                else readF32();
                continue;
            }
            if (cmdType === CMD_SET_ANIM_FRAME) {
                var ags3 = scene.animationGroups || [];
                if (uid < ags3.length) ags3[uid].goToFrame(readF32());
                else readF32();
                continue;
            }
            if (cmdType === CMD_SET_ANIM_LOOP) {
                var ags4 = scene.animationGroups || [];
                if (uid < ags4.length) ags4[uid].loopAnimation = readU8() !== 0;
                else readU8();
                continue;
            }
            if (cmdType === CMD_SKELETON_RETURN_TO_REST) {
                var skels = scene.skeletons || [];
                if (uid < skels.length) skels[uid].returnToRest();
                continue;
            }

            // Entity commands
            var entity = _findEntityByUniqueId(scene, uid);
            if (!entity) {
                // Skip the command data to avoid desync
                _skipCommandData(cmdType, view, offset);
                offset = _skipOffset;
                continue;
            }

            switch (cmdType) {
                case CMD_SET_POSITION:
                    var p = readVec3();
                    entity.position = new BABYLON.Vector3(p.x, p.y, p.z);
                    break;
                case CMD_SET_ROTATION:
                    var r = readVec3();
                    if (entity.rotationQuaternion) {
                        entity.rotationQuaternion = BABYLON.Quaternion.FromEulerAngles(r.x, r.y, r.z);
                    } else {
                        entity.rotation = new BABYLON.Vector3(r.x, r.y, r.z);
                    }
                    break;
                case CMD_SET_SCALING:
                    var s = readVec3();
                    entity.scaling = new BABYLON.Vector3(s.x, s.y, s.z);
                    break;
                case CMD_SET_DIRECTION:
                    var d = readVec3();
                    entity.direction = new BABYLON.Vector3(d.x, d.y, d.z);
                    break;
                case CMD_SET_TARGET:
                    var t = readVec3();
                    if (entity.setTarget) entity.setTarget(new BABYLON.Vector3(t.x, t.y, t.z));
                    else entity.target = new BABYLON.Vector3(t.x, t.y, t.z);
                    break;
                case CMD_SET_ENABLED:
                    var en = readU8() !== 0;
                    if (entity.setEnabled) entity.setEnabled(en);
                    break;
                case CMD_SET_VISIBLE:
                    entity.isVisible = readU8() !== 0;
                    break;
                case CMD_SET_WIREFRAME:
                    entity.wireframe = readU8() !== 0;
                    break;
                case CMD_SET_BACK_FACE_CULLING:
                    entity.backFaceCulling = readU8() !== 0;
                    break;
                case CMD_SET_RECEIVE_SHADOWS:
                    entity.receiveShadows = readU8() !== 0;
                    break;
                case CMD_SET_POINTS_CLOUD_MAT:
                    entity.pointsCloud = readU8() !== 0;
                    break;
                case CMD_SET_DISABLE_DEPTH_WRITE:
                    entity.disableDepthWrite = readU8() !== 0;
                    break;
                case CMD_SET_FORCE_DEPTH_WRITE:
                    entity.forceDepthWrite = readU8() !== 0;
                    break;
                case CMD_SET_ALPHA:
                    entity.alpha = readF32();
                    break;
                case CMD_SET_VISIBILITY:
                    entity.visibility = readF32();
                    break;
                case CMD_SET_INTENSITY:
                    entity.intensity = readF32();
                    break;
                case CMD_SET_FOV:
                    entity.fov = readF32();
                    break;
                case CMD_SET_MIN_Z:
                    entity.minZ = readF32();
                    break;
                case CMD_SET_MAX_Z:
                    entity.maxZ = readF32();
                    break;
                case CMD_SET_INERTIA:
                    entity.inertia = readF32();
                    break;
                case CMD_SET_SPEED:
                    entity.speed = readF32();
                    break;
                case CMD_SET_RANGE:
                    entity.range = readF32();
                    break;
                case CMD_SET_SPOT_ANGLE:
                    entity.angle = readF32();
                    break;
                case CMD_SET_INNER_ANGLE:
                    entity.innerAngle = readF32();
                    break;
                case CMD_SET_EXPONENT:
                    entity.exponent = readF32();
                    break;
                case CMD_SET_ALPHA_CUTOFF:
                    entity.alphaCutOff = readF32();
                    break;
                case CMD_SET_Z_OFFSET:
                    entity.zOffset = readF32();
                    break;
                case CMD_SET_Z_OFFSET_UNITS:
                    entity.zOffsetUnits = readF32();
                    break;
                case CMD_SET_METALLIC:
                    entity.metallic = readF32();
                    break;
                case CMD_SET_ROUGHNESS:
                    entity.roughness = readF32();
                    break;
                case CMD_SET_ENV_INTENSITY:
                    entity.environmentIntensity = readF32();
                    break;
                case CMD_SET_ARC_ALPHA:
                    entity.alpha = readF32();
                    break;
                case CMD_SET_ARC_BETA:
                    entity.beta = readF32();
                    break;
                case CMD_SET_ARC_RADIUS:
                    entity.radius = readF32();
                    break;
                case CMD_SET_LAYER_MASK:
                    entity.layerMask = readU32();
                    break;
                case CMD_SET_TEXTURE_LEVEL:
                    entity.level = readF32();
                    break;
                case CMD_SET_MORPH_INFLUENCE:
                    var morphIdx = readU8();
                    var morphVal = readF32();
                    if (entity.morphTargetManager) {
                        var target = entity.morphTargetManager.getTarget(morphIdx);
                        if (target) target.influence = morphVal;
                    }
                    break;
                case CMD_SET_SHADOW_BIAS:
                    if (entity.getShadowGenerator) {
                        var sgb = entity.getShadowGenerator();
                        if (sgb) sgb.bias = readF32();
                        else readF32();
                    } else readF32();
                    break;
                case CMD_SET_SHADOW_NORMAL_BIAS:
                    if (entity.getShadowGenerator) {
                        var sgnb = entity.getShadowGenerator();
                        if (sgnb) sgnb.normalBias = readF32();
                        else readF32();
                    } else readF32();
                    break;
                case CMD_SET_DIFFUSE_COLOR:
                    var dc = readColor3();
                    if (entity.diffuseColor) entity.diffuseColor = new BABYLON.Color3(dc.r, dc.g, dc.b);
                    else if (entity.albedoColor) entity.albedoColor = new BABYLON.Color3(dc.r, dc.g, dc.b);
                    break;
                case CMD_SET_SPECULAR_COLOR:
                    var sc2 = readColor3();
                    if (entity.specularColor) entity.specularColor = new BABYLON.Color3(sc2.r, sc2.g, sc2.b);
                    else if (entity.reflectivityColor) entity.reflectivityColor = new BABYLON.Color3(sc2.r, sc2.g, sc2.b);
                    break;
                case CMD_SET_EMISSIVE_COLOR:
                    var ec = readColor3();
                    entity.emissiveColor = new BABYLON.Color3(ec.r, ec.g, ec.b);
                    break;
                case CMD_SET_AMBIENT_COLOR:
                    var amc = readColor3();
                    entity.ambientColor = new BABYLON.Color3(amc.r, amc.g, amc.b);
                    break;
                case CMD_SET_LIGHT_DIFFUSE:
                    var ld = readColor3();
                    entity.diffuse = new BABYLON.Color3(ld.r, ld.g, ld.b);
                    break;
                case CMD_SET_LIGHT_SPECULAR:
                    var ls = readColor3();
                    entity.specular = new BABYLON.Color3(ls.r, ls.g, ls.b);
                    break;
                case CMD_SET_TRANSPARENCY_MODE:
                    entity.transparencyMode = readI32();
                    break;
                case CMD_SET_CAMERA_MODE:
                    entity.mode = readI32();
                    break;
                case CMD_SET_NAME:
                    entity.name = readString();
                    break;
                case CMD_DISPOSE_ENTITY:
                    entity.dispose();
                    break;
                default:
                    break;
            }
        } catch (e) {
            // Skip errors for individual commands
        }
    }
}

var _skipOffset = 0;
function _skipCommandData(cmdType, view, off) {
    // Skip known data sizes for each command type to avoid buffer desync
    _skipOffset = off;
    if (cmdType === CMD_SET_POSITION || cmdType === CMD_SET_ROTATION ||
        cmdType === CMD_SET_SCALING || cmdType === CMD_SET_DIRECTION ||
        cmdType === CMD_SET_TARGET || cmdType === CMD_SET_GRAVITY ||
        cmdType === CMD_SET_DIFFUSE_COLOR || cmdType === CMD_SET_SPECULAR_COLOR ||
        cmdType === CMD_SET_EMISSIVE_COLOR || cmdType === CMD_SET_AMBIENT_COLOR ||
        cmdType === CMD_SET_LIGHT_DIFFUSE || cmdType === CMD_SET_LIGHT_SPECULAR ||
        cmdType === CMD_SET_FOG_COLOR || cmdType === CMD_SET_SCENE_AMBIENT) {
        _skipOffset += 12;
    } else if (cmdType === CMD_SET_CLEAR_COLOR) {
        _skipOffset += 16;
    } else if (cmdType >= 0x20 && cmdType <= 0x3C && cmdType !== CMD_SET_LAYER_MASK &&
               cmdType !== CMD_SET_MORPH_INFLUENCE) {
        _skipOffset += 4;
    } else if (cmdType === CMD_SET_LAYER_MASK) {
        _skipOffset += 4;
    } else if (cmdType === CMD_SET_MORPH_INFLUENCE) {
        _skipOffset += 5;
    } else if (cmdType === CMD_SET_ENABLED || cmdType === CMD_SET_VISIBLE ||
               cmdType === CMD_SET_WIREFRAME || cmdType === CMD_SET_BACK_FACE_CULLING ||
               cmdType === CMD_SET_RECEIVE_SHADOWS || cmdType === CMD_SET_POINTS_CLOUD_MAT ||
               cmdType === CMD_SET_DISABLE_DEPTH_WRITE || cmdType === CMD_SET_FORCE_DEPTH_WRITE ||
               cmdType === CMD_SET_AUTO_CLEAR || cmdType === CMD_SET_SHADOW_ENABLED ||
               cmdType === CMD_SET_FORCE_WIREFRAME || cmdType === CMD_SET_FORCE_POINTS_CLOUD) {
        _skipOffset += 1;
    } else if (cmdType === CMD_SET_TRANSPARENCY_MODE || cmdType === CMD_SET_FOG_MODE ||
               cmdType === CMD_SET_TONE_MAPPING_TYPE || cmdType === CMD_SET_CAMERA_MODE) {
        _skipOffset += 4;
    } else if (cmdType === CMD_SET_NAME) {
        var slen = view.getUint16(_skipOffset, true);
        _skipOffset += 2 + slen;
    } else if (cmdType === CMD_DEBUG_SET_TEX_CHANNEL) {
        _skipOffset += 2;
    } else if (cmdType === CMD_SET_ANIM_FRAME || cmdType === CMD_SET_SPEED_RATIO) {
        _skipOffset += 4;
    } else if (cmdType === CMD_SET_ANIM_LOOP) {
        _skipOffset += 1;
    }
    // Action commands (DISPOSE, PLAY, PAUSE, STOP, etc.) have no extra data
}

// Debug helper functions
function _toggleDebugGrid(scene) {
    if (_debugState.gridMesh) {
        _debugState.gridMesh.dispose();
        if (_debugState.gridMaterial) _debugState.gridMaterial.dispose();
        _debugState.gridMesh = null;
        _debugState.gridMaterial = null;
    } else {
        try {
            var gridMat = new BABYLON.GridMaterial("inspectorGrid", scene);
            gridMat.majorUnitFrequency = 10;
            gridMat.minorUnitVisibility = 0.3;
            gridMat.gridRatio = 1;
            gridMat.backFaceCulling = false;
            gridMat.mainColor = new BABYLON.Color3(1, 1, 1);
            gridMat.lineColor = new BABYLON.Color3(1, 1, 1);
            gridMat.opacity = 0.8;

            var gridMesh = BABYLON.MeshBuilder.CreateGround("inspectorGridMesh",
                { width: 100, height: 100 }, scene);
            gridMesh.material = gridMat;
            gridMesh.isPickable = false;

            _debugState.gridMesh = gridMesh;
            _debugState.gridMaterial = gridMat;
        } catch (e) {
            // GridMaterial may not be available
            console.log("GridMaterial not available, using simple grid");
            var simpleGrid = BABYLON.MeshBuilder.CreateGround("inspectorGridMesh",
                { width: 100, height: 100, subdivisions: 20 }, scene);
            var wireMat = new BABYLON.StandardMaterial("inspectorGridMat", scene);
            wireMat.wireframe = true;
            wireMat.alpha = 0.3;
            simpleGrid.material = wireMat;
            simpleGrid.isPickable = false;
            _debugState.gridMesh = simpleGrid;
            _debugState.gridMaterial = wireMat;
        }
    }
}

function _toggleDebugBoundingBoxes(scene) {
    _debugState.showBoundingBoxes = !_debugState.showBoundingBoxes;
    var meshes = scene.meshes || [];
    for (var i = 0; i < meshes.length; i++) {
        meshes[i].showBoundingBox = _debugState.showBoundingBoxes;
    }
}

function _toggleDebugAxes(scene) {
    if (_debugState.axesViewer) {
        _debugState.axesViewer.dispose();
        _debugState.axesViewer = null;
    } else {
        try {
            _debugState.axesViewer = new BABYLON.Debug.AxesViewer(scene, 2);
        } catch (e) {}
    }
}

var _texChannelNames = [
    "diffuseTexture", "ambientTexture", "specularTexture", "emissiveTexture",
    "bumpTexture", "opacityTexture", "reflectionTexture", "refractionTexture",
    "lightmapTexture"
];
var _savedTextures = {};

function _setTextureChannel(scene, channel, enabled) {
    if (channel >= _texChannelNames.length) return;
    var slotName = _texChannelNames[channel];
    var mats = scene.materials || [];

    if (!enabled) {
        _savedTextures[slotName] = _savedTextures[slotName] || {};
        for (var i = 0; i < mats.length; i++) {
            var mat = mats[i];
            if (mat[slotName]) {
                _savedTextures[slotName][mat.uniqueId] = mat[slotName];
                mat[slotName] = null;
            }
        }
    } else {
        var saved = _savedTextures[slotName] || {};
        for (var i = 0; i < mats.length; i++) {
            var mat = mats[i];
            if (saved[mat.uniqueId]) {
                mat[slotName] = saved[mat.uniqueId];
            }
        }
        _savedTextures[slotName] = {};
    }
}

// Initialize with default scene and start render loop
currentScene = createDefaultScene();

var _serializeCounter = 0;
var _SERIALIZE_INTERVAL = 10; // every ~10 frames for better responsiveness

engine.runRenderLoop(function () {
    if (currentScene) {
        // Only render when scene is ready and has an active camera
        if (currentScene.activeCamera) {
            currentScene.render();
        }

        _serializeCounter++;
        if (_serializeCounter >= _SERIALIZE_INTERVAL) {
            _serializeCounter = 0;
            try {
                var buf = serializeSceneData(currentScene);
                if (buf && typeof _nativeSetSceneData === "function") {
                    _nativeSetSceneData(buf);
                }
            } catch (e) {}
        }
    }
});
