#pragma once

#include <windows.h>
#include <commdlg.h>
#include <string>

namespace RetroNotepad {

    class FindReplaceController {
    public:
        FindReplaceController();
        ~FindReplaceController();

        void ShowFindDialog(HWND hwndOwner);
        void ShowReplaceDialog(HWND hwndOwner);

        // Procesa el mensaje registrado FINDMSGSTRING
        bool HandleMessage(HWND hwndOwner, HWND hwndEdit, UINT msg, WPARAM wParam, LPARAM lParam);

        // Búsqueda directa (F3 = siguiente, Shift+F3 = anterior)
        bool FindNext(HWND hwndOwner, HWND hwndEdit, bool searchDown);

        [[nodiscard]] HWND GetDialogHandle() const noexcept { return m_hDlg; }
        [[nodiscard]] UINT GetFindMessageId() const noexcept { return m_findMsgId; }
        [[nodiscard]] bool HasSearchTerm() const noexcept { return m_findBuf[0] != L'\0'; }

        void CloseDialog();

    private:
        bool PerformFind(HWND hwndOwner, HWND hwndEdit, const std::wstring& textToFind,
                         bool searchDown, bool matchCase, bool wrapAround);

        void PerformReplace(HWND hwndOwner, HWND hwndEdit, const std::wstring& textToFind,
                            const std::wstring& textToReplace, bool searchDown, bool matchCase);

        void PerformReplaceAll(HWND hwndOwner, HWND hwndEdit, const std::wstring& textToFind,
                               const std::wstring& textToReplace, bool matchCase);

        HWND m_hDlg;
        FINDREPLACEW m_fr;
        wchar_t m_findBuf[256];
        wchar_t m_replaceBuf[256];
        UINT m_findMsgId;
        bool m_lastMatchCase;
        bool m_lastSearchDown;
    };

} // namespace RetroNotepad
