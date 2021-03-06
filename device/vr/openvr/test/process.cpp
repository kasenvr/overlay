// #include <windows.h>
// #include <stdio.h>
// #include <fcntl.h>
// #include <io.h>
// #include <iostream>
// #include <fstream>

#include "device/vr/openvr/test/out.h"
#include "third_party/openvr/src/src/vrcommon/sharedlibtools_public.h"
#include "device/vr/openvr/test/fake_openvr_impl_api.h"

std::string logSuffix = "_process";
HWND g_hWnd = NULL;
bool live = true;

decltype(D3D11CreateDeviceAndSwapChain) *RealD3D11CreateDeviceAndSwapChain = nullptr;

inline uint32_t vtable_offset(HMODULE module, void *cls, unsigned int offset) {
	uintptr_t *vtable = *(uintptr_t **)cls;
	return (uint32_t)(vtable[offset] - (uintptr_t)module);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // getOut() << "WindowProc message " << message << " " << WM_DESTROY << std::endl;
  // sort through and find what code to run for the message given
  switch(message)
  {
      // this message is read when the window is closed
      case WM_DESTROY:
          {
              // close the application entirely
              live = false;
              PostQuitMessage(0);
              return 0;
          } break;
  }

  // Handle any messages the switch statement didn't
  return DefWindowProc (hWnd, message, wParam, lParam);
}

int WINAPI WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow
) {
  // SetProcessDPIAware();

  shMem = allocateShared("Local\\OpenVrProxyInit", 1024);
  g_offsets = (Offsets *)shMem;

  {
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "RT1";
    RegisterClassExA(&wc);

    g_hWnd = CreateWindowExA(
      NULL,
      wc.lpszClassName,    // name of the window class
      "Reality Tab",   // title of the window
      WS_OVERLAPPEDWINDOW,    // window style
      300,    // x-position of the window
      300,    // y-position of the window
      512,    // width of the window
      512,    // height of the window
      NULL,    // we have no parent window, NULL
      NULL,    // we aren't using menus, NULL
      hInstance,    // application handle
      NULL
    );    // used with multiple windows, NULL
    ShowWindow(g_hWnd, SW_SHOW);
  }
  {
    HMODULE d3d11Module = LoadLibraryA("d3d11.dll");
    if (!d3d11Module) {
      getOut() << "failed to load d3d11 module" << std::endl;
      abort();
    }
    HMODULE dxgiModule = LoadLibraryA("dxgi.dll");
    if (!dxgiModule) {
      getOut() << "failed to load dxgi module" << std::endl;
      abort();
    }
    
    ID3D11Device *deviceBasic;
    ID3D11DeviceContext *contextBasic;
    IDXGISwapChain *swapChain;

    D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_11_1
    };
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.Width = 2;
    swapChainDesc.BufferDesc.Height = 2;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = g_hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = true;

    RealD3D11CreateDeviceAndSwapChain = (decltype(D3D11CreateDeviceAndSwapChain) *)GetProcAddress(d3d11Module, "D3D11CreateDeviceAndSwapChain");
    HRESULT hr = RealD3D11CreateDeviceAndSwapChain(
      NULL, // pAdapter
      D3D_DRIVER_TYPE_HARDWARE, // DriverType
      NULL, // Software
      D3D11_CREATE_DEVICE_DEBUG, // Flags
      featureLevels, // pFeatureLevels
      ARRAYSIZE(featureLevels), // FeatureLevels
      D3D11_SDK_VERSION, // SDKVersion
      &swapChainDesc, // pSwapChainDesc,
      &swapChain, // ppSwapChain
      &deviceBasic, // ppDevice
      NULL, // pFeatureLevel
      &contextBasic // ppImmediateContext
    );
    
    IDXGISwapChain1 *swapChain1;
		hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&swapChain1);
    
    g_offsets->Present = vtable_offset(dxgiModule, swapChain, 8);
    g_offsets->Present1 = vtable_offset(dxgiModule, swapChain1, 22);
    
    getOut() << "offset " << g_offsets->Present << " " << g_offsets->Present1 << std::endl;
  }

  getOut() << "process start " << (void *)g_hWnd << " " << (void *)GetLastError() << std::endl;

  wrapExternalOpenVr([&]() -> void {
    // only look in the override
    getOut() << "got dll dir " << dllDir << std::endl;
    std::string openvrApiDllPath = dllDir + "openvr_api.dll";
    getOut() << "core 2 " << openvrApiDllPath << std::endl;
    void *pMod = SharedLib_Load(openvrApiDllPath.c_str());
    getOut() << "core 3 " << (void *)pMod << std::endl;
    // dumpbin /exports "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\vrclient_x64.dll"
    // nothing more to do if we can't load the DLL
    // getOut() << "core 3 " << pMod << std::endl;
    if( !pMod )
    {
      getOut() << "core abort" << std::endl;
      abort();
      // return vr::VRInitError_Init_VRClientDLLNotFound;
    }
    
    // getOut() << "core 4 " << pMod << std::endl;

    __imp_VR_GetGenericInterface = SharedLib_GetFunction( pMod, "VR_GetGenericInterface" );
    getOut() << "core 4" << std::endl;
    __imp_VR_IsInterfaceVersionVersion = SharedLib_GetFunction( pMod, "VR_IsInterfaceVersionVersion" );
    __imp_VR_GetInitToken = SharedLib_GetFunction( pMod, "VR_GetInitToken" );
    __imp_VR_IsInterfaceVersion = SharedLib_GetFunction( pMod, "VR_IsInterfaceVersion" );
    __imp_VR_InitInternal2 = SharedLib_GetFunction( pMod, "VR_InitInternal2" );
    __imp_VR_IsInterfaceVersionValid = SharedLib_GetFunction( pMod, "VR_IsInterfaceVersionValid" );
    // __imp_VR_ShutdownInternal = SharedLib_GetFunction( pMod, "VR_ShutdownInternal" );
    __imp_VR_IsHmdPresent = SharedLib_GetFunction( pMod, "VR_IsHmdPresent" );
    __imp_VR_GetVRInitErrorAsSymbol = SharedLib_GetFunction( pMod, "VR_GetVRInitErrorAsSymbol" );
    __imp_VR_GetVRInitErrorAsEnglishDescription = SharedLib_GetFunction( pMod, "VR_GetVRInitErrorAsEnglishDescription" );
    getOut() << "core 5" << std::endl;
    if (!__imp_VR_GetGenericInterface) {
      getOut() << "unload abort" << std::endl;
      // SharedLib_Unload( pMod );
      abort();
      // return vr::VRInitError_Init_FactoryNotFound;
    }
    
    // getOut() << "core 6 " << pMod << " " << __imp_VR_GetGenericInterface << std::endl;

    /* int nReturnCode = 0;
    g_pHmdSystem = static_cast< IVRClientCore * > ( fnFactory( vr::IVRClientCore_Version, &nReturnCode ) );
    if( !g_pHmdSystem )
    {
      SharedLib_Unload( pMod );
      return vr::VRInitError_Init_InterfaceNotFound;
    } */

    getOut() << "core 6" << std::endl;

    getOut() << "vr_init " << GetCurrentThreadId() << std::endl;
    vr::EVRInitError result;
    vr::VR_Init(&result, vr::VRApplication_Scene);
    if (result != vr::VRInitError_None) {
      getOut() << "vr_init failed" << std::endl;
      abort();
    }
    getOut() << "classes init 1" << std::endl;

    vr::g_vrsystem = vr::VRSystem();
    vr::g_vrcompositor = vr::VRCompositor();
    vr::g_vrchaperone = vr::VRChaperone();
    vr::g_vrchaperonesetup = vr::VRChaperoneSetup();
    vr::g_vroverlay = vr::VROverlay();
    vr::g_vrrendermodels = vr::VRRenderModels();
    vr::g_vrscreenshots = vr::VRScreenshots();
    vr::g_vrsettings = vr::VRSettings();
    vr::g_vrextendeddisplay = vr::VRExtendedDisplay();
    vr::g_vrapplications = vr::VRApplications();
    vr::g_vrinput = vr::VRInput();

    getOut() << "classes init 2" << std::endl;
  });

  g_fnp = new FnProxy();
  g_hijacker = new Hijacker(*g_fnp);
  vr::g_pvrsystem = new vr::PVRSystem(vr::g_vrsystem, *g_fnp);
  vr::g_pvrcompositor = new vr::PVRCompositor(vr::g_vrcompositor, *g_hijacker, *g_fnp);
  vr::g_pvrclientcore = new vr::PVRClientCore(vr::g_pvrcompositor, *g_fnp);
  vr::g_pvrinput = new vr::PVRInput(vr::g_vrinput, *g_fnp);
  vr::g_pvrscreenshots = new vr::PVRScreenshots(vr::g_vrscreenshots, *g_fnp);
  vr::g_pvrchaperone = new vr::PVRChaperone(vr::g_vrchaperone, *g_fnp);
  vr::g_pvrchaperonesetup = new vr::PVRChaperoneSetup(vr::g_vrchaperonesetup, *g_fnp);
  vr::g_pvrsettings = new vr::PVRSettings(vr::g_vrsettings, *g_fnp);
  vr::g_pvrrendermodels = new vr::PVRRenderModels(vr::g_vrrendermodels, *g_fnp);
  vr::g_pvrapplications = new vr::PVRApplications(vr::g_vrapplications, *g_fnp);
  vr::g_pvroverlay = new vr::PVROverlay(vr::g_vroverlay, *g_fnp);
  /* FnProxy fnp;
  Hijacker hijacker(fnp);
  vr::PVRSystem system(vr::g_vrsystem, fnp);
  vr::PVRCompositor compositor(vr::g_vrcompositor, hijacker, fnp);
  vr::PVRClientCore clientcore(&compositor, fnp);
  vr::PVRInput input(vr::g_vrinput, fnp);
  vr::PVRScreenshots screenshots(vr::g_vrscreenshots, fnp);
  vr::PVRChaperone chaperone(vr::g_vrchaperone, fnp);
  vr::PVRChaperoneSetup chaperonesetup(vr::g_vrchaperonesetup, fnp);
  vr::PVRSettings settings(vr::g_vrsettings, fnp);
  vr::PVRRenderModels rendermodels(vr::g_vrrendermodels, fnp);
  vr::PVRApplications applications(vr::g_vrapplications, fnp);
  vr::PVROverlay overlay(vr::g_vroverlay, fnp); */
  while (live) {
    DWORD result = MsgWaitForMultipleObjects(
      1,
      &g_fnp->inSem.h,
      false,
      INFINITE,
      QS_ALLEVENTS
    );
    if (result == WAIT_OBJECT_0) {
      g_fnp->handle();
    } else if (result == (WAIT_OBJECT_0 + 1)) {
      MSG msg;
      // wait for the next message in the queue, store the result in 'msg'
      if (GetMessage(&msg, NULL, 0, 0)) {
        // translate keystroke messages into the right format
        TranslateMessage(&msg);

        // send the message to the WindowProc function
        DispatchMessage(&msg);
      }
    } else {
      getOut() << "unknown message wake: " << (void *)result << std::endl;
      abort();
    }
  }
  
  getOut() << "process exit" << std::endl;

  return 0;
}