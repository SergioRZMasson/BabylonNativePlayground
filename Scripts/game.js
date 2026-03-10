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
    var pgRoot = "https://playground.babylonjs.com";

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
                            console.log("Playground loaded: " + hash);
                        }).catch(function (e) {
                            console.error("Playground createScene promise rejected:");
                            onError(e);
                        });
                    } else if (newScene) {
                        setCurrentScene(newScene);
                        console.log("Playground loaded: " + hash);
                    } else {
                        console.error("createScene did not return a scene.");
                        onError();
                    }
                } catch (e) {
                    console.error("Error evaluating playground code:");
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

// Initialize with default scene and start render loop
currentScene = createDefaultScene();

engine.runRenderLoop(function () {
    if (currentScene) {
        currentScene.render();
    }
});
