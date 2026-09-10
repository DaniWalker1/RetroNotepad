#include "Localization.hpp"
#include "resource.h"

namespace RetroNotepad {

    Language Localization::s_currentLanguage = Language::Spanish;

    void Localization::Initialize() {
        LANGID langId = GetUserDefaultUILanguage();
        if (PRIMARYLANGID(langId) == LANG_SPANISH) {
            s_currentLanguage = Language::Spanish;
        } else {
            s_currentLanguage = Language::English;
        }
    }

    void Localization::SetLanguage(Language lang) {
        s_currentLanguage = lang;
    }

    Language Localization::GetLanguage() noexcept {
        return s_currentLanguage;
    }

    std::wstring_view Localization::Get(StringId id) {
        const bool es = (s_currentLanguage == Language::Spanish);
        switch (id) {
            case StringId::AppTitle:
                return L"Retro Notepad";
            case StringId::Untitled:
                return es ? L"Sin título" : L"Untitled";
            case StringId::SavePrompt:
                return es ? L"¿Deseas guardar los cambios en %s?" : L"Do you want to save changes to %s?";
            case StringId::TextNotFound:
                return es ? L"No se puede encontrar \"%s\"" : L"Cannot find \"%s\"";
            case StringId::LineOutOfRange:
                return es ? L"El número de línea está fuera del intervalo." : L"Line number out of range.";
            case StringId::StatusLineCol:
                return es ? L"Lín %zu, Col %zu" : L"Ln %zu, Col %zu";
            case StringId::StatusChars:
                return es ? L"%zu caracteres" : L"%zu characters";
            case StringId::StatusCharSingle:
                return es ? L"%zu carácter" : L"%zu character";
            case StringId::StatusCharsSelected:
                return es ? L"%zu de %zu caracteres" : L"%zu of %zu characters";
            case StringId::StatusZoom:
                return L"%d%%";
            case StringId::StatusWindowsCRLF:
                return L"Windows (CRLF)";
            case StringId::StatusUnixLF:
                return L"Unix (LF)";
            case StringId::StatusMacCR:
                return L"Macintosh (CR)";
            case StringId::StatusUtf8:
                return L"UTF-8";
            case StringId::StatusUtf8Bom:
                return es ? L"UTF-8 con BOM" : L"UTF-8 with BOM";
            case StringId::StatusUtf16LE:
                return L"UTF-16 LE";
            case StringId::StatusUtf16BE:
                return L"UTF-16 BE";
            case StringId::StatusAnsi:
                return L"ANSI";
            case StringId::FilterTextFiles:
                return es ? L"Documentos de texto (*.txt)" : L"Text Documents (*.txt)";
            case StringId::FilterAllFiles:
                return es ? L"Todos los archivos (*.*)" : L"All Files (*.*)";
            case StringId::AboutDescription:
                return es ? L"Retro Notepad\nClon 1:1 de Windows 10 Notepad en C++20 nativo.\nDiseñado para máxima velocidad, seguridad y zero-dependencies."
                          : L"Retro Notepad\n1:1 Windows 10 Notepad Clone in Modern Native C++20.\nEngineered for maximum speed, security and zero-dependencies.";
            case StringId::GotoTitle:
                return es ? L"Ir a la línea" : L"Go To Line";
            case StringId::GotoPrompt:
                return es ? L"Número de línea:" : L"Line number:";
            case StringId::GotoBtn:
                return es ? L"Ir a" : L"Go To";
            case StringId::CancelBtn:
                return es ? L"Cancelar" : L"Cancel";
            case StringId::MenuTheme:
                return es ? L"&Tema" : L"&Theme";
            case StringId::ThemeSystem:
                return es ? L"&Acorde al sistema" : L"&System default";
            case StringId::ThemeLight:
                return es ? L"&Claro" : L"&Light";
            case StringId::ThemeDark:
                return es ? L"&Oscuro" : L"&Dark";
            default:
                return L"";
        }
    }

    void Localization::ApplyMenu(HMENU hMenu) {
        if (!hMenu) return;

        const bool es = (s_currentLanguage == Language::Spanish);

        auto setItemText = [hMenu](UINT uId, const wchar_t* text) {
            MENUITEMINFOW mii{};
            mii.cbSize = sizeof(MENUITEMINFOW);
            mii.fMask = MIIM_STRING;
            mii.dwTypeData = const_cast<LPWSTR>(text);
            ::SetMenuItemInfoW(hMenu, uId, FALSE, &mii);
        };

        auto setPosText = [hMenu](UINT uPos, const wchar_t* text) {
            MENUITEMINFOW mii{};
            mii.cbSize = sizeof(MENUITEMINFOW);
            mii.fMask = MIIM_STRING;
            mii.dwTypeData = const_cast<LPWSTR>(text);
            ::SetMenuItemInfoW(hMenu, uPos, TRUE, &mii);
        };

        // Barra superior (Top-level menus)
        setPosText(0, es ? L"&Archivo" : L"&File");
        setPosText(1, es ? L"&Edición" : L"&Edit");
        setPosText(2, es ? L"F&ormato" : L"F&ormat");
        setPosText(3, es ? L"&Ver" : L"&View");
        setPosText(4, es ? L"A&yuda" : L"&Help");

        // Menú Archivo
        setItemText(IDM_FILE_NEW, es ? L"&Nuevo\tCtrl+N" : L"&New\tCtrl+N");
        setItemText(IDM_FILE_NEW_WINDOW, es ? L"Nueva &ventana\tCtrl+Mayús+N" : L"New &window\tCtrl+Shift+N");
        setItemText(IDM_FILE_OPEN, es ? L"&Abrir...\tCtrl+O" : L"&Open...\tCtrl+O");
        setItemText(IDM_FILE_SAVE, es ? L"&Guardar\tCtrl+S" : L"&Save\tCtrl+S");
        setItemText(IDM_FILE_SAVEAS, es ? L"Guardar &como...\tCtrl+Mayús+S" : L"Save &As...\tCtrl+Shift+S");
        setItemText(IDM_FILE_PAGESETUP, es ? L"Configurar &página..." : L"Page Set&up...");
        setItemText(IDM_FILE_PRINT, es ? L"&Imprimir...\tCtrl+P" : L"&Print...\tCtrl+P");
        setItemText(IDM_FILE_EXIT, es ? L"&Salir" : L"E&xit");

        // Menú Edición
        setItemText(IDM_EDIT_UNDO, es ? L"&Deshacer\tCtrl+Z" : L"&Undo\tCtrl+Z");
        setItemText(IDM_EDIT_CUT, es ? L"Cor&tar\tCtrl+X" : L"Cu&t\tCtrl+X");
        setItemText(IDM_EDIT_COPY, es ? L"&Copiar\tCtrl+C" : L"&Copy\tCtrl+C");
        setItemText(IDM_EDIT_PASTE, es ? L"&Pegar\tCtrl+V" : L"&Paste\tCtrl+V");
        setItemText(IDM_EDIT_DELETE, es ? L"&Eliminar\tSupr" : L"De&lete\tDel");
        setItemText(IDM_EDIT_SEARCH_BING, es ? L"Búsqueda con &Bing...\tCtrl+E" : L"Search with &Bing...\tCtrl+E");
        setItemText(IDM_EDIT_FIND, es ? L"&Buscar...\tCtrl+F" : L"&Find...\tCtrl+F");
        setItemText(IDM_EDIT_FIND_NEXT, es ? L"Buscar &siguiente\tF3" : L"Find &Next\tF3");
        setItemText(IDM_EDIT_FIND_PREV, es ? L"Buscar &anterior\tMayús+F3" : L"Find &Previous\tShift+F3");
        setItemText(IDM_EDIT_REPLACE, es ? L"&Reemplazar...\tCtrl+H" : L"&Replace...\tCtrl+H");
        setItemText(IDM_EDIT_GOTO, es ? L"&Ir a...\tCtrl+G" : L"&Go To...\tCtrl+G");
        setItemText(IDM_EDIT_SELECTALL, es ? L"Seleccionar &todo\tCtrl+A" : L"Select &All\tCtrl+A");
        setItemText(IDM_EDIT_TIMEDATE, es ? L"&Hora y fecha\tF5" : L"Time/&Date\tF5");

        // Menú Formato
        setItemText(IDM_FORMAT_WORDWRAP, es ? L"&Ajuste de línea" : L"&Word Wrap");
        setItemText(IDM_FORMAT_FONT, es ? L"&Fuente..." : L"&Font...");

        // Menú Ver
        HMENU hViewMenu = ::GetSubMenu(hMenu, 3);
        if (hViewMenu) {
            // Posición 0: Submenú Zoom
            MENUITEMINFOW miiZoom{};
            miiZoom.cbSize = sizeof(MENUITEMINFOW);
            miiZoom.fMask = MIIM_STRING;
            miiZoom.dwTypeData = const_cast<LPWSTR>(es ? L"&Zoom" : L"&Zoom");
            ::SetMenuItemInfoW(hViewMenu, 0, TRUE, &miiZoom);

            // Posición 3: Submenú Tema
            MENUITEMINFOW miiTheme{};
            miiTheme.cbSize = sizeof(MENUITEMINFOW);
            miiTheme.fMask = MIIM_STRING;
            miiTheme.dwTypeData = const_cast<LPWSTR>(es ? L"&Tema" : L"&Theme");
            ::SetMenuItemInfoW(hViewMenu, 3, TRUE, &miiTheme);

            // Posición 4: Submenú Idioma
            MENUITEMINFOW miiLang{};
            miiLang.cbSize = sizeof(MENUITEMINFOW);
            miiLang.fMask = MIIM_STRING;
            miiLang.dwTypeData = const_cast<LPWSTR>(es ? L"&Idioma / Language" : L"&Language / Idioma");
            ::SetMenuItemInfoW(hViewMenu, 4, TRUE, &miiLang);
        }

        setItemText(IDM_VIEW_ZOOM_IN, es ? L"&Acercar\tCtrl+Más" : L"Zoom &In\tCtrl+Plus");
        setItemText(IDM_VIEW_ZOOM_OUT, es ? L"A&lejar\tCtrl+Menos" : L"Zoom &Out\tCtrl+Minus");
        setItemText(IDM_VIEW_ZOOM_RESET, es ? L"&Restaurar zoom predeterminado\tCtrl+0" : L"Restore &Default Zoom\tCtrl+0");
        setItemText(IDM_VIEW_STATUSBAR, es ? L"&Barra de estado" : L"&Status Bar");

        // Opciones de Tema
        setItemText(IDM_VIEW_THEME_SYSTEM, es ? L"&Acorde al sistema" : L"&System default");
        setItemText(IDM_VIEW_THEME_LIGHT, es ? L"&Claro" : L"&Light");
        setItemText(IDM_VIEW_THEME_DARK, es ? L"&Oscuro" : L"&Dark");

        // Idiomas
        ::CheckMenuItem(hMenu, IDM_VIEW_LANG_ES, es ? MF_CHECKED : MF_UNCHECKED);
        ::CheckMenuItem(hMenu, IDM_VIEW_LANG_EN, es ? MF_UNCHECKED : MF_CHECKED);

        // Menú Ayuda
        setItemText(IDM_HELP_VIEWHELP, es ? L"&Ver la Ayuda" : L"View &Help");
        setItemText(IDM_HELP_FEEDBACK, es ? L"&Enviar comentarios" : L"Send &Feedback");
        setItemText(IDM_HELP_ABOUT, es ? L"&Acerca de Retro Notepad" : L"&About Retro Notepad");
    }

} // namespace RetroNotepad
