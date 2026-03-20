// ===========================================================================
// OBJ file loader — receives raw file bytes from C++ and loads into a new scene
// ===========================================================================
function load_obj(arrayBuffer, fileName) {
    if (!arrayBuffer || arrayBuffer.byteLength === 0) {
        console.error("load_obj: empty buffer");
        return;
    }

    console.log("Loading OBJ: " + (fileName || "file.obj") + " (" + arrayBuffer.byteLength + " bytes)");

    var newScene = new BABYLON.Scene(engine);

    // Convert ArrayBuffer to string for OBJ (it's text-based)
    var bytes = new Uint8Array(arrayBuffer);
    var text = "";
    var chunkSize = 8192;
    for (var i = 0; i < bytes.length; i += chunkSize) {
        var end = Math.min(i + chunkSize, bytes.length);
        var chunk = "";
        for (var j = i; j < end; j++) {
            chunk += String.fromCharCode(bytes[j]);
        }
        text += chunk;
    }

    var assetUrl = "data:;base64," + btoa(text);

    BABYLON.SceneLoader.Append(assetUrl, "", newScene, function () {
        newScene.createDefaultCameraOrLight(true, true, true);
        try { newScene.createDefaultEnvironment(); } catch (e) {}
        setCurrentScene(newScene);
        console.log("OBJ loaded successfully: " + (fileName || "file.obj"));
    }, null, function (scene, message, exception) {
        console.error("Failed to load OBJ: " + message);
        newScene.dispose();
    }, ".obj");
}
