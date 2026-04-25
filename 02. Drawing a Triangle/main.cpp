#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <assert.h>

// world->local->view->projection
// Interpolate in Rasterization
// 
// 창 크기 변경 감지용 플래그
static bool global_windowDidResize = false;

// 윈도우 메시지 처리 함수
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_KEYDOWN:
    {
        // ESC 누르면 종료
        if (wparam == VK_ESCAPE)
            DestroyWindow(hwnd);
        break;
    }
    case WM_DESTROY:
    {
        // 프로그램 종료 메시지
        PostQuitMessage(0);
        break;
    }
    case WM_SIZE:
    {
        // 창 크기 변경 시 swapchain 다시 만들어야 해서 체크
        global_windowDidResize = true;
        break;
    }
    default:
        result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // -------------------------
    // 1. 윈도우 생성
    // -------------------------
    HWND hwnd;
    {
        WNDCLASSEXW winClass = {};
        winClass.cbSize = sizeof(WNDCLASSEXW);
        winClass.style = CS_HREDRAW | CS_VREDRAW;
        winClass.lpfnWndProc = &WndProc;
        winClass.hInstance = hInstance;
        winClass.hCursor = LoadCursorW(0, IDC_ARROW);
        winClass.lpszClassName = L"MyWindowClass";

        RegisterClassExW(&winClass);

        // 원하는 클라이언트 영역 크기 설정
        RECT rect = { 0, 0, 1024, 768 };
        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);

        hwnd = CreateWindowExW(
            0,
            winClass.lpszClassName,
            L"Triangle",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            0, 0, hInstance, 0
        );
    }

    // -------------------------
    // 2. D3D11 디바이스 & 컨텍스트 생성
    // -------------------------
    ID3D11Device1* device;
    ID3D11DeviceContext1* context;
    {
        ID3D11Device* baseDevice;
        ID3D11DeviceContext* baseContext;

        // GPU 기능 레벨 설정
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

        // 디바이스 생성
        D3D11CreateDevice(
            0, D3D_DRIVER_TYPE_HARDWARE, 0,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, 1,
            D3D11_SDK_VERSION,
            &baseDevice, 0, &baseContext
        );

        // 11.1 인터페이스로 업캐스팅
        baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&device);
        baseContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&context);

        baseDevice->Release();
        baseContext->Release();
    }

    // -------------------------
    // 3. 스왑체인 생성 (화면 출력 버퍼)
    // -------------------------
    IDXGISwapChain1* swapChain;
    {
        IDXGIFactory2* factory;
        IDXGIDevice1* dxgiDevice;

        device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice);

        IDXGIAdapter* adapter;
        dxgiDevice->GetAdapter(&adapter);
        dxgiDevice->Release();

        adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
        adapter->Release();

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SampleDesc.Count = 1;

        // 윈도우에 연결된 스왑체인 생성
        factory->CreateSwapChainForHwnd(device, hwnd, &desc, 0, 0, &swapChain);
        factory->Release();
    }

    // -------------------------
    // 4. 렌더 타겟 (화면에 그릴 대상)
    // -------------------------
    ID3D11RenderTargetView* rtv;
    {
        ID3D11Texture2D* backBuffer;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

        // 백버퍼를 렌더 타겟으로 사용
        device->CreateRenderTargetView(backBuffer, 0, &rtv);
        backBuffer->Release();
    }

    // -------------------------
    // 5. 셰이더 컴파일 & 생성
    // -------------------------
    ID3DBlob* vsBlob;
    ID3D11VertexShader* vs;

    // vertex shader
    D3DCompileFromFile(L"shaders.hlsl", 0, 0, "vs_main", "vs_5_0", 0, 0, &vsBlob, 0);
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), 0, &vs);

    // pixel shader
    ID3DBlob* psBlob;
    ID3D11PixelShader* ps;
    D3DCompileFromFile(L"shaders.hlsl", 0, 0, "ps_main", "ps_5_0", 0, 0, &psBlob, 0);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), 0, &ps);

    // -------------------------
    // 6. 입력 레이아웃 (버텍스 구조 정의)
    // -------------------------
    ID3D11InputLayout* layout;
    {
        D3D11_INPUT_ELEMENT_DESC desc[] =
        {
            {"POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        device->CreateInputLayout(desc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout);
        vsBlob->Release();
    }

    // -------------------------
    // 7. 버텍스 버퍼 (삼각형 데이터)
    // -------------------------
    ID3D11Buffer* vb;
    UINT stride = 6 * sizeof(float);
    UINT offset = 0;
    UINT vertexCount;

    {
        float data[] =
        {
            0.0f,  0.5f, 0,1,0,1,
            0.5f, -0.5f, 1,0,0,1,
           -0.5f, -0.5f, 0,0,1,1
        };

        vertexCount = 3;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(data);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA init = { data };

        device->CreateBuffer(&desc, &init, &vb);
    }

    // -------------------------
    // 8. 메인 루프
    // -------------------------
    bool running = true;
    while (running)
    {
        // 메시지 처리
        MSG msg = {};
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                running = false;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 창 크기 변경 시 swapchain 재생성
        if (global_windowDidResize)
        {
            context->OMSetRenderTargets(0, 0, 0);
            rtv->Release();

            swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

            ID3D11Texture2D* backBuffer;
            swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
            device->CreateRenderTargetView(backBuffer, 0, &rtv);
            backBuffer->Release();

            global_windowDidResize = false;
        }

        // 화면 클리어 (배경색)
        float color[4] = { 0.1f, 0.2f, 0.6f, 1 };
        context->ClearRenderTargetView(rtv, color);

        // viewport 설정
        RECT r;
        GetClientRect(hwnd, &r);
        D3D11_VIEWPORT vp = { 0,0,(float)(r.right - r.left),(float)(r.bottom - r.top),0,1 };
        context->RSSetViewports(1, &vp);

        // 렌더 타겟 설정
        context->OMSetRenderTargets(1, &rtv, 0);

        // 파이프라인 설정
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(layout);
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

        context->VSSetShader(vs, 0, 0);
        context->PSSetShader(ps, 0, 0);

        // 그리기
        context->Draw(vertexCount, 0);

        // 화면 출력
        swapChain->Present(1, 0);
    }

    return 0;
}