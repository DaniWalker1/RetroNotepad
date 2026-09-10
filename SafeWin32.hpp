#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <strsafe.h>
#include <utility>
#include <concepts>
#include <string>
#include <string_view>
#include <span>

namespace RetroNotepad::Safe {

    // Encapsulador RAII para HANDLEs del Kernel de Windows (Archivos, Procesos, Eventos)
    class SafeHandle {
    public:
        constexpr SafeHandle() noexcept : m_handle(INVALID_HANDLE_VALUE) {}
        explicit SafeHandle(HANDLE h) noexcept : m_handle(h) {}

        ~SafeHandle() noexcept {
            Close();
        }

        SafeHandle(const SafeHandle&) = delete;
        SafeHandle& operator=(const SafeHandle&) = delete;

        SafeHandle(SafeHandle&& other) noexcept : m_handle(other.m_handle) {
            other.m_handle = INVALID_HANDLE_VALUE;
        }

        SafeHandle& operator=(SafeHandle&& other) noexcept {
            if (this != &other) {
                Close();
                m_handle = other.m_handle;
                other.m_handle = INVALID_HANDLE_VALUE;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept {
            return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return IsValid();
        }

        [[nodiscard]] HANDLE Get() const noexcept {
            return m_handle;
        }

        HANDLE Release() noexcept {
            HANDLE temp = m_handle;
            m_handle = INVALID_HANDLE_VALUE;
            return temp;
        }

        void Reset(HANDLE newHandle = INVALID_HANDLE_VALUE) noexcept {
            if (m_handle != newHandle) {
                Close();
                m_handle = newHandle;
            }
        }

        void Close() noexcept {
            if (IsValid()) {
                ::CloseHandle(m_handle);
                m_handle = INVALID_HANDLE_VALUE;
            }
        }

    private:
        HANDLE m_handle;
    };

    // Encapsulador RAII genérico para Objetos GDI (HFONT, HBRUSH, HBITMAP, HPEN)
    template <typename T>
    concept GdiObjectType = std::is_pointer_v<T>;

    template <GdiObjectType T>
    class SafeGdiObject {
    public:
        constexpr SafeGdiObject() noexcept : m_object(nullptr) {}
        explicit SafeGdiObject(T obj) noexcept : m_object(obj) {}

        ~SafeGdiObject() noexcept {
            Destroy();
        }

        SafeGdiObject(const SafeGdiObject&) = delete;
        SafeGdiObject& operator=(const SafeGdiObject&) = delete;

        SafeGdiObject(SafeGdiObject&& other) noexcept : m_object(other.m_object) {
            other.m_object = nullptr;
        }

        SafeGdiObject& operator=(SafeGdiObject&& other) noexcept {
            if (this != &other) {
                Destroy();
                m_object = other.m_object;
                other.m_object = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept {
            return m_object != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return IsValid();
        }

        [[nodiscard]] T Get() const noexcept {
            return m_object;
        }

        T Release() noexcept {
            T temp = m_object;
            m_object = nullptr;
            return temp;
        }

        void Reset(T newObj = nullptr) noexcept {
            if (m_object != newObj) {
                Destroy();
                m_object = newObj;
            }
        }

        void Destroy() noexcept {
            if (m_object != nullptr) {
                ::DeleteObject(static_cast<HGDIOBJ>(m_object));
                m_object = nullptr;
            }
        }

    private:
        T m_object;
    };

    using SafeFont = SafeGdiObject<HFONT>;
    using SafeBrush = SafeGdiObject<HBRUSH>;
    using SafeBitmap = SafeGdiObject<HBITMAP>;

    // Encapsulador RAII para Contextos de Dispositivo de Ventana (HDC con ReleaseDC)
    class SafeDC {
    public:
        SafeDC(HWND hwnd, HDC hdc) noexcept : m_hwnd(hwnd), m_hdc(hdc) {}
        ~SafeDC() noexcept {
            if (m_hwnd != nullptr && m_hdc != nullptr) {
                ::ReleaseDC(m_hwnd, m_hdc);
                m_hdc = nullptr;
            }
        }

        SafeDC(const SafeDC&) = delete;
        SafeDC& operator=(const SafeDC&) = delete;

        SafeDC(SafeDC&& other) noexcept : m_hwnd(other.m_hwnd), m_hdc(other.m_hdc) {
            other.m_hdc = nullptr;
            other.m_hwnd = nullptr;
        }

        SafeDC& operator=(SafeDC&& other) noexcept {
            if (this != &other) {
                if (m_hwnd != nullptr && m_hdc != nullptr) {
                    ::ReleaseDC(m_hwnd, m_hdc);
                }
                m_hwnd = other.m_hwnd;
                m_hdc = other.m_hdc;
                other.m_hdc = nullptr;
                other.m_hwnd = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_hdc != nullptr; }
        [[nodiscard]] HDC Get() const noexcept { return m_hdc; }

    private:
        HWND m_hwnd;
        HDC m_hdc;
    };

    // Encapsulador RAII para Contextos de Dispositivo GDI creados (CreateDC, CreateCompatibleDC, PrintDlg) que requieren DeleteDC
    class SafeDeleteDC {
    public:
        constexpr SafeDeleteDC() noexcept : m_hdc(nullptr) {}
        explicit SafeDeleteDC(HDC hdc) noexcept : m_hdc(hdc) {}
        ~SafeDeleteDC() noexcept {
            Destroy();
        }

        SafeDeleteDC(const SafeDeleteDC&) = delete;
        SafeDeleteDC& operator=(const SafeDeleteDC&) = delete;

        SafeDeleteDC(SafeDeleteDC&& other) noexcept : m_hdc(other.m_hdc) {
            other.m_hdc = nullptr;
        }

        SafeDeleteDC& operator=(SafeDeleteDC&& other) noexcept {
            if (this != &other) {
                Destroy();
                m_hdc = other.m_hdc;
                other.m_hdc = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_hdc != nullptr; }
        [[nodiscard]] HDC Get() const noexcept { return m_hdc; }

        void Reset(HDC newHdc = nullptr) noexcept {
            if (m_hdc != newHdc) {
                Destroy();
                m_hdc = newHdc;
            }
        }

        void Destroy() noexcept {
            if (m_hdc != nullptr) {
                ::DeleteDC(m_hdc);
                m_hdc = nullptr;
            }
        }

    private:
        HDC m_hdc;
    };

    // Encapsulador RAII para Módulos y DLLs de Windows (HMODULE con FreeLibrary)
    class SafeModule {
    public:
        constexpr SafeModule() noexcept : m_hModule(nullptr) {}
        explicit SafeModule(HMODULE h) noexcept : m_hModule(h) {}
        ~SafeModule() noexcept {
            Free();
        }

        SafeModule(const SafeModule&) = delete;
        SafeModule& operator=(const SafeModule&) = delete;

        SafeModule(SafeModule&& other) noexcept : m_hModule(other.m_hModule) {
            other.m_hModule = nullptr;
        }

        SafeModule& operator=(SafeModule&& other) noexcept {
            if (this != &other) {
                Free();
                m_hModule = other.m_hModule;
                other.m_hModule = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_hModule != nullptr; }
        [[nodiscard]] HMODULE Get() const noexcept { return m_hModule; }

        void Reset(HMODULE newModule = nullptr) noexcept {
            if (m_hModule != newModule) {
                Free();
                m_hModule = newModule;
            }
        }

        void Free() noexcept {
            if (m_hModule != nullptr) {
                ::FreeLibrary(m_hModule);
                m_hModule = nullptr;
            }
        }

    private:
        HMODULE m_hModule;
    };

    // Encapsulador RAII para Memoria Global de Windows (HGLOBAL)
    class SafeGlobalMem {
    public:
        constexpr SafeGlobalMem() noexcept : m_hglobal(nullptr) {}
        explicit SafeGlobalMem(HGLOBAL h) noexcept : m_hglobal(h) {}

        ~SafeGlobalMem() noexcept {
            Free();
        }

        SafeGlobalMem(const SafeGlobalMem&) = delete;
        SafeGlobalMem& operator=(const SafeGlobalMem&) = delete;

        SafeGlobalMem(SafeGlobalMem&& other) noexcept : m_hglobal(other.m_hglobal) {
            other.m_hglobal = nullptr;
        }

        SafeGlobalMem& operator=(SafeGlobalMem&& other) noexcept {
            if (this != &other) {
                Free();
                m_hglobal = other.m_hglobal;
                other.m_hglobal = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_hglobal != nullptr; }
        [[nodiscard]] HGLOBAL Get() const noexcept { return m_hglobal; }

        HGLOBAL Release() noexcept {
            HGLOBAL temp = m_hglobal;
            m_hglobal = nullptr;
            return temp;
        }

        void Free() noexcept {
            if (m_hglobal != nullptr) {
                ::GlobalFree(m_hglobal);
                m_hglobal = nullptr;
            }
        }

    private:
        HGLOBAL m_hglobal;
    };

    // Bloqueo y desbloqueo seguro de memoria global (GlobalLock / GlobalUnlock)
    template <typename T = void>
    class SafeGlobalLock {
    public:
        explicit SafeGlobalLock(HGLOBAL h) noexcept : m_hglobal(h), m_ptr(nullptr) {
            if (m_hglobal != nullptr) {
                m_ptr = static_cast<T*>(::GlobalLock(m_hglobal));
            }
        }

        ~SafeGlobalLock() noexcept {
            if (m_hglobal != nullptr && m_ptr != nullptr) {
                ::GlobalUnlock(m_hglobal);
                m_ptr = nullptr;
            }
        }

        SafeGlobalLock(const SafeGlobalLock&) = delete;
        SafeGlobalLock& operator=(const SafeGlobalLock&) = delete;

        SafeGlobalLock(SafeGlobalLock&& other) noexcept : m_hglobal(other.m_hglobal), m_ptr(other.m_ptr) {
            other.m_hglobal = nullptr;
            other.m_ptr = nullptr;
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_ptr != nullptr; }
        [[nodiscard]] T* Get() const noexcept { return m_ptr; }
        [[nodiscard]] T* operator->() const noexcept { return m_ptr; }

    private:
        HGLOBAL m_hglobal;
        T* m_ptr;
    };

    // Encapsulador RAII para Menús de Windows
    class SafeMenu {
    public:
        constexpr SafeMenu() noexcept : m_menu(nullptr) {}
        explicit SafeMenu(HMENU m) noexcept : m_menu(m) {}

        ~SafeMenu() noexcept {
            Destroy();
        }

        SafeMenu(const SafeMenu&) = delete;
        SafeMenu& operator=(const SafeMenu&) = delete;

        SafeMenu(SafeMenu&& other) noexcept : m_menu(other.m_menu) {
            other.m_menu = nullptr;
        }

        SafeMenu& operator=(SafeMenu&& other) noexcept {
            if (this != &other) {
                Destroy();
                m_menu = other.m_menu;
                other.m_menu = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_menu != nullptr; }
        [[nodiscard]] HMENU Get() const noexcept { return m_menu; }

        HMENU Release() noexcept {
            HMENU temp = m_menu;
            m_menu = nullptr;
            return temp;
        }

        void Destroy() noexcept {
            if (m_menu != nullptr) {
                ::DestroyMenu(m_menu);
                m_menu = nullptr;
            }
        }

    private:
        HMENU m_menu;
    };

} // namespace RetroNotepad::Safe
