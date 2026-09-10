#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <memory>
#include "SafeWin32.hpp"
#include "EncodingDetector.hpp"
#include "FindReplaceController.hpp"

namespace RetroNotepad {

    enum class ThemeMode {
        System,
        Light,
        Dark
    };

    class MainWindow {
    public:
        MainWindow();
        ~MainWindow();

        bool Create(HINSTANCE hInstance, int nCmdShow);
        bool OpenFileDirectly(const std::wstring& path);
        [[nodiscard]] HWND GetHwnd() const noexcept { return m_hwnd; }
        [[nodiscard]] HWND GetEditHwnd() const noexcept { return m_hwndEdit; }
        [[nodiscard]] HWND GetFindDialogHwnd() const noexcept { return m_findReplace.GetDialogHandle(); }

        // Bucle y filtros
        bool PreTranslateMessage(MSG* pMsg);

    private:
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
        static INT_PTR CALLBACK GotoDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

        // Inicialización y controles
        void CreateControls();
        void RecreateEditControl(bool wordWrap);
        void UpdateLayout();
        void UpdateStatusBarParts();
        void UpdateStatusBarText();
        void UpdateTitle();
        void UpdateFont();

        // Operaciones de archivo
        bool DoFileNew();
        void DoFileNewWindow();
        bool DoFileOpen();
        bool DoFileSave();
        bool DoFileSaveAs();
        bool PromptToSaveChanges();

        // Operaciones de edición y formato
        void DoEditTimeDate();
        void DoSearchWithBing();
        void DoGoToLine();
        void DoChooseFont();
        void DoAbout();
        void ToggleWordWrap();
        void ToggleStatusBar();

        // Control de Zoom
        void SetZoom(int zoomPercent);
        void ZoomIn();
        void ZoomOut();
        void ResetZoom();

        // Idioma y Tema
        void SwitchLanguage(Language lang);
        void SetThemeMode(ThemeMode mode);
        void ApplyTheme();
        [[nodiscard]] bool IsSystemDarkModeActive() const;
        void UpdateThemeMenuChecks();
        void DrawMenuNCBottomLine();

        // Miembros de ventana
        HWND m_hwnd;
        HWND m_hwndEdit;
        HWND m_hwndStatusBar;
        HINSTANCE m_hInstance;

        // Estado del documento
        std::wstring m_filePath;
        bool m_isModified;
        Encoding m_encoding;
        LineEnding m_lineEnding;
        bool m_wordWrap;
        bool m_showStatusBar;

        // Tema (Light/Dark acorde al sistema)
        ThemeMode m_themeMode;
        bool m_isCurrentDark;
        Safe::SafeBrush m_hDarkEditBgBrush;
        Safe::SafeBrush m_hLightEditBgBrush;
        Safe::SafeBrush m_hDarkHoverBrush;
        Safe::SafeBrush m_hDarkSepBrush;

        // Visualización y tipografía
        LOGFONTW m_logFont;
        Safe::SafeFont m_hFont;
        int m_zoomPercent;

        // Búsqueda y reemplazo
        FindReplaceController m_findReplace;
    };

} // namespace RetroNotepad
