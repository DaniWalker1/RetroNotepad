#pragma once

#include <windows.h>
#include <string>
#include <string_view>

namespace RetroNotepad {

    enum class Language {
        Spanish,
        English
    };

    enum class StringId {
        AppTitle,
        Untitled,
        SavePrompt,
        TextNotFound,
        LineOutOfRange,
        StatusLineCol,
        StatusChars,
        StatusCharSingle,
        StatusCharsSelected,
        StatusZoom,
        StatusWindowsCRLF,
        StatusUnixLF,
        StatusMacCR,
        StatusUtf8,
        StatusUtf8Bom,
        StatusUtf16LE,
        StatusUtf16BE,
        StatusAnsi,
        FilterTextFiles,
        FilterAllFiles,
        AboutDescription,
        GotoTitle,
        GotoPrompt,
        GotoBtn,
        CancelBtn,
        MenuTheme,
        ThemeSystem,
        ThemeLight,
        ThemeDark
    };

    class Localization {
    public:
        static void Initialize();
        static void SetLanguage(Language lang);
        static Language GetLanguage() noexcept;

        static std::wstring_view Get(StringId id);
        static void ApplyMenu(HMENU hMenu);

    private:
        static Language s_currentLanguage;
    };

} // namespace RetroNotepad
