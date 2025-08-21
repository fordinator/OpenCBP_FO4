#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#define SAFE_RELEASE(x) if((x)) {x->Release(); x = nullptr;}

class CVMTHook {
public:
    CVMTHook(DWORD_PTR** objPtr) {
        vtable = objPtr;
        originalVTable = *objPtr;
    }
    
    DWORD_PTR dwHookMethod(DWORD_PTR newFunc, int index) {
        DWORD_PTR originalFunc = originalVTable[index];
        DWORD oldProtect;
        VirtualProtect((LPVOID)(originalVTable + index), sizeof(DWORD_PTR), PAGE_EXECUTE_READWRITE, &oldProtect);
        originalVTable[index] = newFunc;
        VirtualProtect((LPVOID)(originalVTable + index), sizeof(DWORD_PTR), oldProtect, &oldProtect);
        return originalFunc;
    }
    
private:
    DWORD_PTR** vtable;
    DWORD_PTR* originalVTable;
};

enum LogLevel { logERROR };

class LogWrapper {
public:
    LogWrapper(LogLevel level) {}
    template<typename T>
    LogWrapper& operator<<(const T& value) { return *this; }
};

#define FILE_LOG(level) LogWrapper(level)

class DX11Manager {
public:
    bool bInitialized = false;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    CVMTHook* VMTSwapChainHook = nullptr;
    
    IDXGISwapChain* tempSwapChain = nullptr;
    ID3D11Device* tempDevice = nullptr;
    ID3D11DeviceContext* tempContext = nullptr;
    DWORD_PTR* pSwapChainVtable = nullptr;
    DWORD_PTR dwVTableAddress = 0;
    
    bool __stdcall HookPresent(IDXGISwapChain* swapChain);
    bool __stdcall CreateDeviceAndSwapChain();
    bool FindDynamicallySwapChain();
    void ReleaseCreatedDevice();
};