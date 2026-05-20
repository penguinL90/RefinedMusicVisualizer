#pragma once
#include "Vector2.h"
#include "NodeOptions.h"

struct DrawCore
{
    DrawCore(HWND hwnd, UINT width, UINT height, NodeOptions const& options);
    ~DrawCore() = default;
    DrawCore(const DrawCore&) = delete;
    DrawCore(DrawCore&&) = delete;
    DrawCore& operator=(const DrawCore&) = delete;
    DrawCore& operator=(DrawCore&&) = delete;

    void Draw(std::span<float> nodeValues);

private:
    struct CBuffer
    {
        float MaxLength;
        float MinLength;
        float Width;
        float Padding;
        DirectX::XMFLOAT4 Color;
        DirectX::XMMATRIX TransformMatrix;
    };
    NodeOptions options;
    CBuffer cbuffer;
    std::vector<Vector2> nodePositions;
    std::vector<float> nodePrevValues;
    std::vector<float> nodeDampingSpeeds;
    UINT width, height;
    HWND hwnd;
    UINT indices[9] = { 0, 1, 2, 3, 0xFFFFFFFF, 4, 5, 6, 7 };
    UINT indexCount = sizeof(indices) / sizeof(UINT);
    wil::com_ptr<IDXGIDevice> pDXGIDevice;
    wil::com_ptr<ID3D11DeviceContext> pD3DDeviceContext;
    wil::com_ptr<ID3D11Device> pD3DDevice;
    wil::com_ptr<ID3D11Buffer> pD3DSB_NodePositions;
    wil::com_ptr<ID3D11Buffer> pD3DSB_NodeValues;
    wil::com_ptr<ID3D11Buffer> pD3DSB_NodePrevValues;
    wil::com_ptr<ID3D11Buffer> pD3DConstantBuffer;
    wil::com_ptr<ID3D11Buffer> pD3DIndexBuffer;
    wil::com_ptr<ID3D11VertexShader> pD3DVertexShader;
    wil::com_ptr<ID3D11PixelShader> pD3DPixelShader;
    wil::com_ptr<ID3D11RenderTargetView> pD3DRenderTargetView;
    wil::com_ptr<IDXGISwapChain1> pDXGISwapChain;
    wil::com_ptr<IDCompositionDevice> pDCompDevice;
    wil::com_ptr<IDCompositionTarget> pDCompTarget;
    wil::com_ptr<IDCompositionVisual> pDCompVisual;
    wil::com_ptr<ID3D11ShaderResourceView> pD3D11SRV_Values;
    wil::com_ptr<ID3D11ShaderResourceView> pD3D11SRV_PrevValues;
    wil::com_ptr<ID3D11ShaderResourceView> pD3D11SRV_Positions;

    void initialize();
    void initNodes();
    void initD3DDevices();
    void initShaders();
    void initNodeBuffers();
    void initCBuffer();
    void initIndexBuffer();
    void initSwapChain();
    void initDComp();
};
