#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <assert.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "3DMaths.h"
#include "ObjLoading.h"

static bool global_windowDidResize = false;

// Input
enum GameAction {
    GameActionMoveCamFwd,
    GameActionMoveCamBack,
    GameActionMoveCamLeft,
    GameActionMoveCamRight,
    GameActionTurnCamLeft,
    GameActionTurnCamRight,
    GameActionLookUp,
    GameActionLookDown,
    GameActionRaiseCam,
    GameActionLowerCam,
    GameActionCount
};
static bool global_keyIsDown[GameActionCount] = {};

//
// ID3D11Device1* d3d11Device : GPU 리소스 생성 담당 객체.
// IDXGISwapChain1* swapChain : 화면 출력용 백버퍼 체인.
// ID3D11RenderTargetView** d3d11FrameBufferView : 생성된 RTV(출력 대상) 반환
// ID3D11DepthStencilView** depthBufferView : 생성된 DSV(픽셀의 깊이를 판단하는 버퍼) 반환
//
bool win32CreateD3D11RenderTargets(ID3D11Device1* d3d11Device, IDXGISwapChain1* swapChain, ID3D11RenderTargetView** d3d11FrameBufferView, ID3D11DepthStencilView** depthBufferView)
{
    // 스왑체인 백버퍼 가져오기.
    ID3D11Texture2D* d3d11FrameBuffer;
    HRESULT hResult = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
    assert(SUCCEEDED(hResult));

    // 백버퍼를 RenderTarget으로 사용할 View 생성.
    hResult = d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, 0, d3d11FrameBufferView);
    assert(SUCCEEDED(hResult));

    // 백버퍼 설정을 DepthBuffer용으로 복사
    D3D11_TEXTURE2D_DESC depthBufferDesc;
    d3d11FrameBuffer->GetDesc(&depthBufferDesc);

    // 백버퍼 COM 참조 해제 (RTV가 별도 참조 유지)
    d3d11FrameBuffer->Release();

    // Depth/Stencil 포맷으로 변경
    depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    
    // Depth Buffer Texture 생성
    ID3D11Texture2D* depthBuffer;
    d3d11Device->CreateTexture2D(&depthBufferDesc, nullptr, &depthBuffer);

    // 깊이 테스트(Depth Test : 3D에서 누가 앞에 있고 뒤에 있는지 판단하는 과정)용 버퍼로 쓰게 만드는 설정 함수.
    d3d11Device->CreateDepthStencilView(depthBuffer, nullptr, depthBufferView);

    // Depth Texture COM 참조 해제 (DSV가 내부 참조 유지)
    depthBuffer->Release();

    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch(msg)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            bool isDown = (msg == WM_KEYDOWN);
            if(wparam == VK_ESCAPE)
                DestroyWindow(hwnd);
            else if(wparam == 'W')
                global_keyIsDown[GameActionMoveCamFwd] = isDown;
            else if(wparam == 'A')
                global_keyIsDown[GameActionMoveCamLeft] = isDown;
            else if(wparam == 'S')
                global_keyIsDown[GameActionMoveCamBack] = isDown;
            else if(wparam == 'D')
                global_keyIsDown[GameActionMoveCamRight] = isDown;
            else if(wparam == 'E')
                global_keyIsDown[GameActionRaiseCam] = isDown;
            else if(wparam == 'Q')
                global_keyIsDown[GameActionLowerCam] = isDown;
            else if(wparam == VK_UP)
                global_keyIsDown[GameActionLookUp] = isDown;
            else if(wparam == VK_LEFT)
                global_keyIsDown[GameActionTurnCamLeft] = isDown;
            else if(wparam == VK_DOWN)
                global_keyIsDown[GameActionLookDown] = isDown;
            else if(wparam == VK_RIGHT)
                global_keyIsDown[GameActionTurnCamRight] = isDown;
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        case WM_SIZE:
        {
            global_windowDidResize = true;
            break;
        }
        default:
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nShowCmd*/)
{
    // Open a window
    HWND hwnd;
    {
        WNDCLASSEXW winClass = {};
        // 구조체 크기 지정.
        winClass.cbSize = sizeof(WNDCLASSEXW);
        // 창이 가로/세로로 크기 변경될 때 다시 그리기
        winClass.style = CS_HREDRAW | CS_VREDRAW;
        // 윈도우 메세지 처리 함수 (키보드, 마우스, 종료 등)
        winClass.lpfnWndProc = &WndProc;
        // 현재 프로그램 인스턴스 핸들
        winClass.hInstance = hInstance;
        // 기본 아이콘 설정.
        winClass.hIcon = LoadIconW(0, IDI_APPLICATION);
        // 마우스 커서 설정 (화살표)
        winClass.hCursor = LoadCursorW(0, IDC_ARROW);
        // 윈도우 클래스 이름 (중요: CreateWindow에서 사용)
        winClass.lpszClassName = L"MyWindowClass";
        // 작은 아이콘 설정
        winClass.hIconSm = LoadIconW(0, IDI_APPLICATION);

        // 윈도우 클래스 등록.
        if(!RegisterClassExW(&winClass)) {
            MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }

        // 클라이언트 영역 기준으로 원하는 해상도 설정
        RECT initialRect = { 0, 0, 1024, 768 };
        
        // 창 테두리/타이틀바 포함해서 실제 창 크기 계산
        AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
        LONG initialWidth = initialRect.right - initialRect.left;
        LONG initialHeight = initialRect.bottom - initialRect.top;

        // 윈도우 생성
        hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
                                winClass.lpszClassName,
                                L"10. Blinn-Phong Lighting",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                initialWidth, 
                                initialHeight,
                                0, 0, hInstance, 0);

        // 창 생성 실패 체크
        if(!hwnd) {
            MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
    }

    // Create D3D11 Device and Context
    ID3D11Device1* d3d11Device;
    ID3D11DeviceContext1* d3d11DeviceContext;
    {
        // 기본 D3D11 인터페이스로 생성.
        ID3D11Device* baseDevice;
        ID3D11DeviceContext* baseDeviceContext;

        // 사용할 Direct3D 기능 레벨 지정
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        
        // 디바이스 생성 옵션
        // BGRA 포맷 지원 활성화 (Direct2D 호환 등에 필요)
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        // 디버그 빌드에서는 Direct3D 디버그 레이어 활성화
        #if defined(DEBUG_BUILD)
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        // Driect3D 디바이스 및 디바이스 컨텍스트 생성
        HRESULT hResult = D3D11CreateDevice(
            0,                          // 기본 어댑터 사용
            D3D_DRIVER_TYPE_HARDWARE,   // 하드웨어 GPU 사용                           
            0,                          // 소프트웨어 레스터라이저 없음
            creationFlags,              // 생성 플래그
            featureLevels,              // 지원 기능 레벨 목록
            ARRAYSIZE(featureLevels),   // 기능 레벨 개수
            D3D11_SDK_VERSION,          // SDK 버전
            &baseDevice,                // 생성된 디바이스 반환
            0,                          // 실제 생성된 기능 레벨 (필요 없어서 null)
            &baseDeviceContext);        // 생성된 디바이스 컨텍스트 반환

        // 생성 실패 시 에러 메세지 출력 후 종료
        if(FAILED(hResult)){
            MessageBoxA(0, "D3D11CreateDevice() failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
        
        // 기본 ID3D11Device 인터페이스를 ID3D11Device1 인터페이스로 업그레이드(QueryInterface)
        // Get 1.1 interface of D3D11 Device and Context
        hResult = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&d3d11Device);
        assert(SUCCEEDED(hResult));
        baseDevice->Release();

        // 기본 디바이스 컨텍스트를 ID3D11DeviceContext1 인터페이스로 업그레이드
        hResult = baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&d3d11DeviceContext);
        assert(SUCCEEDED(hResult));
        
        // 기본 컨텍스트 인터페이스 해제
        baseDeviceContext->Release();
    }

#ifdef DEBUG_BUILD
    // Set up debug layer to break on D3D11 errors
    ID3D11Debug *d3dDebug = nullptr;
    d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
    if (d3dDebug)
    {
        ID3D11InfoQueue *d3dInfoQueue = nullptr;
        if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
        {
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            d3dInfoQueue->Release();
        }
        d3dDebug->Release();
    }
#endif
    
    // 화면 출력용 스왑 체인 인터페이스
    // Create Swap Chain
    IDXGISwapChain1* d3d11SwapChain;
    {
        // 스왑 체인을 생성하려면 DXGI Factory가 필요함
        // Get DXGI Factory (needed to create Swap Chain)
        IDXGIFactory2* dxgiFactory;
        {
            // D3D11 디바이스를 DXGI 디바이스 인터페이스로 변환
            IDXGIDevice1* dxgiDevice;
            HRESULT hResult = d3d11Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice);
            assert(SUCCEEDED(hResult));

            // DXGI 디바이스로부터 그래픽 어댑터(GPU) 가져오기
            IDXGIAdapter* dxgiAdapter;
            hResult = dxgiDevice->GetAdapter(&dxgiAdapter);
            assert(SUCCEEDED(hResult));
            dxgiDevice->Release();

            // 현재 사용 중인 GPU 정보 얻기
            DXGI_ADAPTER_DESC adapterDesc;
            dxgiAdapter->GetDesc(&adapterDesc);

            // 디버그 출력창에 GPU 이름 출력
            OutputDebugStringA("Graphics Device: ");
            OutputDebugStringW(adapterDesc.Description);

            // 어댑터의 부모 객체인 DXGI Factory 가져오기
            // 스왑 체인 생성에 필요함
            hResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
            assert(SUCCEEDED(hResult));
            dxgiAdapter->Release();
        }
        
        // 스왑 체인 설정 구조체
        DXGI_SWAP_CHAIN_DESC1 d3d11SwapChainDesc = {};

        // 0으로 설정하면 현재 윈도우 크기 사용
        d3d11SwapChainDesc.Width = 0; // use window width
        d3d11SwapChainDesc.Height = 0; // use window height

        // 백버퍼 포맷
        // BGRA 8비트 + sRGB 색공간 사용.
        d3d11SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        // 멀티샘플링(MSAA) 설정
        d3d11SwapChainDesc.SampleDesc.Count = 1;    // 샘플 개수
        d3d11SwapChainDesc.SampleDesc.Quality = 0;  // 품질 레벨

        // 백버퍼를 랜더 타겟으로 사용
        d3d11SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

        // 더블 버퍼링 사용
        d3d11SwapChainDesc.BufferCount = 2;
        
        // 화면 크기 변경 시 버퍼 스케일링 방식
        d3d11SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;

        // 프레젠트 방식 호환성 높음
        d3d11SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        // 알파 모드 사용 안 함
        d3d11SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        // 추가 옵션 없음
        d3d11SwapChainDesc.Flags = 0;

        // HAND 기반 스왑 체인 생성
        HRESULT hResult = dxgiFactory->CreateSwapChainForHwnd(
            d3d11Device,            // D3D11 디바이스
            hwnd,                   // 출력 대상 윈도우 핸들
            &d3d11SwapChainDesc,    // 스왑 체인 설정
            0,                      // 전체화면 설정 없음  
            0,                      // 출력 제한 없음
            &d3d11SwapChain);       // 생성된 스왑 체인 반환

        assert(SUCCEEDED(hResult));

        dxgiFactory->Release();
    }

    // Create Render Target and Depth Buffer
    ID3D11RenderTargetView* d3d11FrameBufferView;
    ID3D11DepthStencilView* depthBufferView;
    win32CreateD3D11RenderTargets(d3d11Device, d3d11SwapChain, &d3d11FrameBufferView, &depthBufferView);

    UINT shaderCompileFlags = 0;
    // Compiling with this flag allows debugging shaders with Visual Studio
    #if defined(DEBUG_BUILD)
    shaderCompileFlags |= D3DCOMPILE_DEBUG;
    #endif

    // Create Vertex Shader for rendering our lights
    ID3DBlob* lightVsCode;
    ID3D11VertexShader* lightVertexShader;
    {
        // HLSL 파일에서 버텍스 셰이더 컴파일
        ID3DBlob* compileErrors;
        HRESULT hResult = D3DCompileFromFile(
            L"Lights.hlsl",     // 셰이더 파일 경로
            nullptr,            // 매크로 정의 없음.
            nullptr,            // include 핸들러 없음.
            "vs_main",          // 진입 함수 이름 (entry point)
            "vs_5_0",           // 셰이더 모델 (Vertex Shader 5.0)
            shaderCompileFlags, // 컴파일 옵션
            0,                  // effect 플래그 없음
            &lightVsCode,       // 컴파일된 바이트코드 반환
            &compileErrors);    // 컴파일 에러 메세지 반환

        // 컴파일 실패 처리
        if(FAILED(hResult))
        {
            const char* errorString = NULL;

            // 파일 자체를 찾을 수 없는 경우
            if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";

            // HLSL 문법 오류 등의 컴파일 에러가 있는 경우
            else if(compileErrors){
                errorString = (const char*)compileErrors->GetBufferPointer();
            }
            // 에러 메세지 출력
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        // 컴파일된 바이트코드로 버텍스 셰이더 생성
        hResult = d3d11Device->CreateVertexShader(lightVsCode->GetBufferPointer(), lightVsCode->GetBufferSize(), nullptr, &lightVertexShader);
        assert(SUCCEEDED(hResult));
    }

    // Create Pixel Shader for rendering our lights
    ID3D11PixelShader* lightPixelShader;
    {
        // 컴파일된 픽셀 셰이더 바이트코드를 저장할 Blob
        ID3DBlob* psBlob;

        // 셰이더 컴파일 에러 메세지를 저장할 Blob
        ID3DBlob* compileErrors;

        // Lights.hlsl 파일의 ps_main 함수를 픽셀 셰이더로 컴파일
        HRESULT hResult = D3DCompileFromFile(
            L"Lights.hlsl",         // HLSL 파일 경로
            nullptr,                // 매크로 정의 없음
            nullptr,                // include 핸들러 없음
            "ps_main",              // 엔트리 포인트 함수 이름
            "ps_5_0",               // 픽셀 셰이더 모델 5.0
            shaderCompileFlags,     // 컴파일 옵션
            0,                      // effect 플래그 없음
            &psBlob,                // 컴파일된 셰이더 코드 변환
            &compileErrors);        // 에러 메세지 반환

        // 셰이더 컴파일 실패 처리
        if(FAILED(hResult))
        {
            const char* errorString = NULL;
            
            // 파일을 찾지 못한 경우
            if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            // HLSL 문법 오류 등 컴파일 에러 발생 시
            else if(compileErrors){
                errorString = (const char*)compileErrors->GetBufferPointer();
            }

            // 에러 메세지 박스 출력
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        // 컴파일된 셰이더 바이트코드로 픽셀 셰이더 생성
        hResult = d3d11Device->CreatePixelShader(
            psBlob->GetBufferPointer(), // 셰이더 코드 메모리 주소
            psBlob->GetBufferSize(),    // 셰이더 코드 크기
            nullptr,                    // 클래스 링크 없음   
            &lightPixelShader);         // 생성된 픽셀 셰이더 반환

        assert(SUCCEEDED(hResult));
        psBlob->Release();
    }

    // 광원(light) 버텍스 셰이더에 사용할 입력 레이아웃 객체
    // Create Input Layout for our light vertex shader
    ID3D11InputLayout* lightInputLayout;
    {
        // 버텍스 데이터 구조 정의
        // 셰이더 입력 시멘틱과 GPU 측 버텍스 구조를 연결함
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { 
                "POS",                          // HLSL 시멘틱 이름
                0,                              // 시멘틱 인덱스
                DXGI_FORMAT_R32G32B32_FLOAT,    // float3 (x, y, z)
                0,                              // 입력 슬롯 번호
                0,                              // 버텍스 시작 오프셋
                D3D11_INPUT_PER_VERTEX_DATA,    // 버텍스 단위 데이터
                0                               // 인스턴싱 사용 안함.
            }
        };

        // 입력 레이아웃 생성
        HRESULT hResult = d3d11Device->CreateInputLayout(
            inputElementDesc,                   // 입력 요소 배열
            ARRAYSIZE(inputElementDesc),        // 입력 요소 개수
            lightVsCode->GetBufferPointer(),    // 버텍스 셰이더 바이트코드
            lightVsCode->GetBufferSize(),       // 바이트코드 크기
            &lightInputLayout);                 // 생성된 입력 레이아웃 변환

        assert(SUCCEEDED(hResult));
        lightVsCode->Release();
    }

    // Create Vertex Shader for rendering our lit objects
    // Blinn-Phong 조명 모델용 버텍스 셰이더 바이트코드 저장
    ID3DBlob* blinnPhongVsCode;

    // 생성된 Blinn-Phong 버텍스 셰이더 객체
    ID3D11VertexShader* blinnPhongVertexShader;
    {   
        // 셰이더 컴파일 에러 메세지 저장용 Blob
        ID3DBlob* compileErrors;

        // BlinnPhong.hlsl 파일의 vs_main 함수를 버텍스 셰이더로 컴파일
        HRESULT hResult = D3DCompileFromFile(
            L"BlinnPhong.hlsl", // 셰이더 파일 경로
            nullptr,            // 매크로 정의 없음    
            nullptr,            // include 처리기 없음
            "vs_main",          // 엔트리 포인트 함수 이름
            "vs_5_0",           // vertex Shader Model 5.0
            shaderCompileFlags, // 컴파일 옵션
            0,                  // 추가 플래그 없음
            &blinnPhongVsCode,  // 컴파일된 셰이더 코드 반환
            &compileErrors);    // 컴파일 에러 메세지 반환

        // 셰이더 컴파일 실패 처리
        if(FAILED(hResult))
        {
            const char* errorString = NULL;
            if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if(compileErrors){
                errorString = (const char*)compileErrors->GetBufferPointer();
            }
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        // 컴파일된 바이트코드로 버테긋 셰이더 생성
        hResult = d3d11Device->CreateVertexShader(
            blinnPhongVsCode->GetBufferPointer(),   // 셰이더 코드 시작 주소
            blinnPhongVsCode->GetBufferSize(),      // 셰이더 코드 크기
            nullptr,                                // 클래스 링크 없음
            &blinnPhongVertexShader);               // 생성된 셰이더 반환

        assert(SUCCEEDED(hResult));
    }

    // Create Pixel Shader for rendering our lit objects
    // Blinn-Phong 조명 모델용 픽셀 셰이더 객체
    ID3D11PixelShader* blinnPhongPixelShader;
    {
        // 컴파일된 픽셀 셰이더 바이트코드 저장용 Blob
        ID3DBlob* psBlob;
        
        // 셰이더 컴파일 에러 메세지 저장용 Blob
        ID3DBlob* compileErrors;

        // BlinnPhong.hlsl 파일의 ps_main 함수를 픽셀 셰이더로 컴파일
        HRESULT hResult = D3DCompileFromFile(
            L"BlinnPhong.hlsl", // 셰이더 파일 경로
            nullptr,            // 매크로 정의 없음
            nullptr,            // include 처리기 없음
            "ps_main",          // 픽셀 셰이더 엔트리 포인트 함수
            "ps_5_0",           // Pixel Shader Model 5.0
            shaderCompileFlags, // 컴파일 옵션
            0,                  // 추가 플래그 없음
            &psBlob,            // 컴파일된 셰이더 코드 반환
            &compileErrors);       // 컴파일 에러 메세지 반환

        if(FAILED(hResult))
        {
            const char* errorString = NULL;
            if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if(compileErrors){
                errorString = (const char*)compileErrors->GetBufferPointer();
            }
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        // 컴파일된 바이트코드로 픽셀 셰이더 생성
        hResult = d3d11Device->CreatePixelShader(
            psBlob->GetBufferPointer(), // 셰이더 코드 메모리 주소
            psBlob->GetBufferSize(),    // 셰이더 코드 크기
            nullptr,                    // 클래스 링크 없음
            &blinnPhongPixelShader);    // 생성된 픽셀 셰이더 반환

        assert(SUCCEEDED(hResult));
        psBlob->Release();
    }

    // Create Input Layout for our Blinn-Phong vertex shader
    // Blinn-Phong 버텍스 셰이더에 사용할 입력 레이아웃 객체
    ID3D11InputLayout* blinnPhongInputLayout;
    {
        // 버텍스 데이터 레이아웃 정의
        // CPU 측 버텍스 구조와 HLSL 입력 시멘틱을 연결함
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            // 정점 위치(Position)
            { 
                "POS",                          // HLSL 시맨틱 이름
                0,                              // 시맨틱 인덱스
                DXGI_FORMAT_R32G32B32_FLOAT,    // float3 (x, y, z)
                0,                              // 입력 슬롯 번호
                0,                              // 시작 오프셋
                D3D11_INPUT_PER_VERTEX_DATA,    // 버텍스 단위 데이터
                0 
            },

            // 텍스처 좌표(texture UV)
            { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            // 법선 벡터(normal)
            { "NORM", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        // 입력 레이아웃 생성
        HRESULT hResult = d3d11Device->CreateInputLayout(
            inputElementDesc,                       // 입력 요소 배열
            ARRAYSIZE(inputElementDesc),            // 입력 요소 개수
            blinnPhongVsCode->GetBufferPointer(),   // 버텍스 셰이더 바이트코드
            blinnPhongVsCode->GetBufferSize(),      // 바이트코드 크기
            &blinnPhongInputLayout);                // 생성된 입력 레이아웃 반환

        assert(SUCCEEDED(hResult));
        blinnPhongVsCode->Release();
    }

    // Create Vertex and Index Buffer
    // 큐브 모델의 버텍스/인덱스 버퍼 및 랜더링 정보
    ID3D11Buffer* cubeVertexBuffer; // 버텍스 버퍼 (정점 데이터)
    ID3D11Buffer* cubeIndexBuffer;  // 인덱스 버퍼 (삼각형 인덱스)
    UINT cubeNumIndices;            // 인덱스 개수 (랜더링에 사용)
    UINT cubeStride;                // 한 정점의 크기 (byte 단위)
    UINT cubeOffset;                // 버텍스 시작 오프셋

    {
        // OBJ 파일 로드 (큐브 모델)
        LoadedObj obj = loadObj("cube.obj");

        // 한 정점이 차지하는 메모리 크기
        cubeStride = sizeof(VertexData);

        // 버텍스 시작 위치는 0
        cubeOffset = 0;

        // 전체 인덱스 개수 저장
        cubeNumIndices = obj.numIndices;

        // -------------------------
        // Vertex Buffer 생성
        // -------------------------

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = obj.numVertices * sizeof(VertexData);  // 전체 버텍스 크기
        vertexBufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;                 // 한 번 생성 후 변경 없음
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;              // 버텍스 버퍼로 사용

        // 초기 데이터 설정
        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { obj.vertexBuffer };

        // GPU 버텍스 버퍼 생성
        HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &cubeVertexBuffer);
        assert(SUCCEEDED(hResult));

        // -------------------------
        // Index Buffer 생성
        // -------------------------

        D3D11_BUFFER_DESC indexBufferDesc = {};
        indexBufferDesc.ByteWidth = obj.numIndices * sizeof(uint16_t);  // 인덱스 전체 크기
        indexBufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;              // 변경 없음
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;            // 인덱스 버퍼로 사용

        // 초기 인덱스 데이터
        D3D11_SUBRESOURCE_DATA indexSubresourceData = { obj.indexBuffer };

        // GPU 인덱스 버퍼 생성
        hResult = d3d11Device->CreateBuffer(&indexBufferDesc, &indexSubresourceData, &cubeIndexBuffer);
        assert(SUCCEEDED(hResult));
        freeLoadedObj(obj);
    }

    // Create Sampler State
    // 텍스쳐 샘플링 방식(필터링/ 주소 모드 등)을 정의하는 Sampler State
    ID3D11SamplerState* samplerState;
    {
        // 샘플러 상태 설정 구조체
        D3D11_SAMPLER_DESC samplerDesc = {};
        // 필터링 방식 : 
        // MIN/MAG/MIP 모두 포인트 샘플링 (가장 가까운 픽셀 사용 -> 픽셀 느낌 유지)
        samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;

        // UV가 0~1 범위를 벗어났을 때 처리 방식
        samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;

        // Border 색상 (uv가 밖으로 나가면 이 색이 사용됨)
        samplerDesc.BorderColor[0] = 1.0f; // R
        samplerDesc.BorderColor[1] = 1.0f; // G
        samplerDesc.BorderColor[2] = 1.0f; // B
        samplerDesc.BorderColor[3] = 1.0f; // A

        // 비교 함수 (Depth comparison sampling 등에 사용)
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

        // Sampler State 생성
        d3d11Device->CreateSamplerState(&samplerDesc, &samplerState);
    }
    
    // Load Image
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* testTextureBytes = stbi_load("test.png", &texWidth, &texHeight,
                                                &texNumChannels, texForceNumChannels);
    assert(testTextureBytes);
    int texBytesPerRow = 4 * texWidth;

    // Create Texture
    // 셰이더에서 사용할 텍스처 뷰 (Shader Resource View)
    ID3D11ShaderResourceView* textureView;
    {
        // 2D 텍스처 설명 구조체
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width              = texWidth;  // 텍스처 가로 크기
        textureDesc.Height             = texHeight; // 텍스처 세로 크기
        
        textureDesc.MipLevels          = 1; // 밉맵 없음 (1레벨만 사용)
        textureDesc.ArraySize          = 1; // 텍스처 배열 아님
        
        // SRGB 포맷 (감마 보정된 색상)
        textureDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        textureDesc.SampleDesc.Count   = 1; // MSAA 없음

        // CPU에서 초기 데이터를 넣고 이후 변경 안 함
        textureDesc.Usage              = D3D11_USAGE_IMMUTABLE;

        // 셰이더에서 읽을 수 있도록 설정
        textureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

        // -------------------------
        // 초기 텍스처 데이터 설정
        // -------------------------
        D3D11_SUBRESOURCE_DATA textureSubresourceData = {};

        textureSubresourceData.pSysMem = testTextureBytes;      // 실제 이미지 데이터
        textureSubresourceData.SysMemPitch = texBytesPerRow;    // 한 줄(row) 바이트 크기

        // -------------------------
        // GPU 텍스처 생성
        // -------------------------
        ID3D11Texture2D* texture;
        d3d11Device->CreateTexture2D(&textureDesc, &textureSubresourceData, &texture);
        
        // -------------------------
        // Shader Resource View 생성
        // (셰이더에서 texture 접근 가능하게 함)
        // -------------------------
        d3d11Device->CreateShaderResourceView(texture, nullptr, &textureView);
        texture->Release();
    }

    free(testTextureBytes);

    // ================================
    // Light Vertex Shader 상수 버퍼
    // ================================
    
    // Create Constant Buffer for our light vertex shader
    // 셰이더에 전달할 상수 데이터 구조
    struct LightVSConstants
    {
        float4x4 modelViewProj; // MVP 행렬 (월드+뷰+프로젝션 변환)
        float4 color;           // 라이트 색상
    };

    // 상수 버퍼 (GPU로 전달되는 작은 데이터 블록)
    ID3D11Buffer* lightVSConstantBuffer;
    {
        // 상수 버퍼 생성 설정
        D3D11_BUFFER_DESC constantBufferDesc = {};

        // Constant Buffer는 16바이트 정렬 필수
        // (Direct3D 규칙: ByteWidth는 16의 배수)
        // ByteWidth must be a multiple of 16, per the docs
        constantBufferDesc.ByteWidth      = sizeof(LightVSConstants) + 0xf & 0xfffffff0;

        constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC; // CPU에서 자주 업데이트
        constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        
        // CPU에서 Map()으로 쓰기 가능하게 설정
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        // GPU 상수 버퍼 생성
        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &lightVSConstantBuffer);
        assert(SUCCEEDED(hResult));
    }

    // ================================
    // Blinn-Phong Vertex Shader 상수 버퍼
    // ================================

    // Create Constant Buffer for our Blinn-Phong vertex shader
    struct BlinnPhongVSConstants
    {
        float4x4 modelViewProj; // MVP 행렬
        float4x4 modelView;     // Model-View 행렬
        float3x3 normalMatrix;  // 노멀 변환 행렬
    };

    ID3D11Buffer* blinnPhongVSConstantBuffer;
    {
        D3D11_BUFFER_DESC constantBufferDesc = {};
        
        // 16바이트 정렬 필수
        // ByteWidth must be a multiple of 16, per the docs
        constantBufferDesc.ByteWidth      = sizeof(BlinnPhongVSConstants) + 0xf & 0xfffffff0;
        constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;

        // CPU에서 매 프레임 업데이트 가능
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &blinnPhongVSConstantBuffer);
        assert(SUCCEEDED(hResult));
    }

    // ================================
    // Blinn-Phong Pixel Shader 상수 버퍼
    // ================================

    // 방향광 (Directional Light)
    struct DirectionalLight
    {
        // 조명 방향 (카메라 공간 기준, 빛이 오는 방향)
        float4 dirEye; //NOTE: Direction towards the light
        // 조명 색상
        float4 color;
    };

    // 점광원 (Point Light)
    struct PointLight
    {
        float4 posEye;  // 카메라 공간 기준 광원 위치
        float4 color;   // 광원 색상
    };

    // Create Constant Buffer for our Blinn-Phong pixel shader
    // 픽셀 셰이더에 전달할 상수 데이터 구조
    struct BlinnPhongPSConstants
    {
        DirectionalLight dirLight;  // 방향광 1개
        PointLight pointLights[2];  // 점광원 2개
    };

    // GPU 상수 버퍼
    ID3D11Buffer* blinnPhongPSConstantBuffer;
    {
        D3D11_BUFFER_DESC constantBufferDesc = {};

        // ByteWidth must be a multiple of 16, per the docs
        // Constant Buffer는 16바이트 정렬 필수
        constantBufferDesc.ByteWidth      = sizeof(BlinnPhongPSConstants) + 0xf & 0xfffffff0;

        constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;

        // CPU에서 Map()으로 쓰기 가능
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        // 상수 버퍼 생성
        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &blinnPhongPSConstantBuffer);
        assert(SUCCEEDED(hResult));
    }

    // ================================
    // Rasterizer State (래스터라이저 상태)
    // ================================

    ID3D11RasterizerState* rasterizerState;
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;     // 솔리드 렌더링
        rasterizerDesc.CullMode = D3D11_CULL_BACK;      // 백스페이스 컬링

        // 시계/반시계 방향 정의 (True면 CCW가 앞면)
        rasterizerDesc.FrontCounterClockwise = TRUE;

        d3d11Device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
    }

    // ================================
    // Depth / Stencil State
    // ================================

    ID3D11DepthStencilState* depthStencilState;
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable    = TRUE;                         // 깊이 테스트 활성화
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;   // 깊이 기록
        depthStencilDesc.DepthFunc      = D3D11_COMPARISON_LESS;        // 더 가까운 픽셀만 통과

        d3d11Device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
    }

    // ================================
    // Camera (카메라 상태)
    // ================================

    // Camera
    float3 cameraPos = {0, 0, 2};   // 카메라 위치
    float3 cameraFwd = {0, 0, -1};  // 카메라 전방 방향

    float cameraPitch = 0.f;        // 상하 회전
    float cameraYaw = 0.f;          // 좌우 회전

    // 투영 행렬 (Perspective Projection)
    float4x4 perspectiveMat = {};

    // 윈도우 크기 변경 시 재계산 강제
    global_windowDidResize = true; // To force initial perspectiveMat calculation

    // ================================
    // Timing (시간 측정)
    // ================================
    
    // Timing
    LONGLONG startPerfCount = 0;
    LONGLONG perfCounterFrequency = 0;
    {
        LARGE_INTEGER perfCount;
        QueryPerformanceCounter(&perfCount);
        startPerfCount = perfCount.QuadPart;
        LARGE_INTEGER perfFreq;
        QueryPerformanceFrequency(&perfFreq);
        perfCounterFrequency = perfFreq.QuadPart;
    }
    double currentTimeInSeconds = 0.0;

    // ================================
    // 메인 렌더링 루프
    // ================================

    // Main Loop
    bool isRunning = true;
    while(isRunning)
    {
        // -------------------------
        // Delta Time 계산 (dt)
        // -------------------------

        float dt;
        {
            double previousTimeInSeconds = currentTimeInSeconds;
            LARGE_INTEGER perfCount;
            QueryPerformanceCounter(&perfCount);

            currentTimeInSeconds = (double)(perfCount.QuadPart - startPerfCount) / (double)perfCounterFrequency;
            dt = (float)(currentTimeInSeconds - previousTimeInSeconds);

            // 프레임이 너무 길어지는 경우 clamp (물리 폭주 방지)
            if(dt > (1.f / 60.f))
                dt = (1.f / 60.f);
        }

        // -------------------------
        // Windows 메시지 처리
        // -------------------------

        MSG msg = {};
        while(PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT)
                isRunning = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        
        // -------------------------
        // 윈도우 크기 계산
        // -------------------------

        // Get window dimensions
        int windowWidth, windowHeight;
        float windowAspectRatio;
        {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            windowWidth = clientRect.right - clientRect.left;
            windowHeight = clientRect.bottom - clientRect.top;
            windowAspectRatio = (float)windowWidth / (float)windowHeight;
        }

        // -------------------------
        // 리사이즈 처리
        // -------------------------
        if(global_windowDidResize)
        {
            // 기존 렌더 타겟 해제
            d3d11DeviceContext->OMSetRenderTargets(0, 0, 0);
            d3d11FrameBufferView->Release();
            depthBufferView->Release();

            // 스왑체인 버퍼 재생성
            HRESULT res = d3d11SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            assert(SUCCEEDED(res));
            
            // 렌더 타겟 재생성
            win32CreateD3D11RenderTargets(d3d11Device, d3d11SwapChain, &d3d11FrameBufferView, &depthBufferView);
            // 투영 행렬 갱신
            perspectiveMat = makePerspectiveMat(windowAspectRatio, degreesToRadians(84), 0.1f, 1000.f);

            global_windowDidResize = false;
        }

        // ================================
        // Camera Update
        // ================================
        // 
        // Update camera
        {
            float3 camFwdXZ = normalise(float3{cameraFwd.x, 0, cameraFwd.z});
            float3 cameraRightXZ = cross(camFwdXZ, {0, 1, 0});

            const float CAM_MOVE_SPEED = 5.f; // in metres per second
            const float CAM_MOVE_AMOUNT = CAM_MOVE_SPEED * dt;

            // 이동
            if(global_keyIsDown[GameActionMoveCamFwd])
                cameraPos += camFwdXZ * CAM_MOVE_AMOUNT;
            if(global_keyIsDown[GameActionMoveCamBack])
                cameraPos -= camFwdXZ * CAM_MOVE_AMOUNT;
            if(global_keyIsDown[GameActionMoveCamLeft])
                cameraPos -= cameraRightXZ * CAM_MOVE_AMOUNT;
            if(global_keyIsDown[GameActionMoveCamRight])
                cameraPos += cameraRightXZ * CAM_MOVE_AMOUNT;
            if(global_keyIsDown[GameActionRaiseCam])
                cameraPos.y += CAM_MOVE_AMOUNT;
            if(global_keyIsDown[GameActionLowerCam])
                cameraPos.y -= CAM_MOVE_AMOUNT;
            
            // 회전
            const float CAM_TURN_SPEED = M_PI; // in radians per second
            const float CAM_TURN_AMOUNT = CAM_TURN_SPEED * dt;

            if(global_keyIsDown[GameActionTurnCamLeft])
                cameraYaw += CAM_TURN_AMOUNT;
            if(global_keyIsDown[GameActionTurnCamRight])
                cameraYaw -= CAM_TURN_AMOUNT;
            if(global_keyIsDown[GameActionLookUp])
                cameraPitch += CAM_TURN_AMOUNT;
            if(global_keyIsDown[GameActionLookDown])
                cameraPitch -= CAM_TURN_AMOUNT;

            // Wrap yaw to avoid floating-point errors if we turn too far
            // yaw wrap (floating point drift 방지)
            while(cameraYaw >= 2*M_PI) 
                cameraYaw -= 2*M_PI;
            while(cameraYaw <= -2*M_PI) 
                cameraYaw += 2*M_PI;

            // Clamp pitch to stop camera flipping upside down
            // pitch clamp (카메라 뒤집힘 방지)
            if(cameraPitch > degreesToRadians(85)) 
                cameraPitch = degreesToRadians(85);
            if(cameraPitch < -degreesToRadians(85)) 
                cameraPitch = -degreesToRadians(85);
        }

        // Calculate view matrix from camera data
        // 
        // float4x4 viewMat = inverse(rotateXMat(cameraPitch) * rotateYMat(cameraYaw) * translationMat(cameraPos));
        // NOTE: We can simplify this calculation to avoid inverse()!
        // Applying the rule inverse(A*B) = inverse(B) * inverse(A) gives:
        // float4x4 viewMat = inverse(translationMat(cameraPos)) * inverse(rotateYMat(cameraYaw)) * inverse(rotateXMat(cameraPitch));
        // The inverse of a rotation/translation is a negated rotation/translation:

        // ================================
        // View Matrix 계산
        // ================================

        float4x4 viewMat = translationMat(-cameraPos) * rotateYMat(-cameraYaw) * rotateXMat(-cameraPitch);
        float4x4 inverseViewMat = rotateXMat(cameraPitch) * rotateYMat(cameraYaw) * translationMat(cameraPos);
        // Update the forward vector we use for camera movement:
        cameraFwd = {-viewMat.m[2][0], -viewMat.m[2][1], -viewMat.m[2][2]};

        // ================================
        // Cube Transform 계산
        // ================================
        
        // Calculate matrices for cubes
        const int NUM_CUBES = 3;
        float4x4 cubeModelViewMats[NUM_CUBES];
        float3x3 cubeNormalMats[NUM_CUBES];
        {
            float3 cubePositions[NUM_CUBES] = {
                {0.f, 0.f, 0.f},
                {-3.f, 0.f, -1.5f},
                {4.5f, 0.2f, -3.f}
            };

            float modelXRotation = 0.2f * (float)(M_PI * currentTimeInSeconds);
            float modelYRotation = 0.1f * (float)(M_PI * currentTimeInSeconds);
            for(int i=0; i<NUM_CUBES; ++i)
            {
                modelXRotation += 0.6f*i; // Add an offset so cubes have different phases
                modelYRotation += 0.6f*i;
                float4x4 modelMat = rotateXMat(modelXRotation) * rotateYMat(modelYRotation) * translationMat(cubePositions[i]);
                float4x4 inverseModelMat = translationMat(-cubePositions[i]) * rotateYMat(-modelYRotation) * rotateXMat(-modelXRotation);
                cubeModelViewMats[i] = modelMat * viewMat;
                float4x4 inverseModelViewMat = inverseViewMat * inverseModelMat;
                cubeNormalMats[i] = float4x4ToFloat3x3(transpose(inverseModelViewMat));
            }
        }

        // ================================
        // Point Light 애니메이션
        // ================================
        
        // Move the point lights
        const int NUM_LIGHTS = 2;
        float4 lightColor[NUM_LIGHTS] = {
            {0.1f, 0.4f, 0.9f, 1.f},
            {0.9f, 0.1f, 0.6f, 1.f}
        };
        float4x4 lightModelViewMats[NUM_LIGHTS];
        float4 pointLightPosEye[NUM_LIGHTS];
        {
            float4 initialPointLightPositions[NUM_LIGHTS] = {
                {1, 0.5f, 0, 1},
                {-1, 0.7f, -1.2f, 1}
            };

            float lightRotation = -0.3f * (float)(M_PI * currentTimeInSeconds);
            for(int i=0; i<NUM_LIGHTS; ++i)
            {
                lightRotation += 0.5f*i; // Add an offset so lights have different phases
                                        
                lightModelViewMats[i] = scaleMat(0.2f) * translationMat(initialPointLightPositions[i].xyz) * rotateYMat(lightRotation) * viewMat;
                pointLightPosEye[i] = lightModelViewMats[i].cols[3];
            }
        }

        // ================================
        // Clear
        // ================================

        FLOAT backgroundColor[4] = { 0.1f, 0.2f, 0.6f, 1.0f };
        d3d11DeviceContext->ClearRenderTargetView(d3d11FrameBufferView, backgroundColor);
        
        d3d11DeviceContext->ClearDepthStencilView(depthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);

        // ================================
        // Pipeline Setup
        // ================================

        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)windowWidth, (FLOAT)windowHeight, 0.0f, 1.0f };
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        d3d11DeviceContext->RSSetState(rasterizerState);
        d3d11DeviceContext->OMSetDepthStencilState(depthStencilState, 0);

        d3d11DeviceContext->OMSetRenderTargets(1, &d3d11FrameBufferView, depthBufferView);

        d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d11DeviceContext->IASetVertexBuffers(0, 1, &cubeVertexBuffer, &cubeStride, &cubeOffset);
        d3d11DeviceContext->IASetIndexBuffer(cubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

        // ================================
        // Draw Lights
        // ================================
        {
            d3d11DeviceContext->IASetInputLayout(lightInputLayout);
            d3d11DeviceContext->VSSetShader(lightVertexShader, nullptr, 0);
            d3d11DeviceContext->PSSetShader(lightPixelShader, nullptr, 0);
            d3d11DeviceContext->VSSetConstantBuffers(0, 1, &lightVSConstantBuffer);

            for(int i=0; i<NUM_LIGHTS; ++i){
                // Update vertex shader constant buffer
                D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                d3d11DeviceContext->Map(lightVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
                LightVSConstants* constants = (LightVSConstants*)(mappedSubresource.pData);
                constants->modelViewProj = lightModelViewMats[i] * perspectiveMat;
                constants->color = lightColor[i];
                d3d11DeviceContext->Unmap(lightVSConstantBuffer, 0);

                d3d11DeviceContext->DrawIndexed(cubeNumIndices, 0, 0);
            }
        }
        
        // ================================
        // Draw Cubes (Blinn-Phong)
        // ================================
        {
            d3d11DeviceContext->IASetInputLayout(blinnPhongInputLayout);
            d3d11DeviceContext->VSSetShader(blinnPhongVertexShader, nullptr, 0);
            d3d11DeviceContext->PSSetShader(blinnPhongPixelShader, nullptr, 0);

            d3d11DeviceContext->PSSetShaderResources(0, 1, &textureView);
            d3d11DeviceContext->PSSetSamplers(0, 1, &samplerState);

            d3d11DeviceContext->VSSetConstantBuffers(0, 1, &blinnPhongVSConstantBuffer);
            d3d11DeviceContext->PSSetConstantBuffers(0, 1, &blinnPhongPSConstantBuffer);

            // Update pixel shader constant buffer
            // Pixel shader constants update
            {
                D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                d3d11DeviceContext->Map(blinnPhongPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
                BlinnPhongPSConstants* constants = (BlinnPhongPSConstants*)(mappedSubresource.pData);
                constants->dirLight.dirEye = normalise(float4{1.f, 1.f, 1.f, 0.f});
                constants->dirLight.color = {0.7f, 0.8f, 0.2f, 1.f};
                for(int i=0; i<NUM_LIGHTS; ++i){
                    constants->pointLights[i].posEye = pointLightPosEye[i];
                    constants->pointLights[i].color = lightColor[i];
                }
                d3d11DeviceContext->Unmap(blinnPhongPSConstantBuffer, 0);
            }

            for(int i=0; i<NUM_CUBES; ++i)
            {
                // Update vertex shader constant buffer
                D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                d3d11DeviceContext->Map(blinnPhongVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
                BlinnPhongVSConstants* constants = (BlinnPhongVSConstants*)(mappedSubresource.pData);
                constants->modelViewProj = cubeModelViewMats[i] * perspectiveMat;
                constants->modelView = cubeModelViewMats[i];
                constants->normalMatrix = cubeNormalMats[i];
                d3d11DeviceContext->Unmap(blinnPhongVSConstantBuffer, 0);

                d3d11DeviceContext->DrawIndexed(cubeNumIndices, 0, 0);
            }
        }
    
        // ================================
        // Present
        // ================================
        d3d11SwapChain->Present(1, 0);
    }

    return 0;
}
