// ===========================================================================
// Environment map loader — applies .env texture to the current scene
// ===========================================================================
function load_env(arrayBuffer, fileName) {
    if (!arrayBuffer || arrayBuffer.byteLength === 0) {
        console.error("load_env: empty buffer");
        return;
    }
    if (!currentScene) {
        console.error("load_env: no active scene");
        return;
    }

    console.log("Loading environment: " + (fileName || "environment.env") + " (" + arrayBuffer.byteLength + " bytes)");

    try {
        var hdrTexture = new BABYLON.CubeTexture("data:environment.env", currentScene, {
            buffer: new Uint8Array(arrayBuffer),
            forcedExtension: ".env",
        });

        currentScene.environmentTexture = hdrTexture;
        currentScene.imageProcessingConfiguration.toneMappingEnabled = true;
        currentScene.imageProcessingConfiguration.toneMappingType = BABYLON.ImageProcessingConfiguration.TONEMAPPING_KHR_PBR_NEUTRAL;

        console.log("Environment map applied: " + (fileName || "environment.env"));
    } catch (e) {
        console.error("Failed to load environment map: " + e);
    }
}
