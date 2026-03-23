#include "SubstanceTexture.h"
#include <d3d11.h>
#include <stdexcept>
#include <cstring>

Babylon::Graphics::TextureT CreateSubstanceTexture(
    void* nativeDevice,
    uint32_t width,
    uint32_t height)
{
    auto* device = static_cast<ID3D11Device*>(nativeDevice);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create D3D11 texture for Substance output");

    return static_cast<ID3D11Resource*>(texture);
}

void UpdateSubstanceTexture(
    void* nativeDevice,
    Babylon::Graphics::TextureT texture,
    uint32_t width,
    uint32_t height,
    const uint8_t* rgbaData,
    size_t dataSize)
{
    auto* device = static_cast<ID3D11Device*>(nativeDevice);

    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);

    D3D11_BOX box{};
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = width;
    box.bottom = height;
    box.back = 1;

    uint32_t rowPitch = width * 4; // RGBA8 = 4 bytes per pixel
    ctx->UpdateSubresource(texture, 0, &box, rgbaData, rowPitch, 0);
    ctx->Release();
}
