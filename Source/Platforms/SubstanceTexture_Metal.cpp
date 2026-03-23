// Metal stub — to be implemented when building on macOS
#include "SubstanceTexture.h"
#include <stdexcept>

Babylon::Graphics::TextureT CreateSubstanceTexture(
    void* nativeDevice,
    uint32_t width,
    uint32_t height)
{
    // TODO: Implement Metal texture creation using MTLDevice
    throw std::runtime_error("Metal Substance texture not yet implemented");
}

void UpdateSubstanceTexture(
    void* nativeDevice,
    Babylon::Graphics::TextureT texture,
    uint32_t width,
    uint32_t height,
    const uint8_t* rgbaData,
    size_t dataSize)
{
    // TODO: Implement Metal texture update using MTLTexture replaceRegion
    throw std::runtime_error("Metal Substance texture update not yet implemented");
}
