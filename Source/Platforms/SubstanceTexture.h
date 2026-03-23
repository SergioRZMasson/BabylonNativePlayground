#pragma once

#include <Babylon/Graphics/RendererType.h>
#include <cstdint>

// Creates a native 2D texture (RGBA8, 1024x1024) suitable for use with
// Babylon::Plugins::ExternalTexture. The caller owns the returned handle.
Babylon::Graphics::TextureT CreateSubstanceTexture(
    void* nativeDevice,
    uint32_t width,
    uint32_t height);

// Updates an existing native texture with new RGBA8 pixel data.
void UpdateSubstanceTexture(
    void* nativeDevice,
    Babylon::Graphics::TextureT texture,
    uint32_t width,
    uint32_t height,
    const uint8_t* rgbaData,
    size_t dataSize);
