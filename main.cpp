#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include "resource.h"
#include "Localization.hpp"
#include "MainWindow.hpp"
#include "SafeWin32.hpp"

// Enlazar bibliotecas nativas de Windows
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR /*lpCmdLine*/,
    _In_ int nCmdShow
) {
    // Inicializar controles comunes nativos de Windows
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx(&icex);

    // Inicializar subsistema multilenguaje detectando idioma del SO
    RetroNotepad::Localization::Initialize();

    // Habilitar soporte de modo oscuro para controles y menús nativos en Windows 10/11
    RetroNotepad::Safe::SafeModule hUxTheme(::LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (hUxTheme.IsValid()) {
        using fnSetPreferredAppMode = int(WINAPI*)(int appMode);
        fnSetPreferredAppMode pSetPreferredAppMode =
            reinterpret_cast<fnSetPreferredAppMode>(::GetProcAddress(hUxTheme.Get(), MAKEINTRESOURCEA(135)));
        if (pSetPreferredAppMode) {
            pSetPreferredAppMode(1); // 1 = AllowDark
        }
    }

    RetroNotepad::MainWindow mainWindow;
    if (!mainWindow.Create(hInstance, nCmdShow)) {
        return 1;
    }

    // Procesar argumento de archivo desde la línea de comandos si fue proporcionado
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1) {
            mainWindow.OpenFileDirectly(argv[1]);
        }
        ::LocalFree(argv);
    }

    // Cargar tabla de aceleradores de teclado
    HACCEL hAccel = ::LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATOR));

    // Bucle principal de mensajes Win32 con filtrado modeless y aceleradores
    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        // Enrutamiento de mensajes para diálogos modeless de búsqueda y reemplazo
        if (mainWindow.PreTranslateMessage(&msg)) {
            continue;
        }

        // Procesamiento de aceleradores de teclado
        if (hAccel && ::TranslateAcceleratorW(mainWindow.GetHwnd(), hAccel, &msg)) {
            continue;
        }

        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
