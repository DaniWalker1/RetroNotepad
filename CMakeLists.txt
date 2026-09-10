cmake_minimum_required(VERSION 3.20)
project(RetroNotepad LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enlazado estático de la CRT (/MT en Release, /MTd en Debug) para binario 100% autónomo (Zero-Dependency)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Fuentes del proyecto
set(SOURCES
    main.cpp
    MainWindow.hpp
    MainWindow.cpp
    Localization.hpp
    Localization.cpp
    EncodingDetector.hpp
    EncodingDetector.cpp
    FindReplaceController.hpp
    FindReplaceController.cpp
    SafeWin32.hpp
    resource.h
    RetroNotepad.rc
)

add_executable(RetroNotepad WIN32 ${SOURCES})

# Definiciones de preprocesador para compatibilidad Unicode y seguridad
target_compile_definitions(RetroNotepad PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    STRICT
)

if (MSVC)
    # Banderas de compilador: Hardening de seguridad estricto y optimización
    target_compile_options(RetroNotepad PRIVATE
        /W4
        /WX
        /sdl
        /GS
        /guard:cf
        /utf-8
        /permissive-
        /Zc:__cplusplus
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
    )

    # Banderas de enlazador: Mitigaciones de exploits modernas, ASLR 64-bit, DEP, CET y optimización de tamaño (< 300 KB)
    target_link_options(RetroNotepad PRIVATE
        /SUBSYSTEM:WINDOWS
        /MANIFEST:NO
        /NXCOMPAT
        /DYNAMICBASE
        /HIGHENTROPYVA
        /guard:cf
        /CETCOMPAT
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()

# Bibliotecas de Windows del sistema
target_link_libraries(RetroNotepad PRIVATE
    comctl32
    comdlg32
    shell32
    user32
    gdi32
    advapi32
    dwmapi
    uxtheme
)
