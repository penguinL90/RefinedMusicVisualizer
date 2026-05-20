#include "pch.h"
#include "DrawCore.h"
#include "vertexShader.h"
#include "pixelShader.h"

DrawCore::DrawCore(HWND _hwnd, UINT _width, UINT _height, NodeOptions const& _options) :
    hwnd(_hwnd), width(_width), height(_height), options(_options)
{
    cbuffer.Color = _options.Color;
    cbuffer.MaxLength = _options.MaxLength;
    cbuffer.MinLength = _options.MinLength;
    cbuffer.Width = _options.Width;
    cbuffer.TransformMatrix = 
        DirectX::XMMatrixTranspose(
            DirectX::XMMatrixOrthographicOffCenterLH(
                0.0f, (float)_width,
                (float)_height, 0.0f,
                0.0f, 1.0f
            )
        );
    initialize();
}

void DrawCore::initialize()
{
    initNodes();
    initD3DDevices();
    initShaders();
    initIndexBuffer();
    initCBuffer();
    initNodeBuffers();
    initSwapChain();
    initDComp();
}

void DrawCore::initNodes()
{
    if (options.Count <= 1) throw std::runtime_error("Node Count must be greater than 1");
    Vector2 pos
    {
        width * 0.5f - options.Gap * (options.Count - 1) * 0.5f ,
        height * 0.5f + options.MaxLength * 0.5 
    }; 
    

    for (UINT i = 0; i < options.Count; ++i)
    {
        nodePositions.push_back(pos);
        pos.X += options.Gap;
    }

    nodePrevValues.resize(options.Count);
    std::fill(nodePrevValues.begin(), nodePrevValues.end(), 0);
}

void DrawCore::initD3DDevices()
{
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDevice(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
        NULL,
        0,
        D3D11_SDK_VERSION,
        pD3DDevice.put(),
        &fl,
        pD3DDeviceContext.put());
    THROW_IF_FAILED(hr);
    pD3DDevice.query_to<IDXGIDevice>(pDXGIDevice.put());
}

void DrawCore::initShaders()
{

    HRESULT hr = pD3DDevice->CreateVertexShader(
        vsByteCode,
        sizeof(vsByteCode),
        NULL,
        pD3DVertexShader.put()
    );
    THROW_IF_FAILED(hr);

    hr = pD3DDevice->CreatePixelShader(
        psByteCode,
        sizeof(psByteCode),
        NULL,
        pD3DPixelShader.put()
    );
    THROW_IF_FAILED(hr);
    pD3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pD3DDeviceContext->IASetInputLayout(NULL);

    D3D11_VIEWPORT vp{};
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)width;
    vp.Height = (float)height;
    pD3DDeviceContext->RSSetViewports(1, &vp);
    pD3DDeviceContext->VSSetShader(pD3DVertexShader.get(), NULL, 0);
    pD3DDeviceContext->PSSetShader(pD3DPixelShader.get(), NULL, 0);
}

void DrawCore::initNodeBuffers()
{
    HRESULT hr = S_OK;
    D3D11_BUFFER_DESC floatSBDesc{};
    floatSBDesc.ByteWidth = sizeof(float) * options.Count;
    floatSBDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    floatSBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    floatSBDesc.Usage = D3D11_USAGE_DYNAMIC;
    floatSBDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    floatSBDesc.StructureByteStride = sizeof(float);

    D3D11_BUFFER_DESC float2SBDesc{};
    float2SBDesc.ByteWidth = sizeof(DirectX::XMFLOAT2) * options.Count;
    float2SBDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    float2SBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    float2SBDesc.Usage = D3D11_USAGE_DYNAMIC;
    float2SBDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    float2SBDesc.StructureByteStride = sizeof(DirectX::XMFLOAT2);

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
    shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    shaderResourceViewDesc.Buffer.FirstElement = 0;
    shaderResourceViewDesc.Buffer.NumElements = options.Count;


    D3D11_SUBRESOURCE_DATA positionsInitData{};

    hr = pD3DDevice->CreateBuffer(&floatSBDesc, NULL, pD3DSB_NodeValues.put());
    THROW_IF_FAILED(hr);
    hr = pD3DDevice->CreateShaderResourceView(
        pD3DSB_NodeValues.get(),
        &shaderResourceViewDesc,
        pD3D11SRV_Values.put());
    THROW_IF_FAILED(hr);

    hr = pD3DDevice->CreateBuffer(&floatSBDesc, NULL, pD3DSB_NodePrevValues.put());
    THROW_IF_FAILED(hr);
    hr = pD3DDevice->CreateShaderResourceView(
        pD3DSB_NodePrevValues.get(),
        &shaderResourceViewDesc,
        pD3D11SRV_PrevValues.put());
    THROW_IF_FAILED(hr);

    positionsInitData.pSysMem = nodePositions.data();
    hr = pD3DDevice->CreateBuffer(&float2SBDesc, &positionsInitData, pD3DSB_NodePositions.put());
    THROW_IF_FAILED(hr);
    hr = pD3DDevice->CreateShaderResourceView(
        pD3DSB_NodePositions.get(),
        &shaderResourceViewDesc,
        pD3D11SRV_Positions.put());
    THROW_IF_FAILED(hr);

    ID3D11ShaderResourceView* const srvs[] =
    {
        pD3D11SRV_Values.get(),
        pD3D11SRV_PrevValues.get(),
        pD3D11SRV_Positions.get(),
    };

    pD3DDeviceContext->VSSetShaderResources(0, 3, srvs);
}

void DrawCore::initCBuffer()
{
    D3D11_BUFFER_DESC constantBufferDesc{};
    constantBufferDesc.ByteWidth = sizeof(CBuffer);
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.MiscFlags = 0;
    constantBufferDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = &this->cbuffer;
    HRESULT hr = pD3DDevice->CreateBuffer(&constantBufferDesc, &initData, pD3DConstantBuffer.addressof());
    THROW_IF_FAILED(hr);
    pD3DDeviceContext->VSSetConstantBuffers(0, 1, pD3DConstantBuffer.addressof());
    pD3DDeviceContext->PSSetConstantBuffers(0, 1, pD3DConstantBuffer.addressof());
}

void DrawCore::initIndexBuffer()
{
    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = sizeof(indices);
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = indices;

    HRESULT hr = pD3DDevice->CreateBuffer(&indexBufferDesc, &initData, pD3DIndexBuffer.addressof());
    THROW_IF_FAILED(hr);
    pD3DDeviceContext->IASetIndexBuffer(pD3DIndexBuffer.get(), DXGI_FORMAT_R32_UINT, 0);
}

void DrawCore::initSwapChain()
{
    wil::com_ptr<IDXGIAdapter> pDXGIAdapter;
    HRESULT hr = pDXGIDevice->GetAdapter(pDXGIAdapter.put());
    THROW_IF_FAILED(hr);

    wil::com_ptr<IDXGIFactory2> pDXGIFactory;
    hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), pDXGIFactory.put_void());
    THROW_IF_FAILED(hr);

    DXGI_SWAP_CHAIN_DESC1 scDesc1{};
    scDesc1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scDesc1.BufferCount = 2;
    scDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc1.Width = width;
    scDesc1.Height = height;
    scDesc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc1.Stereo = FALSE;
    scDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scDesc1.Scaling = DXGI_SCALING_STRETCH;
    scDesc1.Flags = 0;
    scDesc1.SampleDesc.Count = 1;

    hr = pDXGIFactory->CreateSwapChainForComposition(
        pD3DDevice.get(),
        &scDesc1,
        NULL,
        pDXGISwapChain.put());
    THROW_IF_FAILED(hr);
    wil::com_ptr<ID3D11Texture2D> pD3DTexture2D;
    hr = pDXGISwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), pD3DTexture2D.put_void());
    THROW_IF_FAILED(hr);
    hr = pD3DDevice->CreateRenderTargetView(pD3DTexture2D.get(), NULL, pD3DRenderTargetView.put());
    THROW_IF_FAILED(hr);
    pD3DDeviceContext->OMSetRenderTargets(1, pD3DRenderTargetView.addressof(), NULL);
}

void DrawCore::initDComp()
{
    HRESULT hr = DCompositionCreateDevice(
        pDXGIDevice.get(),
        __uuidof(IDCompositionDevice),
        pDCompDevice.put_void()
    );
    THROW_IF_FAILED(hr);
    hr = pDCompDevice->CreateTargetForHwnd(hwnd, TRUE, pDCompTarget.put());
    THROW_IF_FAILED(hr);
    hr = pDCompDevice->CreateVisual(pDCompVisual.put());
    THROW_IF_FAILED(hr);
    hr = pDCompVisual->SetContent(pDXGISwapChain.get());
    THROW_IF_FAILED(hr);
    hr = pDCompTarget->SetRoot(pDCompVisual.get());
    THROW_IF_FAILED(hr);
    hr = pDCompDevice->Commit();
    THROW_IF_FAILED(hr);
}

void DrawCore::Draw(std::span<float> nodeValues)
{
    for (UINT i = 0; i < options.Count; ++i)
    {
        float temp = nodePrevValues[i];
        nodePrevValues[i] = max(temp - options.DampingRate, nodeValues[i]);
    }
    HRESULT hr = S_OK;
    pD3DDeviceContext->OMSetRenderTargets(1, pD3DRenderTargetView.addressof(), NULL);
    pD3DDeviceContext->ClearRenderTargetView(pD3DRenderTargetView.get(), options.ClearColor);
    D3D11_MAPPED_SUBRESOURCE srValues{};
    D3D11_MAPPED_SUBRESOURCE srPrevValues{};

    hr = pD3DDeviceContext->Map(pD3DSB_NodeValues.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &srValues);
    THROW_IF_FAILED(hr);
    memcpy(srValues.pData, nodeValues.data(), sizeof(float) * options.Count);
    pD3DDeviceContext->Unmap(pD3DSB_NodeValues.get(), 0);

    hr = pD3DDeviceContext->Map(pD3DSB_NodePrevValues.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &srPrevValues);
    THROW_IF_FAILED(hr);
    memcpy(srPrevValues.pData, nodePrevValues.data(), sizeof(float) * options.Count);
    pD3DDeviceContext->Unmap(pD3DSB_NodePrevValues.get(), 0);

    pD3DDeviceContext->DrawIndexedInstanced(indexCount, options.Count, 0, 0, 0);
    pDXGISwapChain->Present(1, 0);
}
