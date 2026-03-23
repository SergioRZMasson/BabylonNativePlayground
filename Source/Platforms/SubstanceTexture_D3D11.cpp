#include "SubstanceTexture.h"
#include <d3d11.h>
#include <stdexcept>
#include <cmath>
#include <cstring>

static uint32_t ComputeMipLevels(uint32_t width, uint32_t height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

Babylon::Graphics::TextureT CreateSubstanceTexture(
    void* nativeDevice,
    uint32_t width,
    uint32_t height)
{
    auto* device = static_cast<ID3D11Device*>(nativeDevice);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = ComputeMipLevels(width, height);
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

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

    // Upload level 0
    uint32_t rowPitch = width * 4;
    ctx->UpdateSubresource(texture, 0, nullptr, rgbaData, rowPitch, 0);

    // Generate the full mip chain from level 0
    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(texture, nullptr, &srv);
    if (srv)
    {
        ctx->GenerateMips(srv);
        srv->Release();
    }

    ctx->Release();
}
