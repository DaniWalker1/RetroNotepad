#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include "Localization.hpp"

namespace RetroNotepad {

    enum class Encoding {
        Utf8,
        Utf8Bom,
        Utf16LE,
        Utf16BE,
        Ansi
    };

    enum class LineEnding {
        WindowsCRLF,
        UnixLF,
        MacintoshCR
    };

    struct DocumentData {
        std::wstring text;
        Encoding encoding{ Encoding::Utf8 };
        LineEnding lineEnding{ LineEnding::WindowsCRLF };
    };

    class EncodingDetector {
    public:
        // Límite de seguridad para evitar desbordamiento y DoS (256 MB)
        static constexpr uint64_t MaxSafeFileSize = 256 * 1024 * 1024;

        // Carga y decodifica un archivo de manera segura
        static bool LoadFile(const std::wstring& filePath, DocumentData& outData, std::wstring& outError);

        // Guarda el texto en el archivo respetando codificación y saltos de línea
        static bool SaveFile(const std::wstring& filePath, const std::wstring& text,
                             Encoding encoding, LineEnding lineEnding, std::wstring& outError);

        // Obtiene la representación de texto para la barra de estado
        static std::wstring_view GetEncodingName(Encoding enc);
        static std::wstring_view GetLineEndingName(LineEnding le);

        // Convierte el texto para el control EDIT de Windows (normaliza a CRLF para visualización correcta)
        static std::wstring NormalizeForEditControl(const std::wstring& text);

        // Convierte el texto desde el control EDIT al formato de fin de línea deseado
        static std::wstring ConvertLineEndings(const std::wstring& text, LineEnding targetEnding);

    private:
        static LineEnding DetectLineEnding(const std::wstring& text);
    };

} // namespace RetroNotepad
