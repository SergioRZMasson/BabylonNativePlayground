// ===========================================================================
// Substance texture loader — receives ExternalTexture promises from C++ and
// wraps them as Babylon.js textures on a PBR material
// ===========================================================================

// Channel type → PBR material texture property mapping
var _substanceChannelMap = {
    "BaseColor":        "albedoTexture",
    "Diffuse":          "albedoTexture",
    "Normal":           "bumpTexture",
    "Roughness":        "microSurfaceTexture",
    "Metallic":         "reflectivityTexture",
    "AmbientOcclusion": "ambientTexture",
    "Emissive":         "emissiveTexture",
    "Opacity":          "opacityTexture",
    "Height":           "parallaxTexture",
    "Glossiness":       "microSurfaceTexture",
    "Specular":         "reflectivityTexture"
};

/**
 * Find a material by its uniqueId in the current scene.
 */
function _findMaterialByUid(uid) {
    if (!currentScene) return null;
    var materials = currentScene.materials;
    for (var i = 0; i < materials.length; i++) {
        if (materials[i].uniqueId === uid) {
            return materials[i];
        }
    }
    return null;
}

/**
 * Called from C++ when the user clicks "Load Substance Textures".
 * Each entry has a texturePromise (ExternalTexture), channelType, width, height.
 * @param {number} materialUid - The uniqueId of the target BabylonJS material
 * @param {Array} textures - Array of { texturePromise, channelType, width, height }
 */
function _applySubstanceTextures(materialUid, textures) {
    var material = _findMaterialByUid(materialUid);
    if (!material) {
        console.error("[Substance] Material with uniqueId " + materialUid + " not found");
        return;
    }

    if (!(material instanceof BABYLON.PBRMaterial)) {
        console.error("[Substance] Material '" + material.name + "' is not a PBRMaterial");
        return;
    }

    console.log("[Substance] Applying " + textures.length + " texture(s) to PBR material '" + material.name + "'");

    for (var i = 0; i < textures.length; i++) {
        (function(tex) {
            var channelType = tex.channelType;
            var property = _substanceChannelMap[channelType];

            if (!property) {
                console.log("[Substance]   Skipping unmapped channel: " + channelType);
                return;
            }

            tex.texturePromise.then(function(externalTexture) {
                var nativeEngine = currentScene.getEngine();
                var internalTexture = nativeEngine.wrapNativeTexture(externalTexture);

                var babylonTexture = new BABYLON.Texture(null, currentScene);
                babylonTexture._texture = internalTexture;
                babylonTexture.name = "substance_" + channelType;

                // Normal maps need linear color space
                if (channelType === "Normal") {
                    babylonTexture.gammaSpace = false;
                }

                // Non-color data should be linear
                if (channelType === "Roughness" || channelType === "Metallic" ||
                    channelType === "AmbientOcclusion" || channelType === "Height" ||
                    channelType === "Glossiness") {
                    babylonTexture.gammaSpace = false;
                }

                // Adjust PBR properties for separate metallic/roughness textures
                if (channelType === "Metallic") {
                    material.useMetallnessFromMetallicTextureBlue = false;
                }
                if (channelType === "Roughness") {
                    material.useRoughnessFromMetallicTextureAlpha = false;
                    material.useAutoMicroSurfaceFromReflectivityMap = false;
                }

                // Dispose any existing texture in this slot
                if (material[property]) {
                    try { material[property].dispose(); } catch (e) {}
                }

                material[property] = babylonTexture;
                material.markDirty();
                console.log("[Substance]   " + channelType + " -> " + property + " (" + tex.width + "x" + tex.height + ")");
            }).catch(function(err) {
                console.error("[Substance] Failed to wrap external texture for " + channelType + ": " + err);
            });
        })(textures[i]);
    }

    console.log("[Substance] Texture binding initiated for '" + material.name + "'");
}
