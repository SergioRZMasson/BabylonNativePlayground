// ===========================================================================
// GLB file loader — receives raw file bytes from C++ and loads into a new scene
// ===========================================================================
function load_glb(arrayBuffer, fileName) {
    if (!arrayBuffer || arrayBuffer.byteLength === 0) {
        console.error("load_glb: empty buffer");
        return;
    }

    console.log("Loading GLB: " + (fileName || "file.glb") + " (" + arrayBuffer.byteLength + " bytes)");

    var newScene = new BABYLON.Scene(engine);
    var data = new Uint8Array(arrayBuffer);

    BABYLON.SceneLoader.AppendAsync("", data, newScene, null, ".glb").then(function () {
        newScene.createDefaultCameraOrLight(true, true, true);
        try { newScene.createDefaultEnvironment(); } catch (e) {}
        setCurrentScene(newScene);
        console.log("GLB loaded successfully: " + (fileName || "file.glb"));
    }).catch(function (err) {
        console.error("Failed to load GLB: " + err);
        newScene.dispose();
    });
}
