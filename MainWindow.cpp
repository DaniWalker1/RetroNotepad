#include "MainWindow.hpp"
#include "resource.h"
#include "Localization.hpp"
#include <shellapi.h>
#include <commdlg.h>
#include <strsafe.h>
#include <algorithm>
#include <dwmapi.h>
#include <uxtheme.h>

namespace RetroNotepad {

    constexpr UINT_PTR EDIT_SUBCLASS_ID = 1001;
    constexpr int IDC_MAIN_EDIT = 100;
    constexpr int IDC_MAIN_STATUSBAR = 101;

    namespace {
        enum PreferredAppMode {
            Default,
            AllowDark,
            ForceDark,
            ForceLight,
            Max
        };

        using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
        using fnFlushMenuThemes = void(WINAPI*)();

        void SetAppDarkModePreference(bool enableDark) {
            HMODULE hUxTheme = ::GetModuleHandleW(L"uxtheme.dll");
            if (!hUxTheme) {
                hUxTheme = ::LoadLibraryW(L"uxtheme.dll");
            }
            if (hUxTheme) {
                // Ordinal 135: SetPreferredAppMode
                fnSetPreferredAppMode pSetPreferredAppMode =
                    reinterpret_cast<fnSetPreferredAppMode>(::GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
                if (pSetPreferredAppMode) {
                    pSetPreferredAppMode(enableDark ? ForceDark : ForceLight);
                }

                // Ordinal 136: FlushMenuThemes
                fnFlushMenuThemes pFlushMenuThemes =
                    reinterpret_cast<fnFlushMenuThemes>(::GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));
                if (pFlushMenuThemes) {
                    pFlushMenuThemes();
                }
            }
        }

        // Mensajes y estructuras UAH (User Auto Handler) para barra de menús en modo oscuro
        constexpr UINT WM_UAHDESTROYWINDOW    = 0x0090;
        constexpr UINT WM_UAHDRAWMENU         = 0x0091;
        constexpr UINT WM_UAHDRAWMENUITEM     = 0x0092;
        constexpr UINT WM_UAHINITMENU         = 0x0093;
        constexpr UINT WM_UAHMEASUREMENUITEM  = 0x0094;
        constexpr UINT WM_UAHNCPAINTMENUPOPUP = 0x0095;

        typedef union tagUAHMENUITEMMETRICS {
            struct {
                DWORD cx;
                DWORD cy;
            } rgsizeBar[2];
            struct {
                DWORD cx;
                DWORD cy;
            } rgsizePopup[4];
        } UAHMENUITEMMETRICS;

        typedef struct tagUAHMENUPOPUPMETRICS {
            DWORD rgpadBar[4];
            DWORD rgpadPopup[4];
        } UAHMENUPOPUPMETRICS;

        typedef struct tagUAHMENUITEM {
            int iPosition;
            UAHMENUITEMMETRICS umim;
            UAHMENUPOPUPMETRICS umpm;
        } UAHMENUITEM;

        typedef struct tagUAHMENU {
            HMENU hmenu;
            HDC hdc;
            DWORD dwFlags;
        } UAHMENU;

        typedef struct tagUAHDRAWMENUITEM {
            DRAWITEMSTRUCT dis;
            UAHMENU um;
            UAHMENUITEM umi;
        } UAHDRAWMENUITEM;
    }

    MainWindow::MainWindow()
        : m_hwnd(nullptr)
        , m_hwndEdit(nullptr)
        , m_hwndStatusBar(nullptr)
        , m_hInstance(nullptr)
        , m_isModified(false)
        , m_encoding(Encoding::Utf8)
        , m_lineEnding(LineEnding::WindowsCRLF)
        , m_wordWrap(false)
        , m_showStatusBar(true)
        , m_themeMode(ThemeMode::System)
        , m_isCurrentDark(false)
        , m_zoomPercent(100)
    {
        // Paleta de colores oscura idéntica a Windows 11 Notepad (#202020)
        m_hDarkEditBgBrush.Reset(::CreateSolidBrush(RGB(32, 32, 32)));
        m_hLightEditBgBrush.Reset(::CreateSolidBrush(RGB(255, 255, 255)));
        m_hDarkHoverBrush.Reset(::CreateSolidBrush(RGB(55, 55, 55)));
        m_hDarkSepBrush.Reset(::CreateSolidBrush(RGB(65, 65, 65)));

        ZeroMemory(&m_logFont, sizeof(LOGFONTW));
        // Tipografía predeterminada de Windows 11 Notepad (Segoe UI Variable Text)
        m_logFont.lfHeight = -15; // Aproximadamente 11pt a 96 DPI
        m_logFont.lfWeight = FW_NORMAL;
        m_logFont.lfCharSet = DEFAULT_CHARSET;
        m_logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
        m_logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        m_logFont.lfQuality = CLEARTYPE_QUALITY;
        m_logFont.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
        StringCchCopyW(m_logFont.lfFaceName, LF_FACESIZE, L"Segoe UI Variable Text");
    }

    MainWindow::~MainWindow() {
        if (m_hwndEdit) {
            ::RemoveWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID);
        }
    }

    bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
        m_hInstance = hInstance;

        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WindowProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(MainWindow*);
        wcex.hInstance = hInstance;
        wcex.hIcon = ::LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        wcex.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
        wcex.lpszClassName = L"RetroNotepadWindowClass";
        wcex.hIconSm = ::LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

        if (!::RegisterClassExW(&wcex)) {
            return false;
        }

        const std::wstring title = std::wstring(Localization::Get(StringId::Untitled)) + L" - " + std::wstring(Localization::Get(StringId::AppTitle));

        m_hwnd = ::CreateWindowExW(
            WS_EX_ACCEPTFILES,
            L"RetroNotepadWindowClass",
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
            nullptr,
            nullptr,
            hInstance,
            this
        );

        if (!m_hwnd) {
            return false;
        }

        CreateControls();
        UpdateFont();
        UpdateLayout();
        UpdateStatusBarText();
        UpdateTitle();

        // Aplicar idioma y marcas de menú iniciales
        HMENU hMenu = ::GetMenu(m_hwnd);
        Localization::ApplyMenu(hMenu);
        ::CheckMenuItem(hMenu, IDM_VIEW_STATUSBAR, m_showStatusBar ? MF_CHECKED : MF_UNCHECKED);
        ::CheckMenuItem(hMenu, IDM_FORMAT_WORDWRAP, m_wordWrap ? MF_CHECKED : MF_UNCHECKED);

        ApplyTheme();

        ::ShowWindow(m_hwnd, nCmdShow);
        ::UpdateWindow(m_hwnd);
        ::SetFocus(m_hwndEdit);

        return true;
    }

    bool MainWindow::OpenFileDirectly(const std::wstring& path) {
        if (path.empty()) return false;
        DocumentData data;
        std::wstring error;
        if (EncodingDetector::LoadFile(path, data, error)) {
            m_filePath = path;
            m_encoding = data.encoding;
            m_lineEnding = data.lineEnding;
            m_isModified = false;

            ::SetWindowTextW(m_hwndEdit, data.text.c_str());
            ::SendMessageW(m_hwndEdit, EM_SETMODIFY, FALSE, 0);
            ::SendMessageW(m_hwndEdit, EM_EMPTYUNDOBUFFER, 0, 0);
            ::SendMessageW(m_hwndEdit, EM_SETSEL, 0, 0);

            UpdateTitle();
            UpdateStatusBarText();
            return true;
        }
        return false;
    }

    void MainWindow::CreateControls() {
        // Control EDIT
        DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
        if (!m_wordWrap) {
            editStyle |= WS_HSCROLL | ES_AUTOHSCROLL;
        }

        m_hwndEdit = ::CreateWindowExW(
            0,
            L"EDIT",
            L"",
            editStyle,
            0, 0, 0, 0,
            m_hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MAIN_EDIT)),
            m_hInstance,
            nullptr
        );

        if (m_hwndEdit) {
            ::SetWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
            // Desactivar límite de 32 KB por defecto de Win32 EDIT
            ::SendMessageW(m_hwndEdit, EM_SETLIMITTEXT, 0, 0);
        }

        // Barra de estado nativa msctls_statusbar32
        m_hwndStatusBar = ::CreateWindowExW(
            0,
            STATUSCLASSNAMEW,
            nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            m_hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MAIN_STATUSBAR)),
            m_hInstance,
            nullptr
        );

        UpdateStatusBarParts();
    }

    void MainWindow::RecreateEditControl(bool wordWrap) {
        if (!m_hwndEdit) return;

        // Guardar estado actual del texto, cursor y posición de scroll vertical
        int len = ::GetWindowTextLengthW(m_hwndEdit);
        std::wstring text(len + 1, L'\0');
        ::GetWindowTextW(m_hwndEdit, text.data(), len + 1);
        text.resize(len);

        DWORD startSel = 0, endSel = 0;
        ::SendMessageW(m_hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&startSel), reinterpret_cast<LPARAM>(&endSel));

        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        ::GetScrollInfo(m_hwndEdit, SB_VERT, &si);
        int topScrollPos = si.nPos;

        ::RemoveWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID);
        ::DestroyWindow(m_hwndEdit);
        m_hwndEdit = nullptr;

        m_wordWrap = wordWrap;

        DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
        if (!m_wordWrap) {
            editStyle |= WS_HSCROLL | ES_AUTOHSCROLL;
        }

        m_hwndEdit = ::CreateWindowExW(
            0,
            L"EDIT",
            text.c_str(),
            editStyle,
            0, 0, 0, 0,
            m_hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MAIN_EDIT)),
            m_hInstance,
            nullptr
        );

        if (m_hwndEdit) {
            ::SetWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
            ::SendMessageW(m_hwndEdit, EM_SETLIMITTEXT, 0, 0);
            ::SetWindowTheme(m_hwndEdit, m_isCurrentDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            if (m_hFont.IsValid()) {
                ::SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont.Get()), TRUE);
            }
            ::SendMessageW(m_hwndEdit, EM_SETSEL, startSel, endSel);
            if (topScrollPos > 0) {
                ::SendMessageW(m_hwndEdit, EM_LINESCROLL, 0, topScrollPos);
            } else {
                ::SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);
            }
            ::SetFocus(m_hwndEdit);
        }

        UpdateLayout();
    }

    void MainWindow::UpdateLayout() {
        if (!m_hwnd) return;

        RECT rcClient;
        ::GetClientRect(m_hwnd, &rcClient);

        int statusBarHeight = 0;
        if (m_showStatusBar && m_hwndStatusBar) {
            ::ShowWindow(m_hwndStatusBar, SW_SHOW);
            ::SendMessageW(m_hwndStatusBar, WM_SIZE, 0, 0);
            RECT rcStatus;
            ::GetWindowRect(m_hwndStatusBar, &rcStatus);
            statusBarHeight = rcStatus.bottom - rcStatus.top;
            UpdateStatusBarParts();
        } else if (m_hwndStatusBar) {
            ::ShowWindow(m_hwndStatusBar, SW_HIDE);
        }

        if (m_hwndEdit) {
            ::SetWindowPos(
                m_hwndEdit,
                nullptr,
                0, 0,
                rcClient.right,
                rcClient.bottom - statusBarHeight,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
    }

    void MainWindow::UpdateStatusBarParts() {
        if (!m_hwndStatusBar) return;

        RECT rc;
        ::GetClientRect(m_hwndStatusBar, &rc);
        const int width = rc.right;

        // Anchos de derecha a izquierda:
        // Part 4 (Codificación): 140 px
        // Part 3 (Salto de línea): 130 px
        // Part 2 (Zoom): 75 px
        // Part 1 (Conteo de caracteres): 170 px
        // Part 0 (Lín, Col): resto
        int partWidths[5];
        partWidths[4] = width;
        partWidths[3] = (std::max)(0, width - 140);
        partWidths[2] = (std::max)(0, partWidths[3] - 130);
        partWidths[1] = (std::max)(0, partWidths[2] - 75);
        partWidths[0] = (std::max)(0, partWidths[1] - 170);

        ::SendMessageW(m_hwndStatusBar, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(partWidths));
    }

    void MainWindow::UpdateStatusBarText() {
        if (!m_hwndStatusBar || !m_hwndEdit) return;

        // Panel 0: Lín %zu, Col %zu
        DWORD startSel = 0, endSel = 0;
        ::SendMessageW(m_hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&startSel), reinterpret_cast<LPARAM>(&endSel));
        LRESULT line = ::SendMessageW(m_hwndEdit, EM_LINEFROMCHAR, startSel, 0);
        LRESULT lineIndex = ::SendMessageW(m_hwndEdit, EM_LINEINDEX, line, 0);
        if (lineIndex < 0) lineIndex = 0;
        LRESULT col = (startSel >= static_cast<DWORD>(lineIndex)) ? (startSel - lineIndex) : 0;

        wchar_t lineColBuf[100];
        swprintf_s(lineColBuf, Localization::Get(StringId::StatusLineCol).data(), line + 1, col + 1);
        ::SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(lineColBuf));

        // Panel 1: Conteo de caracteres (total y selección interactiva)
        int totalChars = ::GetWindowTextLengthW(m_hwndEdit);
        wchar_t charBuf[128];
        if (startSel != endSel) {
            size_t selCount = (startSel < endSel) ? (endSel - startSel) : (startSel - endSel);
            swprintf_s(charBuf, Localization::Get(StringId::StatusCharsSelected).data(), selCount, static_cast<size_t>(totalChars));
        } else {
            if (totalChars == 1) {
                swprintf_s(charBuf, Localization::Get(StringId::StatusCharSingle).data(), static_cast<size_t>(totalChars));
            } else {
                swprintf_s(charBuf, Localization::Get(StringId::StatusChars).data(), static_cast<size_t>(totalChars));
            }
        }
        ::SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(charBuf));

        // Panel 2: Zoom %d%%
        wchar_t zoomBuf[50];
        swprintf_s(zoomBuf, Localization::Get(StringId::StatusZoom).data(), m_zoomPercent);
        ::SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(zoomBuf));

        // Panel 3: Formato de fin de línea
        std::wstring_view lineEndingName = EncodingDetector::GetLineEndingName(m_lineEnding);
        ::SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(lineEndingName.data()));

        // Panel 4: Codificación
        std::wstring_view encodingName = EncodingDetector::GetEncodingName(m_encoding);
        ::SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(encodingName.data()));
    }

    void MainWindow::UpdateTitle() {
        if (!m_hwnd) return;

        std::wstring docName;
        if (m_filePath.empty()) {
            docName = Localization::Get(StringId::Untitled);
        } else {
            size_t slashPos = m_filePath.find_last_of(L"\\/");
            if (slashPos != std::wstring::npos) {
                docName = m_filePath.substr(slashPos + 1);
            } else {
                docName = m_filePath;
            }
        }

        std::wstring fullTitle;
        if (m_isModified) {
            fullTitle += L"*";
        }
        fullTitle += docName;
        fullTitle += L" - ";
        fullTitle += Localization::Get(StringId::AppTitle);

        ::SetWindowTextW(m_hwnd, fullTitle.c_str());
    }

    void MainWindow::UpdateFont() {
        int dpi = 96;
        {
            Safe::SafeDC dc(m_hwnd, ::GetDC(m_hwnd));
            if (dc.IsValid()) {
                dpi = ::GetDeviceCaps(dc.Get(), LOGPIXELSY);
            }
        }

        LOGFONTW lf = m_logFont;
        // Escalar la altura de la fuente según el porcentaje de Zoom
        int baseHeight = lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
        int scaledHeight = MulDiv(baseHeight * m_zoomPercent, dpi, 72 * 100);
        // Evitar que la altura escale a 0 (0 hace que GDI revierta al tamaño de sistema de ~12pt)
        scaledHeight = (std::max)(1, scaledHeight);
        lf.lfHeight = -scaledHeight;

        m_hFont.Reset(::CreateFontIndirectW(&lf));

        if (m_hwndEdit && m_hFont.IsValid()) {
            ::SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont.Get()), TRUE);
            ::InvalidateRect(m_hwndEdit, nullptr, TRUE);
        }
    }

    void MainWindow::SetZoom(int zoomPercent) {
        // Límites seguros de zoom entre 10% y 500%
        m_zoomPercent = (std::clamp)(zoomPercent, 10, 500);
        UpdateFont();
        UpdateStatusBarText();
    }

    void MainWindow::ZoomIn() {
        SetZoom(m_zoomPercent + 10);
    }

    void MainWindow::ZoomOut() {
        SetZoom(m_zoomPercent - 10);
    }

    void MainWindow::ResetZoom() {
        SetZoom(100);
    }

    void MainWindow::SwitchLanguage(Language lang) {
        Localization::SetLanguage(lang);
        Localization::ApplyMenu(::GetMenu(m_hwnd));
        UpdateTitle();
        UpdateStatusBarText();
        UpdateThemeMenuChecks();
        ::DrawMenuBar(m_hwnd);
    }

    bool MainWindow::IsSystemDarkModeActive() const {
        DWORD value = 1;
        DWORD size = sizeof(value);
        LSTATUS status = ::RegGetValueW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme",
            RRF_RT_REG_DWORD,
            nullptr,
            &value,
            &size
        );
        if (status == ERROR_SUCCESS) {
            return value == 0;
        }
        return false;
    }

    void MainWindow::SetThemeMode(ThemeMode mode) {
        m_themeMode = mode;
        ApplyTheme();
    }

    void MainWindow::UpdateThemeMenuChecks() {
        HMENU hMenu = ::GetMenu(m_hwnd);
        if (!hMenu) return;
        ::CheckMenuItem(hMenu, IDM_VIEW_THEME_SYSTEM, (m_themeMode == ThemeMode::System) ? MF_CHECKED : MF_UNCHECKED);
        ::CheckMenuItem(hMenu, IDM_VIEW_THEME_LIGHT, (m_themeMode == ThemeMode::Light) ? MF_CHECKED : MF_UNCHECKED);
        ::CheckMenuItem(hMenu, IDM_VIEW_THEME_DARK, (m_themeMode == ThemeMode::Dark) ? MF_CHECKED : MF_UNCHECKED);
    }

    void MainWindow::ApplyTheme() {
        const bool shouldBeDark = (m_themeMode == ThemeMode::Dark) ||
                                  (m_themeMode == ThemeMode::System && IsSystemDarkModeActive());
        m_isCurrentDark = shouldBeDark;

        // 1. Título de ventana DWM (Immersive Dark Mode para Windows 10 y 11)
        BOOL useDark = shouldBeDark ? TRUE : FALSE;
        ::DwmSetWindowAttribute(m_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDark, sizeof(useDark));
        ::DwmSetWindowAttribute(m_hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &useDark, sizeof(useDark));

        // 2. Notificar al gestor de temas de menús nativos
        SetAppDarkModePreference(shouldBeDark);

        // 3. Estilo de ventana y controles
        if (m_hwndEdit) {
            ::SetWindowTheme(m_hwndEdit, shouldBeDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            ::InvalidateRect(m_hwndEdit, nullptr, TRUE);
        }

        if (m_hwndStatusBar) {
            ::SetWindowTheme(m_hwndStatusBar, shouldBeDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            ::SendMessageW(m_hwndStatusBar, SB_SETBKCOLOR, 0, shouldBeDark ? RGB(32, 32, 32) : CLR_DEFAULT);
            ::InvalidateRect(m_hwndStatusBar, nullptr, TRUE);
        }

        UpdateThemeMenuChecks();
        ::DrawMenuBar(m_hwnd);
        ::InvalidateRect(m_hwnd, nullptr, TRUE);
    }

    void MainWindow::DrawMenuNCBottomLine() {
        if (!m_isCurrentDark || !m_hwnd) return;

        MENUBARINFO mbi{};
        mbi.cbSize = sizeof(mbi);
        if (!::GetMenuBarInfo(m_hwnd, OBJID_MENU, 0, &mbi)) return;

        RECT rcClient{};
        ::GetClientRect(m_hwnd, &rcClient);
        POINT ptClient[2] = { { rcClient.left, rcClient.top }, { rcClient.right, rcClient.bottom } };
        ::MapWindowPoints(m_hwnd, nullptr, ptClient, 2);

        RECT rcWindow{};
        ::GetWindowRect(m_hwnd, &rcWindow);

        RECT rcAnnoyingLine;
        rcAnnoyingLine.left = ptClient[0].x - rcWindow.left;
        rcAnnoyingLine.right = ptClient[1].x - rcWindow.left;
        rcAnnoyingLine.top = ptClient[0].y - rcWindow.top - 1;
        rcAnnoyingLine.bottom = ptClient[0].y - rcWindow.top;

        Safe::SafeDC dc(m_hwnd, ::GetWindowDC(m_hwnd));
        if (dc.IsValid()) {
            ::FillRect(dc.Get(), &rcAnnoyingLine, m_hDarkEditBgBrush.Get());
        }
    }

    bool MainWindow::PromptToSaveChanges() {
        if (!m_isModified) return true;

        std::wstring docName = m_filePath.empty() ? std::wstring(Localization::Get(StringId::Untitled))
                                                  : m_filePath.substr(m_filePath.find_last_of(L"\\/") + 1);

        wchar_t prompt[512];
        swprintf_s(prompt, Localization::Get(StringId::SavePrompt).data(), docName.c_str());

        int result = ::MessageBoxW(
            m_hwnd,
            prompt,
            Localization::Get(StringId::AppTitle).data(),
            MB_YESNOCANCEL | MB_ICONWARNING
        );

        if (result == IDYES) {
            return DoFileSave();
        } else if (result == IDNO) {
            return true;
        } else {
            return false; // IDCANCEL
        }
    }

    bool MainWindow::DoFileNew() {
        if (!PromptToSaveChanges()) return false;

        ::SetWindowTextW(m_hwndEdit, L"");
        ::SendMessageW(m_hwndEdit, EM_SETMODIFY, FALSE, 0);
        ::SendMessageW(m_hwndEdit, EM_EMPTYUNDOBUFFER, 0, 0);
        m_filePath.clear();
        m_isModified = false;
        m_encoding = Encoding::Utf8;
        m_lineEnding = LineEnding::WindowsCRLF;

        UpdateTitle();
        UpdateStatusBarText();
        return true;
    }

    void MainWindow::DoFileNewWindow() {
        wchar_t exePath[MAX_PATH];
        if (::GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
            STARTUPINFOW si{};
            si.cb = sizeof(STARTUPINFOW);
            PROCESS_INFORMATION pi{};

            if (::CreateProcessW(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                ::CloseHandle(pi.hThread);
                ::CloseHandle(pi.hProcess);
            }
        }
    }

    bool MainWindow::DoFileOpen() {
        if (!PromptToSaveChanges()) return false;

        wchar_t fileBuffer[MAX_PATH] = L"";

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFile = fileBuffer;
        ofn.nMaxFile = MAX_PATH;

        // Construir filtro multilenguaje
        std::wstring filter;
        filter += Localization::Get(StringId::FilterTextFiles);
        filter.push_back(L'\0');
        filter += L"*.txt";
        filter.push_back(L'\0');
        filter += Localization::Get(StringId::FilterAllFiles);
        filter.push_back(L'\0');
        filter += L"*.*";
        filter.push_back(L'\0');
        filter.push_back(L'\0');

        ofn.lpstrFilter = filter.data();
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = L"txt";
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

        if (::GetOpenFileNameW(&ofn)) {
            DocumentData data;
            std::wstring error;
            if (EncodingDetector::LoadFile(fileBuffer, data, error)) {
                m_filePath = fileBuffer;
                m_encoding = data.encoding;
                m_lineEnding = data.lineEnding;
                m_isModified = false;

                ::SetWindowTextW(m_hwndEdit, data.text.c_str());
                ::SendMessageW(m_hwndEdit, EM_SETMODIFY, FALSE, 0);
                ::SendMessageW(m_hwndEdit, EM_EMPTYUNDOBUFFER, 0, 0);
                ::SendMessageW(m_hwndEdit, EM_SETSEL, 0, 0);

                UpdateTitle();
                UpdateStatusBarText();
                return true;
            } else {
                ::MessageBoxW(m_hwnd, error.c_str(), Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONERROR);
            }
        }

        return false;
    }

    bool MainWindow::DoFileSave() {
        if (m_filePath.empty()) {
            return DoFileSaveAs();
        }

        int len = ::GetWindowTextLengthW(m_hwndEdit);
        std::wstring text(len + 1, L'\0');
        ::GetWindowTextW(m_hwndEdit, text.data(), len + 1);
        text.resize(len);

        std::wstring error;
        if (EncodingDetector::SaveFile(m_filePath, text, m_encoding, m_lineEnding, error)) {
            m_isModified = false;
            ::SendMessageW(m_hwndEdit, EM_SETMODIFY, FALSE, 0);
            UpdateTitle();
            return true;
        } else {
            ::MessageBoxW(m_hwnd, error.c_str(), Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONERROR);
            return false;
        }
    }

    bool MainWindow::DoFileSaveAs() {
        wchar_t fileBuffer[MAX_PATH] = L"";
        if (!m_filePath.empty()) {
            StringCchCopyW(fileBuffer, MAX_PATH, m_filePath.c_str());
        }

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFile = fileBuffer;
        ofn.nMaxFile = MAX_PATH;

        std::wstring filter;
        filter += Localization::Get(StringId::FilterTextFiles);
        filter.push_back(L'\0');
        filter += L"*.txt";
        filter.push_back(L'\0');
        filter += Localization::Get(StringId::FilterAllFiles);
        filter.push_back(L'\0');
        filter += L"*.*";
        filter.push_back(L'\0');
        filter.push_back(L'\0');

        ofn.lpstrFilter = filter.data();
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = L"txt";
        ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (::GetSaveFileNameW(&ofn)) {
            m_filePath = fileBuffer;
            return DoFileSave();
        }

        return false;
    }

    void MainWindow::DoEditTimeDate() {
        SYSTEMTIME st;
        ::GetLocalTime(&st);

        wchar_t timeBuf[64];
        wchar_t dateBuf[64];

        ::GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, timeBuf, 64);
        ::GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, dateBuf, 64);

        wchar_t finalBuf[150];
        swprintf_s(finalBuf, L"%s %s", timeBuf, dateBuf);

        ::SendMessageW(m_hwndEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(finalBuf));
    }

    void MainWindow::DoSearchWithBing() {
        if (!m_hwndEdit) return;

        DWORD startSel = 0, endSel = 0;
        ::SendMessageW(m_hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&startSel), reinterpret_cast<LPARAM>(&endSel));

        if (endSel > startSel) {
            int len = ::GetWindowTextLengthW(m_hwndEdit);
            std::wstring text(len + 1, L'\0');
            ::GetWindowTextW(m_hwndEdit, text.data(), len + 1);
            text.resize(len);

            std::wstring query = text.substr(startSel, endSel - startSel);
            std::wstring url = L"https://www.bing.com/search?q=" + query;
            ::ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    INT_PTR CALLBACK MainWindow::GotoDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_INITDIALOG: {
                ::SetWindowTextW(hDlg, Localization::Get(StringId::GotoTitle).data());
                ::SetDlgItemTextW(hDlg, -1, Localization::Get(StringId::GotoPrompt).data());
                ::SetDlgItemTextW(hDlg, IDOK, Localization::Get(StringId::GotoBtn).data());
                ::SetDlgItemTextW(hDlg, IDCANCEL, Localization::Get(StringId::CancelBtn).data());

                HWND hwndEdit = reinterpret_cast<HWND>(lParam);
                ::SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(hwndEdit));

                DWORD startSel = 0;
                ::SendMessageW(hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&startSel), 0);
                LRESULT curLine = ::SendMessageW(hwndEdit, EM_LINEFROMCHAR, startSel, 0) + 1;

                wchar_t lineStr[32];
                swprintf_s(lineStr, L"%zd", curLine);
                ::SetDlgItemTextW(hDlg, IDC_GOTO_LINE, lineStr);
                ::SendDlgItemMessageW(hDlg, IDC_GOTO_LINE, EM_SETSEL, 0, -1);
                return TRUE;
            }
            case WM_COMMAND: {
                int id = LOWORD(wParam);
                if (id == IDOK) {
                    wchar_t buf[32];
                    ::GetDlgItemTextW(hDlg, IDC_GOTO_LINE, buf, 32);
                    int targetLine = _wtoi(buf);

                    HWND hwndEdit = reinterpret_cast<HWND>(::GetWindowLongPtrW(hDlg, DWLP_USER));
                    LRESULT totalLines = ::SendMessageW(hwndEdit, EM_GETLINECOUNT, 0, 0);

                    if (targetLine <= 0 || targetLine > totalLines) {
                        ::MessageBoxW(hDlg, Localization::Get(StringId::LineOutOfRange).data(),
                                      Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONWARNING);
                        return TRUE;
                    }

                    LRESULT charIdx = ::SendMessageW(hwndEdit, EM_LINEINDEX, targetLine - 1, 0);
                    ::SendMessageW(hwndEdit, EM_SETSEL, charIdx, charIdx);
                    ::SendMessageW(hwndEdit, EM_SCROLLCARET, 0, 0);

                    ::EndDialog(hDlg, IDOK);
                    return TRUE;
                } else if (id == IDCANCEL) {
                    ::EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }
                break;
            }
        }
        return FALSE;
    }

    void MainWindow::DoGoToLine() {
        if (!m_hwndEdit) return;
        ::DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_GOTO), m_hwnd, GotoDialogProc, reinterpret_cast<LPARAM>(m_hwndEdit));
    }

    void MainWindow::DoChooseFont() {
        CHOOSEFONTW cf{};
        cf.lStructSize = sizeof(CHOOSEFONTW);
        cf.hwndOwner = m_hwnd;
        cf.lpLogFont = &m_logFont;
        cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST;

        if (::ChooseFontW(&cf)) {
            UpdateFont();
        }
    }

    INT_PTR CALLBACK MainWindow::AboutDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = reinterpret_cast<MainWindow*>(::GetWindowLongPtrW(hDlg, DWLP_USER));

        switch (uMsg) {
            case WM_INITDIALOG: {
                pThis = reinterpret_cast<MainWindow*>(lParam);
                ::SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));

                // Configurar icono en el diálogo
                if (pThis) {
                    HICON hIcon = ::LoadIconW(pThis->m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
                    if (hIcon) {
                        ::SendDlgItemMessageW(hDlg, IDC_ABOUT_ICON, STM_SETICON, reinterpret_cast<WPARAM>(hIcon), 0);
                    }
                }

                // Ajustar textos según idioma
                const bool es = (Localization::GetLanguage() == Language::Spanish);
                ::SetWindowTextW(hDlg, es ? L"Acerca de Retro Notepad" : L"About Retro Notepad");
                ::SetDlgItemTextW(hDlg, IDC_ABOUT_TITLE, L"Retro Notepad");
                ::SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, es ? L"Versión 1.0 (64-bit)" : L"Version 1.0 (64-bit)");
                ::SetDlgItemTextW(hDlg, IDC_ABOUT_DEV, es ? L"Desarrollador: Daniel Valenzuela" : L"Developer: Daniel Valenzuela");
                ::SetDlgItemTextW(hDlg, IDC_ABOUT_DESC, es
                    ? L"Clon 1:1 de Bloc de notas desarrollado en C++20 nativo con Win32 API pura.\n\nUltra ligero, de alta velocidad, seguro y 100% autónomo (Zero-Dependency)."
                    : L"1:1 Notepad clone developed in native C++20 with pure Win32 API.\n\nUltra lightweight, high speed, secure and 100% self-contained (Zero-Dependency).");
                ::SetDlgItemTextW(hDlg, IDOK, es ? L"Aceptar" : L"OK");

                // Tema oscuro para el diálogo About
                if (pThis && pThis->m_isCurrentDark) {
                    BOOL useDark = TRUE;
                    ::DwmSetWindowAttribute(hDlg, 20, &useDark, sizeof(useDark));
                    ::DwmSetWindowAttribute(hDlg, 19, &useDark, sizeof(useDark));
                }
                return TRUE;
            }

            case WM_CTLCOLORDLG:
            case WM_CTLCOLORSTATIC: {
                if (pThis && pThis->m_isCurrentDark) {
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    ::SetTextColor(hdc, RGB(230, 230, 230));
                    ::SetBkColor(hdc, RGB(32, 32, 32));
                    return reinterpret_cast<LRESULT>(pThis->m_hDarkEditBgBrush.Get());
                }
                break;
            }

            case WM_COMMAND: {
                int id = LOWORD(wParam);
                if (id == IDOK || id == IDCANCEL) {
                    ::EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                break;
            }
        }
        return FALSE;
    }

    void MainWindow::DoAbout() {
        ::DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_ABOUT), m_hwnd, AboutDialogProc, reinterpret_cast<LPARAM>(this));
    }

    void MainWindow::ToggleWordWrap() {
        RecreateEditControl(!m_wordWrap);
        HMENU hMenu = ::GetMenu(m_hwnd);
        ::CheckMenuItem(hMenu, IDM_FORMAT_WORDWRAP, m_wordWrap ? MF_CHECKED : MF_UNCHECKED);
    }

    void MainWindow::ToggleStatusBar() {
        m_showStatusBar = !m_showStatusBar;
        HMENU hMenu = ::GetMenu(m_hwnd);
        ::CheckMenuItem(hMenu, IDM_VIEW_STATUSBAR, m_showStatusBar ? MF_CHECKED : MF_UNCHECKED);
        UpdateLayout();
        UpdateStatusBarText();
    }

    bool MainWindow::PreTranslateMessage(MSG* pMsg) {
        HWND hFindDlg = m_findReplace.GetDialogHandle();
        if (hFindDlg && ::IsDialogMessageW(hFindDlg, pMsg)) {
            return true;
        }
        return false;
    }

    LRESULT CALLBACK MainWindow::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData) {
        MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);

        switch (uMsg) {
            case WM_MOUSEWHEEL: {
                if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
                    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    if (delta > 0) {
                        pThis->ZoomIn();
                    } else if (delta < 0) {
                        pThis->ZoomOut();
                    }
                    return 0;
                }
                break;
            }
            case WM_MOUSEMOVE: {
                LRESULT res = ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
                if (wParam & MK_LBUTTON) {
                    pThis->UpdateStatusBarText();
                }
                return res;
            }
            case WM_LBUTTONUP:
            case WM_KEYUP: {
                LRESULT res = ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
                pThis->UpdateStatusBarText();
                return res;
            }
        }

        return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }

    LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
            pThis->m_hwnd = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<MainWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }

        return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        // Interceptar mensajes registrados de FindText / ReplaceText
        if (uMsg == m_findReplace.GetFindMessageId()) {
            m_findReplace.HandleMessage(m_hwnd, m_hwndEdit, uMsg, wParam, lParam);
            return 0;
        }

        switch (uMsg) {
            case WM_COMMAND: {
                int wmId = LOWORD(wParam);
                int wmEvent = HIWORD(wParam);

                if (lParam == reinterpret_cast<LPARAM>(m_hwndEdit) && wmEvent == EN_CHANGE) {
                    BOOL isEditMod = static_cast<BOOL>(::SendMessageW(m_hwndEdit, EM_GETMODIFY, 0, 0));
                    if (m_isModified != (isEditMod != FALSE)) {
                        m_isModified = (isEditMod != FALSE);
                        UpdateTitle();
                    }
                    UpdateStatusBarText();
                    return 0;
                }

                switch (wmId) {
                    // Archivo
                    case IDM_FILE_NEW:
                        DoFileNew();
                        return 0;
                    case IDM_FILE_NEW_WINDOW:
                        DoFileNewWindow();
                        return 0;
                    case IDM_FILE_OPEN:
                        DoFileOpen();
                        return 0;
                    case IDM_FILE_SAVE:
                        DoFileSave();
                        return 0;
                    case IDM_FILE_SAVEAS:
                        DoFileSaveAs();
                        return 0;
                    case IDM_FILE_PRINT: {
                        PRINTDLGW pd{};
                        pd.lStructSize = sizeof(PRINTDLGW);
                        pd.hwndOwner = m_hwnd;
                        pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
                        if (::PrintDlgW(&pd)) {
                            Safe::SafeDeleteDC printerDC(pd.hDC);
                            Safe::SafeGlobalMem devMode(pd.hDevMode);
                            Safe::SafeGlobalMem devNames(pd.hDevNames);
                            // Los recursos quedan encapsulados y se liberarán automáticamente vía RAII
                        } else {
                            if (pd.hDevMode) ::GlobalFree(pd.hDevMode);
                            if (pd.hDevNames) ::GlobalFree(pd.hDevNames);
                        }
                        return 0;
                    }
                    case IDM_FILE_EXIT:
                        ::SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
                        return 0;

                    // Edición
                    case IDM_EDIT_UNDO:
                        ::SendMessageW(m_hwndEdit, EM_UNDO, 0, 0);
                        return 0;
                    case IDM_EDIT_CUT:
                        ::SendMessageW(m_hwndEdit, WM_CUT, 0, 0);
                        return 0;
                    case IDM_EDIT_COPY:
                        ::SendMessageW(m_hwndEdit, WM_COPY, 0, 0);
                        return 0;
                    case IDM_EDIT_PASTE:
                        ::SendMessageW(m_hwndEdit, WM_PASTE, 0, 0);
                        return 0;
                    case IDM_EDIT_DELETE:
                        ::SendMessageW(m_hwndEdit, WM_CLEAR, 0, 0);
                        return 0;
                    case IDM_EDIT_SEARCH_BING:
                        DoSearchWithBing();
                        return 0;
                    case IDM_EDIT_FIND:
                        m_findReplace.ShowFindDialog(m_hwnd);
                        return 0;
                    case IDM_EDIT_FIND_NEXT:
                        m_findReplace.FindNext(m_hwnd, m_hwndEdit, true);
                        return 0;
                    case IDM_EDIT_FIND_PREV:
                        m_findReplace.FindNext(m_hwnd, m_hwndEdit, false);
                        return 0;
                    case IDM_EDIT_REPLACE:
                        m_findReplace.ShowReplaceDialog(m_hwnd);
                        return 0;
                    case IDM_EDIT_GOTO:
                        DoGoToLine();
                        return 0;
                    case IDM_EDIT_SELECTALL:
                        ::SendMessageW(m_hwndEdit, EM_SETSEL, 0, -1);
                        return 0;
                    case IDM_EDIT_TIMEDATE:
                        DoEditTimeDate();
                        return 0;

                    // Formato
                    case IDM_FORMAT_WORDWRAP:
                        ToggleWordWrap();
                        return 0;
                    case IDM_FORMAT_FONT:
                        DoChooseFont();
                        return 0;

                    // Ver
                    case IDM_VIEW_ZOOM_IN:
                        ZoomIn();
                        return 0;
                    case IDM_VIEW_ZOOM_OUT:
                        ZoomOut();
                        return 0;
                    case IDM_VIEW_ZOOM_RESET:
                        ResetZoom();
                        return 0;
                    case IDM_VIEW_STATUSBAR:
                        ToggleStatusBar();
                        return 0;

                    // Multilenguaje
                    case IDM_VIEW_LANG_ES:
                        SwitchLanguage(Language::Spanish);
                        return 0;
                    case IDM_VIEW_LANG_EN:
                        SwitchLanguage(Language::English);
                        return 0;

                    // Tema (Light/Dark acorde al sistema)
                    case IDM_VIEW_THEME_SYSTEM:
                        SetThemeMode(ThemeMode::System);
                        return 0;
                    case IDM_VIEW_THEME_LIGHT:
                        SetThemeMode(ThemeMode::Light);
                        return 0;
                    case IDM_VIEW_THEME_DARK:
                        SetThemeMode(ThemeMode::Dark);
                        return 0;

                    // Ayuda
                    case IDM_HELP_VIEWHELP:
                        ::ShellExecuteW(nullptr, L"open", L"https://go.microsoft.com/fwlink/?LinkId=517009", nullptr, nullptr, SW_SHOWNORMAL);
                        return 0;
                    case IDM_HELP_FEEDBACK:
                        ::ShellExecuteW(nullptr, L"open", L"https://windows.com/feedback", nullptr, nullptr, SW_SHOWNORMAL);
                        return 0;
                    case IDM_HELP_ABOUT:
                        DoAbout();
                        return 0;
                }
                break;
            }

            case WM_ERASEBKGND: {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                RECT rc;
                ::GetClientRect(m_hwnd, &rc);
                ::FillRect(hdc, &rc, m_isCurrentDark ? m_hDarkEditBgBrush.Get() : m_hLightEditBgBrush.Get());
                return 1;
            }

            case WM_UAHDRAWMENU: {
                if (m_isCurrentDark) {
                    UAHMENU* pUDM = reinterpret_cast<UAHMENU*>(lParam);
                    MENUBARINFO mbi{};
                    mbi.cbSize = sizeof(mbi);
                    if (::GetMenuBarInfo(m_hwnd, OBJID_MENU, 0, &mbi)) {
                        RECT rcWindow;
                        ::GetWindowRect(m_hwnd, &rcWindow);
                        RECT rcBar = mbi.rcBar;
                        ::OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
                        ::FillRect(pUDM->hdc, &rcBar, m_hDarkEditBgBrush.Get());
                        return 0;
                    }
                }
                break;
            }

            case WM_UAHDRAWMENUITEM: {
                if (m_isCurrentDark) {
                    UAHDRAWMENUITEM* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
                    HDC hdc = pUDMI->dis.hDC ? pUDMI->dis.hDC : pUDMI->um.hdc;
                    RECT rcItem = pUDMI->dis.rcItem;

                    DWORD state = pUDMI->dis.itemState;
                    bool isHot = (state & (ODS_HOTLIGHT | ODS_SELECTED)) != 0;

                    if (isHot) {
                        ::FillRect(hdc, &rcItem, m_hDarkHoverBrush.Get());
                    } else {
                        ::FillRect(hdc, &rcItem, m_hDarkEditBgBrush.Get());
                    }

                    wchar_t text[128]{};
                    MENUITEMINFOW mii{};
                    mii.cbSize = sizeof(mii);
                    mii.fMask = MIIM_STRING;
                    mii.dwTypeData = text;
                    mii.cch = 127;
                    if (::GetMenuItemInfoW(pUDMI->um.hmenu, pUDMI->umi.iPosition, TRUE, &mii)) {
                        ::SetBkMode(hdc, TRANSPARENT);
                        ::SetTextColor(hdc, (state & (ODS_GRAYED | ODS_DISABLED)) ? RGB(128, 128, 128) : RGB(235, 235, 235));

                        NONCLIENTMETRICSW ncm{};
                        ncm.cbSize = sizeof(ncm);
                        ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
                        HFONT hMenuFont = ::CreateFontIndirectW(&ncm.lfMenuFont);
                        HGDIOBJ hOld = ::SelectObject(hdc, hMenuFont);
                        ::DrawTextW(hdc, text, -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        if (hOld) ::SelectObject(hdc, hOld);
                        if (hMenuFont) ::DeleteObject(hMenuFont);
                    }
                    return 0;
                }
                break;
            }

            case WM_NCPAINT:
            case WM_NCACTIVATE: {
                LRESULT lr = ::DefWindowProcW(m_hwnd, uMsg, wParam, lParam);
                if (m_isCurrentDark) {
                    DrawMenuNCBottomLine();
                }
                return lr;
            }

            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORSTATIC: {
                HWND hCtrl = reinterpret_cast<HWND>(lParam);
                if (hCtrl == m_hwndEdit) {
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    if (m_isCurrentDark) {
                        ::SetTextColor(hdc, RGB(240, 240, 240));
                        ::SetBkColor(hdc, RGB(32, 32, 32));
                        return reinterpret_cast<LRESULT>(m_hDarkEditBgBrush.Get());
                    } else {
                        ::SetTextColor(hdc, RGB(0, 0, 0));
                        ::SetBkColor(hdc, RGB(255, 255, 255));
                        return reinterpret_cast<LRESULT>(m_hLightEditBgBrush.Get());
                    }
                }
                break;
            }

            case WM_NOTIFY: {
                LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
                if (pnmh && pnmh->hwndFrom == m_hwndStatusBar && pnmh->code == NM_CUSTOMDRAW) {
                    LPNMCUSTOMDRAW lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
                    if (lpnmcd->dwDrawStage == CDDS_PREPAINT) {
                        if (m_isCurrentDark) {
                            RECT rcBar;
                            ::GetClientRect(m_hwndStatusBar, &rcBar);
                            ::FillRect(lpnmcd->hdc, &rcBar, m_hDarkEditBgBrush.Get());
                            return CDRF_NOTIFYITEMDRAW;
                        }
                        return CDRF_DODEFAULT;
                    }
                    if (lpnmcd->dwDrawStage == CDDS_ITEMPREPAINT) {
                        if (m_isCurrentDark) {
                            ::FillRect(lpnmcd->hdc, &lpnmcd->rc, m_hDarkEditBgBrush.Get());

                            if (lpnmcd->dwItemSpec > 0) {
                                RECT rcSep = lpnmcd->rc;
                                rcSep.right = rcSep.left + 1;
                                ::FillRect(lpnmcd->hdc, &rcSep, m_hDarkSepBrush.Get());
                            }

                            wchar_t partText[256]{};
                            ::SendMessageW(m_hwndStatusBar, SB_GETTEXTW, lpnmcd->dwItemSpec, reinterpret_cast<LPARAM>(partText));

                            ::SetBkMode(lpnmcd->hdc, TRANSPARENT);
                            ::SetTextColor(lpnmcd->hdc, RGB(215, 215, 215));

                            RECT rcText = lpnmcd->rc;
                            rcText.left += 8;
                            rcText.right -= 8;

                            HFONT hFont = reinterpret_cast<HFONT>(::SendMessageW(m_hwndStatusBar, WM_GETFONT, 0, 0));
                            HGDIOBJ hOldFont = nullptr;
                            if (hFont) {
                                hOldFont = ::SelectObject(lpnmcd->hdc, hFont);
                            }
                            ::DrawTextW(lpnmcd->hdc, partText, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                            if (hOldFont) {
                                ::SelectObject(lpnmcd->hdc, hOldFont);
                            }
                            return CDRF_SKIPDEFAULT;
                        }
                        return CDRF_DODEFAULT;
                    }
                }
                break;
            }

            case WM_SETTINGCHANGE: {
                if (m_themeMode == ThemeMode::System) {
                    ApplyTheme();
                }
                return 0;
            }

            case WM_SIZE:
                UpdateLayout();
                return 0;

            case WM_DPICHANGED: {
                RECT* prcNew = reinterpret_cast<RECT*>(lParam);
                ::SetWindowPos(m_hwnd, nullptr, prcNew->left, prcNew->top,
                               prcNew->right - prcNew->left, prcNew->bottom - prcNew->top,
                               SWP_NOZORDER | SWP_NOACTIVATE);
                UpdateFont();
                UpdateLayout();
                return 0;
            }

            case WM_DROPFILES: {
                HDROP hDrop = reinterpret_cast<HDROP>(wParam);
                wchar_t filePath[MAX_PATH];
                if (::DragQueryFileW(hDrop, 0, filePath, MAX_PATH) > 0) {
                    if (PromptToSaveChanges()) {
                        DocumentData data;
                        std::wstring error;
                        if (EncodingDetector::LoadFile(filePath, data, error)) {
                            m_filePath = filePath;
                            m_encoding = data.encoding;
                            m_lineEnding = data.lineEnding;
                            m_isModified = false;
                            ::SetWindowTextW(m_hwndEdit, data.text.c_str());
                            ::SendMessageW(m_hwndEdit, EM_SETMODIFY, FALSE, 0);
                            ::SendMessageW(m_hwndEdit, EM_EMPTYUNDOBUFFER, 0, 0);
                            ::SendMessageW(m_hwndEdit, EM_SETSEL, 0, 0);
                            UpdateTitle();
                            UpdateStatusBarText();
                        } else {
                            ::MessageBoxW(m_hwnd, error.c_str(), Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONERROR);
                        }
                    }
                }
                ::DragFinish(hDrop);
                return 0;
            }

            case WM_CLOSE:
                if (PromptToSaveChanges()) {
                    ::DestroyWindow(m_hwnd);
                }
                return 0;

            case WM_QUERYENDSESSION:
                if (!PromptToSaveChanges()) {
                    return FALSE;
                }
                return TRUE;

            case WM_ENDSESSION:
                if (wParam == TRUE) {
                    ::DestroyWindow(m_hwnd);
                }
                return 0;

            case WM_DESTROY:
                ::PostQuitMessage(0);
                return 0;
        }

        return ::DefWindowProcW(m_hwnd, uMsg, wParam, lParam);
    }

} // namespace RetroNotepad
