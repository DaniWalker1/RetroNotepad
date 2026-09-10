#include "FindReplaceController.hpp"
#include "Localization.hpp"
#include <algorithm>
#include <vector>

namespace RetroNotepad {

    FindReplaceController::FindReplaceController()
        : m_hDlg(nullptr)
        , m_findMsgId(::RegisterWindowMessageW(FINDMSGSTRINGW))
        , m_lastMatchCase(false)
        , m_lastSearchDown(true)
    {
        ZeroMemory(&m_fr, sizeof(m_fr));
        m_findBuf[0] = L'\0';
        m_replaceBuf[0] = L'\0';
    }

    FindReplaceController::~FindReplaceController() {
        CloseDialog();
    }

    void FindReplaceController::CloseDialog() {
        if (m_hDlg != nullptr) {
            ::DestroyWindow(m_hDlg);
            m_hDlg = nullptr;
        }
    }

    void FindReplaceController::ShowFindDialog(HWND hwndOwner) {
        if (m_hDlg != nullptr) {
            ::SetFocus(m_hDlg);
            return;
        }

        // Si hay texto seleccionado en el control EDIT, precargarlo en el buffer de búsqueda
        HWND hwndEdit = ::GetDlgItem(hwndOwner, 100);
        if (hwndEdit) {
            DWORD start = 0, end = 0;
            ::SendMessageW(hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            if (end > start && (end - start) < sizeof(m_findBuf) / sizeof(wchar_t)) {
                int len = ::GetWindowTextLengthW(hwndEdit);
                if (len > 0 && static_cast<size_t>(start) < static_cast<size_t>(len)) {
                    std::wstring fullText(len + 1, L'\0');
                    ::GetWindowTextW(hwndEdit, fullText.data(), len + 1);
                    fullText.resize(len);
                    size_t count = (std::min)(static_cast<size_t>(end - start), fullText.size() - static_cast<size_t>(start));
                    std::wstring selected = fullText.substr(start, count);
                    if (selected.find(L'\r') == std::wstring::npos && selected.find(L'\n') == std::wstring::npos) {
                        wcsncpy_s(m_findBuf, selected.c_str(), _TRUNCATE);
                    }
                }
            }
        }

        ZeroMemory(&m_fr, sizeof(m_fr));
        m_fr.lStructSize = sizeof(FINDREPLACEW);
        m_fr.hwndOwner = hwndOwner;
        m_fr.Flags = FR_DOWN | FR_HIDEWHOLEWORD;
        m_fr.lpstrFindWhat = m_findBuf;
        m_fr.wFindWhatLen = static_cast<WORD>(sizeof(m_findBuf) / sizeof(wchar_t));

        m_hDlg = ::FindTextW(&m_fr);
    }

    void FindReplaceController::ShowReplaceDialog(HWND hwndOwner) {
        if (m_hDlg != nullptr) {
            ::SetFocus(m_hDlg);
            return;
        }

        ZeroMemory(&m_fr, sizeof(m_fr));
        m_fr.lStructSize = sizeof(FINDREPLACEW);
        m_fr.hwndOwner = hwndOwner;
        m_fr.Flags = FR_DOWN | FR_HIDEWHOLEWORD;
        m_fr.lpstrFindWhat = m_findBuf;
        m_fr.wFindWhatLen = static_cast<WORD>(sizeof(m_findBuf) / sizeof(wchar_t));
        m_fr.lpstrReplaceWith = m_replaceBuf;
        m_fr.wReplaceWithLen = static_cast<WORD>(sizeof(m_replaceBuf) / sizeof(wchar_t));

        m_hDlg = ::ReplaceTextW(&m_fr);
    }

    bool FindReplaceController::HandleMessage(HWND hwndOwner, HWND hwndEdit, UINT msg, WPARAM /*wParam*/, LPARAM lParam) {
        if (msg != m_findMsgId) {
            return false;
        }

        LPFINDREPLACEW lpfr = reinterpret_cast<LPFINDREPLACEW>(lParam);
        if (!lpfr) return false;

        if (lpfr->Flags & FR_DIALOGTERM) {
            m_hDlg = nullptr;
            return true;
        }

        const bool matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
        const bool searchDown = (lpfr->Flags & FR_DOWN) != 0;
        m_lastMatchCase = matchCase;
        m_lastSearchDown = searchDown;

        if (lpfr->Flags & FR_FINDNEXT) {
            PerformFind(hwndOwner, hwndEdit, lpfr->lpstrFindWhat, searchDown, matchCase, true);
            return true;
        }

        if (lpfr->Flags & FR_REPLACE) {
            PerformReplace(hwndOwner, hwndEdit, lpfr->lpstrFindWhat, lpfr->lpstrReplaceWith, searchDown, matchCase);
            return true;
        }

        if (lpfr->Flags & FR_REPLACEALL) {
            PerformReplaceAll(hwndOwner, hwndEdit, lpfr->lpstrFindWhat, lpfr->lpstrReplaceWith, matchCase);
            return true;
        }

        return false;
    }

    bool FindReplaceController::FindNext(HWND hwndOwner, HWND hwndEdit, bool searchDown) {
        if (!HasSearchTerm()) {
            ShowFindDialog(hwndOwner);
            return false;
        }
        return PerformFind(hwndOwner, hwndEdit, m_findBuf, searchDown, m_lastMatchCase, true);
    }

    static std::wstring ToLowerString(const std::wstring& str) {
        std::wstring res(str.size(), L'\0');
        for (size_t i = 0; i < str.size(); ++i) {
            res[i] = static_cast<wchar_t>(::towlower(str[i]));
        }
        return res;
    }

    bool FindReplaceController::PerformFind(HWND hwndOwner, HWND hwndEdit, const std::wstring& textToFind,
                                            bool searchDown, bool matchCase, bool wrapAround) {
        if (textToFind.empty() || !hwndEdit) return false;

        int textLen = ::GetWindowTextLengthW(hwndEdit);
        if (textLen == 0) {
            wchar_t msg[300];
            swprintf_s(msg, Localization::Get(StringId::TextNotFound).data(), textToFind.c_str());
            ::MessageBoxW(m_hDlg ? m_hDlg : hwndOwner, msg, Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONINFORMATION);
            return false;
        }

        std::wstring docText(textLen + 1, L'\0');
        ::GetWindowTextW(hwndEdit, docText.data(), textLen + 1);
        docText.resize(textLen);

        DWORD startSel = 0, endSel = 0;
        ::SendMessageW(hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&startSel), reinterpret_cast<LPARAM>(&endSel));

        std::wstring hay = matchCase ? docText : ToLowerString(docText);
        std::wstring needle = matchCase ? textToFind : ToLowerString(textToFind);

        size_t matchPos = std::wstring::npos;

        if (searchDown) {
            size_t searchFrom = static_cast<size_t>(endSel);
            if (searchFrom < hay.size()) {
                matchPos = hay.find(needle, searchFrom);
            }
            if (matchPos == std::wstring::npos && wrapAround && searchFrom > 0) {
                // Wrap around desde el principio
                matchPos = hay.find(needle, 0);
            }
        } else {
            size_t searchFrom = (startSel > 0) ? static_cast<size_t>(startSel - 1) : std::wstring::npos;
            if (searchFrom != std::wstring::npos) {
                matchPos = hay.rfind(needle, searchFrom);
            }
            if (matchPos == std::wstring::npos && wrapAround) {
                // Wrap around desde el final
                matchPos = hay.rfind(needle, hay.size());
            }
        }

        if (matchPos != std::wstring::npos) {
            DWORD matchStart = static_cast<DWORD>(matchPos);
            DWORD matchEnd = static_cast<DWORD>(matchPos + needle.size());
            ::SendMessageW(hwndEdit, EM_SETSEL, matchStart, matchEnd);
            ::SendMessageW(hwndEdit, EM_SCROLLCARET, 0, 0);
            return true;
        }

        wchar_t msg[300];
        swprintf_s(msg, Localization::Get(StringId::TextNotFound).data(), textToFind.c_str());
        ::MessageBoxW(m_hDlg ? m_hDlg : hwndOwner, msg, Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONINFORMATION);
        return false;
    }

    void FindReplaceController::PerformReplace(HWND hwndOwner, HWND hwndEdit, const std::wstring& textToFind,
                                               const std::wstring& textToReplace, bool searchDown, bool matchCase) {
        if (!hwndEdit || textToFind.empty()) return;

        DWORD startSel = 0, endSel = 0;
        ::SendMessageW(hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&startSel), reinterpret_cast<LPARAM>(&endSel));

        if (endSel > startSel && (endSel - startSel) == textToFind.size()) {
            int len = ::GetWindowTextLengthW(hwndEdit);
            if (len > 0 && static_cast<size_t>(startSel) < static_cast<size_t>(len)) {
                std::wstring docText(len + 1, L'\0');
                ::GetWindowTextW(hwndEdit, docText.data(), len + 1);
                docText.resize(len);

                size_t count = (std::min)(static_cast<size_t>(endSel - startSel), docText.size() - static_cast<size_t>(startSel));
                std::wstring selected = docText.substr(startSel, count);
                bool matches = matchCase ? (selected == textToFind) : (_wcsicmp(selected.c_str(), textToFind.c_str()) == 0);

                if (matches) {
                    ::SendMessageW(hwndEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(textToReplace.c_str()));
                }
            }
        }

        PerformFind(hwndOwner, hwndEdit, textToFind, searchDown, matchCase, true);
    }

    void FindReplaceController::PerformReplaceAll(HWND hwndOwner, HWND hwndEdit, const std::wstring& textToFind,
                                                  const std::wstring& textToReplace, bool matchCase) {
        if (!hwndEdit || textToFind.empty()) return;

        int textLen = ::GetWindowTextLengthW(hwndEdit);
        if (textLen == 0) return;

        std::wstring docText(textLen + 1, L'\0');
        ::GetWindowTextW(hwndEdit, docText.data(), textLen + 1);
        docText.resize(textLen);

        std::wstring hay = matchCase ? docText : ToLowerString(docText);
        std::wstring needle = matchCase ? textToFind : ToLowerString(textToFind);

        size_t pos = 0;
        size_t replacedCount = 0;
        std::wstring newText;
        newText.reserve(docText.size());

        while (true) {
            size_t matchPos = hay.find(needle, pos);
            if (matchPos == std::wstring::npos) {
                newText.append(docText, pos, docText.size() - pos);
                break;
            }
            newText.append(docText, pos, matchPos - pos);
            newText.append(textToReplace);
            pos = matchPos + needle.size();
            ++replacedCount;
        }

        if (replacedCount > 0) {
            ::SetWindowTextW(hwndEdit, newText.c_str());
            ::SendMessageW(hwndEdit, EM_SETSEL, 0, 0);
        } else {
            wchar_t msg[300];
            swprintf_s(msg, Localization::Get(StringId::TextNotFound).data(), textToFind.c_str());
            ::MessageBoxW(m_hDlg ? m_hDlg : hwndOwner, msg, Localization::Get(StringId::AppTitle).data(), MB_OK | MB_ICONINFORMATION);
        }
    }

} // namespace RetroNotepad
