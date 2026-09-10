#include "EncodingDetector.hpp"
#include "SafeWin32.hpp"
#include <algorithm>

namespace RetroNotepad {

    static bool IsReservedDeviceName(const std::wstring& path) {
        size_t slash = path.find_last_of(L"\\/");
        std::wstring filename = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
        size_t dot = filename.find_last_of(L'.');
        std::wstring stem = (dot != std::wstring::npos) ? filename.substr(0, dot) : filename;

        const wchar_t* const reserved[] = {
            L"CON", L"PRN", L"AUX", L"NUL",
            L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
            L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
        };
        for (const auto* dev : reserved) {
            if (_wcsicmp(stem.c_str(), dev) == 0) return true;
        }
        return false;
    }

    bool EncodingDetector::LoadFile(const std::wstring& filePath, DocumentData& outData, std::wstring& outError) {
        using namespace Safe;

        if (IsReservedDeviceName(filePath)) {
            outError = L"El nombre de archivo especificado corresponde a un dispositivo reservado de Windows.";
            return false;
        }

        SafeHandle hFile(::CreateFileW(
            filePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        ));

        if (!hFile.IsValid()) {
            outError = L"No se pudo abrir el archivo para lectura.";
            return false;
        }

        LARGE_INTEGER fileSize{};
        if (!::GetFileSizeEx(hFile.Get(), &fileSize)) {
            outError = L"Error al obtener el tamaño del archivo.";
            return false;
        }

        if (static_cast<uint64_t>(fileSize.QuadPart) > MaxSafeFileSize) {
            outError = L"El archivo supera el límite de tamaño seguro (256 MB).";
            return false;
        }

        const DWORD bytesToRead = static_cast<DWORD>(fileSize.QuadPart);
        std::vector<uint8_t> buffer(bytesToRead);

        DWORD bytesRead = 0;
        DWORD totalRead = 0;
        while (totalRead < bytesToRead) {
            DWORD chunk = (std::min)(bytesToRead - totalRead, static_cast<DWORD>(64 * 1024));
            if (!::ReadFile(hFile.Get(), buffer.data() + totalRead, chunk, &bytesRead, nullptr) || bytesRead == 0) {
                break;
            }
            totalRead += bytesRead;
        }

        if (totalRead == 0) {
            outData.text.clear();
            outData.encoding = Encoding::Utf8;
            outData.lineEnding = LineEnding::WindowsCRLF;
            return true;
        }

        buffer.resize(totalRead);

        // Detección de BOM y Decodificación
        const uint8_t* data = buffer.data();
        const size_t size = buffer.size();

        Encoding detectedEncoding = Encoding::Utf8;
        std::wstring decodedText;

        if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            // UTF-8 con BOM
            detectedEncoding = Encoding::Utf8Bom;
            const char* utf8Data = reinterpret_cast<const char*>(data + 3);
            int utf8Size = static_cast<int>(size - 3);
            if (utf8Size > 0) {
                int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8Data, utf8Size, nullptr, 0);
                if (wideLen > 0) {
                    decodedText.resize(wideLen);
                    ::MultiByteToWideChar(CP_UTF8, 0, utf8Data, utf8Size, decodedText.data(), wideLen);
                }
            }
        } else if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
            // UTF-16 LE
            detectedEncoding = Encoding::Utf16LE;
            const size_t wideCount = (size - 2) / sizeof(wchar_t);
            decodedText.resize(wideCount);
            if (wideCount > 0) {
                std::memcpy(decodedText.data(), data + 2, wideCount * sizeof(wchar_t));
            }
        } else if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
            // UTF-16 BE
            detectedEncoding = Encoding::Utf16BE;
            const size_t wideCount = (size - 2) / sizeof(wchar_t);
            decodedText.resize(wideCount);
            const uint8_t* src = data + 2;
            for (size_t i = 0; i < wideCount; ++i) {
                decodedText[i] = static_cast<wchar_t>((src[i * 2] << 8) | src[i * 2 + 1]);
            }
        } else {
            // Sin BOM: intentamos UTF-8 estricto
            const char* mbData = reinterpret_cast<const char*>(data);
            int mbSize = static_cast<int>(size);
            int wideLen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mbData, mbSize, nullptr, 0);

            if (wideLen > 0) {
                detectedEncoding = Encoding::Utf8;
                decodedText.resize(wideLen);
                ::MultiByteToWideChar(CP_UTF8, 0, mbData, mbSize, decodedText.data(), wideLen);
            } else {
                // Si falla UTF-8 estricto, comprobar si es UTF-16 LE sin BOM mediante IsTextUnicode
                INT tests = IS_TEXT_UNICODE_STATISTICS | IS_TEXT_UNICODE_CONTROLS;
                if (size >= 4 && (size % sizeof(wchar_t) == 0) && ::IsTextUnicode(data, static_cast<int>(size), &tests)) {
                    detectedEncoding = Encoding::Utf16LE;
                    const size_t wideCount = size / sizeof(wchar_t);
                    decodedText.resize(wideCount);
                    std::memcpy(decodedText.data(), data, size);
                } else {
                    // Fallback seguro a ANSI (Página de códigos activa)
                    detectedEncoding = Encoding::Ansi;
                    wideLen = ::MultiByteToWideChar(CP_ACP, 0, mbData, mbSize, nullptr, 0);
                    if (wideLen > 0) {
                        decodedText.resize(wideLen);
                        ::MultiByteToWideChar(CP_ACP, 0, mbData, mbSize, decodedText.data(), wideLen);
                    }
                }
            }
        }

        outData.encoding = detectedEncoding;
        outData.lineEnding = DetectLineEnding(decodedText);
        outData.text = NormalizeForEditControl(decodedText);

        return true;
    }

    bool EncodingDetector::SaveFile(const std::wstring& filePath, const std::wstring& text,
                                   Encoding encoding, LineEnding lineEnding, std::wstring& outError) {
        using namespace Safe;

        if (IsReservedDeviceName(filePath)) {
            outError = L"El nombre de archivo especificado corresponde a un dispositivo reservado de Windows.";
            return false;
        }

        // Convertir saltos de línea al formato deseado
        std::wstring convertedText = ConvertLineEndings(text, lineEnding);

        std::vector<uint8_t> outputBytes;

        switch (encoding) {
            case Encoding::Utf8Bom: {
                // Agregar BOM UTF-8 (EF BB BF)
                outputBytes.push_back(0xEF);
                outputBytes.push_back(0xBB);
                outputBytes.push_back(0xBF);
                [[fallthrough]];
            }
            case Encoding::Utf8: {
                if (!convertedText.empty()) {
                    int utf8Len = ::WideCharToMultiByte(CP_UTF8, 0, convertedText.c_str(),
                                                        static_cast<int>(convertedText.size()),
                                                        nullptr, 0, nullptr, nullptr);
                    if (utf8Len > 0) {
                        size_t offset = outputBytes.size();
                        outputBytes.resize(offset + utf8Len);
                        ::WideCharToMultiByte(CP_UTF8, 0, convertedText.c_str(),
                                              static_cast<int>(convertedText.size()),
                                              reinterpret_cast<char*>(outputBytes.data() + offset),
                                              utf8Len, nullptr, nullptr);
                    }
                }
                break;
            }
            case Encoding::Utf16LE: {
                // BOM UTF-16 LE (FF FE)
                outputBytes.push_back(0xFF);
                outputBytes.push_back(0xFE);
                if (!convertedText.empty()) {
                    const size_t byteCount = convertedText.size() * sizeof(wchar_t);
                    size_t offset = outputBytes.size();
                    outputBytes.resize(offset + byteCount);
                    std::memcpy(outputBytes.data() + offset, convertedText.data(), byteCount);
                }
                break;
            }
            case Encoding::Utf16BE: {
                // BOM UTF-16 BE (FE FF)
                outputBytes.push_back(0xFE);
                outputBytes.push_back(0xFF);
                for (wchar_t wc : convertedText) {
                    outputBytes.push_back(static_cast<uint8_t>((wc >> 8) & 0xFF));
                    outputBytes.push_back(static_cast<uint8_t>(wc & 0xFF));
                }
                break;
            }
            case Encoding::Ansi: {
                if (!convertedText.empty()) {
                    int ansiLen = ::WideCharToMultiByte(CP_ACP, 0, convertedText.c_str(),
                                                        static_cast<int>(convertedText.size()),
                                                        nullptr, 0, nullptr, nullptr);
                    if (ansiLen > 0) {
                        outputBytes.resize(ansiLen);
                        ::WideCharToMultiByte(CP_ACP, 0, convertedText.c_str(),
                                              static_cast<int>(convertedText.size()),
                                              reinterpret_cast<char*>(outputBytes.data()),
                                              ansiLen, nullptr, nullptr);
                    }
                }
                break;
            }
        }

        // Guardado Atómico Seguro:
        // 1. Escribir primero en un archivo temporal con sufijo .tmp~
        // 2. Si la escritura tiene éxito, reemplazar el archivo original con ReplaceFileW o MoveFileExW
        // De esta forma, si el disco se llena o falla la I/O, el archivo original nunca queda truncado a 0 bytes.
        std::wstring tempFilePath = filePath + L".tmp~";

        SafeHandle hFile(::CreateFileW(
            tempFilePath.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        ));

        if (!hFile.IsValid()) {
            outError = L"No se pudo crear el archivo temporal de guardado.";
            return false;
        }

        if (!outputBytes.empty()) {
            DWORD bytesWritten = 0;
            DWORD totalWritten = 0;
            const DWORD totalBytes = static_cast<DWORD>(outputBytes.size());

            while (totalWritten < totalBytes) {
                DWORD chunk = (std::min)(totalBytes - totalWritten, static_cast<DWORD>(64 * 1024));
                if (!::WriteFile(hFile.Get(), outputBytes.data() + totalWritten, chunk, &bytesWritten, nullptr) || bytesWritten == 0) {
                    hFile.Close();
                    ::DeleteFileW(tempFilePath.c_str());
                    outError = L"Error al escribir en el archivo.";
                    return false;
                }
                totalWritten += bytesWritten;
            }
            ::FlushFileBuffers(hFile.Get());
        }
        hFile.Close();

        // Reemplazar atómicamente el archivo destino
        if (::GetFileAttributesW(filePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            if (!::ReplaceFileW(filePath.c_str(), tempFilePath.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
                if (!::MoveFileExW(tempFilePath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                    ::DeleteFileW(tempFilePath.c_str());
                    outError = L"No se pudo reemplazar el archivo existente de forma atómica.";
                    return false;
                }
            }
        } else {
            if (!::MoveFileExW(tempFilePath.c_str(), filePath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
                ::DeleteFileW(tempFilePath.c_str());
                outError = L"No se pudo mover el archivo temporal a su destino final.";
                return false;
            }
        }

        return true;
    }

    LineEnding EncodingDetector::DetectLineEnding(const std::wstring& text) {
        size_t crlfCount = 0;
        size_t lfCount = 0;
        size_t crCount = 0;

        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == L'\r') {
                if (i + 1 < text.size() && text[i + 1] == L'\n') {
                    ++crlfCount;
                    ++i;
                } else {
                    ++crCount;
                }
            } else if (text[i] == L'\n') {
                ++lfCount;
            }
        }

        if (crlfCount >= lfCount && crlfCount >= crCount) {
            return LineEnding::WindowsCRLF;
        }
        if (lfCount > crlfCount && lfCount >= crCount) {
            return LineEnding::UnixLF;
        }
        return LineEnding::MacintoshCR;
    }

    std::wstring EncodingDetector::NormalizeForEditControl(const std::wstring& text) {
        std::wstring result;
        result.reserve(text.size() + text.size() / 10);

        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == L'\r') {
                if (i + 1 < text.size() && text[i + 1] == L'\n') {
                    result.push_back(L'\r');
                    result.push_back(L'\n');
                    ++i;
                } else {
                    result.push_back(L'\r');
                    result.push_back(L'\n');
                }
            } else if (text[i] == L'\n') {
                result.push_back(L'\r');
                result.push_back(L'\n');
            } else {
                result.push_back(text[i]);
            }
        }

        return result;
    }

    std::wstring EncodingDetector::ConvertLineEndings(const std::wstring& text, LineEnding targetEnding) {
        std::wstring result;
        result.reserve(text.size());

        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == L'\r') {
                if (i + 1 < text.size() && text[i + 1] == L'\n') {
                    ++i;
                }
                switch (targetEnding) {
                    case LineEnding::WindowsCRLF:
                        result.push_back(L'\r');
                        result.push_back(L'\n');
                        break;
                    case LineEnding::UnixLF:
                        result.push_back(L'\n');
                        break;
                    case LineEnding::MacintoshCR:
                        result.push_back(L'\r');
                        break;
                }
            } else if (text[i] == L'\n') {
                switch (targetEnding) {
                    case LineEnding::WindowsCRLF:
                        result.push_back(L'\r');
                        result.push_back(L'\n');
                        break;
                    case LineEnding::UnixLF:
                        result.push_back(L'\n');
                        break;
                    case LineEnding::MacintoshCR:
                        result.push_back(L'\r');
                        break;
                }
            } else {
                result.push_back(text[i]);
            }
        }

        return result;
    }

    std::wstring_view EncodingDetector::GetEncodingName(Encoding enc) {
        switch (enc) {
            case Encoding::Utf8: return Localization::Get(StringId::StatusUtf8);
            case Encoding::Utf8Bom: return Localization::Get(StringId::StatusUtf8Bom);
            case Encoding::Utf16LE: return Localization::Get(StringId::StatusUtf16LE);
            case Encoding::Utf16BE: return Localization::Get(StringId::StatusUtf16BE);
            case Encoding::Ansi: return Localization::Get(StringId::StatusAnsi);
            default: return L"UTF-8";
        }
    }

    std::wstring_view EncodingDetector::GetLineEndingName(LineEnding le) {
        switch (le) {
            case LineEnding::WindowsCRLF: return Localization::Get(StringId::StatusWindowsCRLF);
            case LineEnding::UnixLF: return Localization::Get(StringId::StatusUnixLF);
            case LineEnding::MacintoshCR: return Localization::Get(StringId::StatusMacCR);
            default: return L"Windows (CRLF)";
        }
    }

} // namespace RetroNotepad
