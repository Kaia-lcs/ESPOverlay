#include <Windows.h>
#include <d3d9.h>
#include <iostream>
#include <string>
#include <d3dx9.h> // Include for ID3DXLine
#include <TlHelp32.h> // Include for CreateToolhelp32Snapshot
#include <chrono> // For frame rate control

// Example offsets (replace with correct ones)
DWORD entityListBaseAddress = 0xDEADBEEF; 
DWORD entityHealthOffset = 0x100;
DWORD entityTeamOffset = 0x104;
DWORD entityPositionOffset = 0x108;

DWORD GetGameProcessId(const std::string& processName) {
    DWORD processId = 0;
    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapShot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(processEntry);
        if (Process32First(hSnapShot, &processEntry)) {
            do {
                if (processEntry.szExeFile == processName) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapShot, &processEntry));
        }
        CloseHandle(hSnapShot);
    }
    return processId;
}

DWORD FindEntityListBaseAddress(HANDLE hProcess) {
    // Replace with actual memory scanning logic or pattern scanning
    return 0xDEADBEEF; // Placeholder
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    char windowTitle[256];
    GetWindowTextA(hWnd, windowTitle, sizeof(windowTitle));
    std::string title(windowTitle);

    std::cout << "Found window: " << title << std::endl; // Add this for debugging

    if (title.find("GhostOfTsushima") != std::string::npos) { 
        *(HWND*)lParam = hWnd;
        return FALSE; 
    }
    return TRUE; 
}


LPDIRECT3D9 d3d;
LPDIRECT3DDEVICE9 d3ddev;
LPD3DXLINE pLine;

void InitD3D(HWND hWnd) {
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;
    d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &d3ddev);
    D3DXCreateLine(d3ddev, &pLine);
}

void DrawESPBox(float x, float y, float width, float height, D3DCOLOR color) {
    D3DXVECTOR2 lines[] = {
        { x, y }, { x + width, y },
        { x + width, y }, { x + width, y + height },
        { x + width, y + height }, { x, y + height },
        { x, y + height }, { x, y }
    };

    pLine->SetWidth(1.0f);
    pLine->Begin();
    pLine->Draw(lines, 8, color);
    pLine->End();
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SIZE:
        if (d3ddev != NULL) {
            d3ddev->Reset(&d3dpp); 
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}



// Function to enable debug privileges
BOOL EnableDebugPrivilege() {
    HANDLE hToken;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        std::cerr << "Error: Failed to open process token." << std::endl;
        return FALSE;
    }

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        std::cerr << "Error: Failed to lookup privilege value." << std::endl;
        CloseHandle(hToken);
        return FALSE;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {
        std::cerr << "Error: Failed to adjust token privileges." << std::endl;
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);
    return TRUE;
}



int main() {
    std::string gameProcessName = "GhostOfTsushima.exe"; 
    DWORD gameProcessId = GetGameProcessId(gameProcessName);
    if (gameProcessId == 0) {
        std::cout << "Game process not found." << std::endl;
        return 1;
    }
    
    // Enable debug privileges before opening the process
    if (!EnableDebugPrivilege()) {
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, gameProcessId);
    if (hProcess == NULL) {
        std::cout << "Failed to open process." << std::endl;
        return 1;
    }

    entityListBaseAddress = FindEntityListBaseAddress(hProcess);
    if (entityListBaseAddress == 0) {
        std::cout << "Failed to find entity list base address." << std::endl;
        CloseHandle(hProcess); 
        return 1;
    }

    HWND hWnd = NULL;
    EnumWindows(EnumWindowsProc, (LPARAM)&hWnd); 
    if (hWnd == NULL) {
        std::cout << "Failed to find game window." << std::endl;
        CloseHandle(hProcess); 
        return 1;
    }


    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "OverlayWindow", NULL };
    RegisterClassEx(&wc);
    hWnd = CreateWindowEx(WS_EX_TRANSPARENT | WS_EX_LAYERED, "OverlayWindow", NULL, WS_POPUP | WS_VISIBLE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);
    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_ALPHA);


    InitD3D(hWnd);
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);


    auto lastTime = std::chrono::high_resolution_clock::now();

    bool running = true; // Control the loop with a running flag

    while (running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedTime = currentTime - lastTime;
        lastTime = currentTime;

        double deltaTime = elapsedTime.count();

        d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        d3ddev->BeginScene();

        for (int i = 0; i < 100; i++) { 
            DWORD entityAddress = entityListBaseAddress + i * 0x100; 
            int health;
            int team;
            float x, y;


            if (!ReadProcessMemory(hProcess, (LPCVOID)(entityAddress + entityHealthOffset), &health, sizeof(int), nullptr)) {
                // Handle error, this could be caused by entity list changes, memory corruption, etc.
                std::cout << "Failed to read health for entity " << i << std::endl;
                continue;
            }
            if (!ReadProcessMemory(hProcess, (LPCVOID)(entityAddress + entityTeamOffset), &team, sizeof(int), nullptr)) {
                std::cout << "Failed to read team for entity " << i << std::endl;
                continue;
            }
            if (!ReadProcessMemory(hProcess, (LPCVOID)(entityAddress + entityPositionOffset), &x, sizeof(float), nullptr)) {
                std::cout << "Failed to read x position for entity " << i << std::endl;
                continue;
            }
            if (!ReadProcessMemory(hProcess, (LPCVOID)(entityAddress + entityPositionOffset + sizeof(float)), &y, sizeof(float), nullptr)) {
                std::cout << "Failed to read y position for entity " << i << std::endl;
                continue;
            }

            if (team != 1 && health > 0) {
                DrawESPBox(x, y, 50, 80, D3DCOLOR_ARGB(255, 255, 0, 0)); 
            }
        }

        d3ddev->EndScene();
        d3ddev->Present(NULL, NULL, NULL, NULL);

        // Exit condition for example - press the 'End' key to quit
        if (GetAsyncKeyState(VK_END)) {
            running = false; // Exit loop
        }

        // Maintain ~60 FPS
        if (deltaTime < (1.0 / 60.0)) {
            Sleep((DWORD)((1.0 / 60.0 - deltaTime) * 1000)); 
        }
    }

    // Cleanup
    pLine->Release();
    d3ddev->Release();
    d3d->Release();
    CloseHandle(hProcess); 

    return 0;
}