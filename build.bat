@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Compilando Retro Notepad (Release / x64)
echo ===================================================

set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VS_VCVARS%" (
    set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VS_VCVARS%" (
    set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VS_VCVARS%" (
    set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
)

call "%VS_VCVARS%" x64
if errorlevel 1 (
    echo [ERROR] No se pudo inicializar el entorno de compilacion de MSVC x64.
    exit /b 1
)

set "CMAKE_BIN=C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_BIN=C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

if not exist "%CMAKE_BIN%" (
    set "CMAKE_BIN=cmake"
)
if not exist "%NINJA_BIN%" (
    set "NINJA_BIN=ninja"
)

if not exist build (
    mkdir build
)

echo.
echo [1/2] Configurando CMake...
"%CMAKE_BIN%" -B build -G "Ninja" -DCMAKE_MAKE_PROGRAM="%NINJA_BIN%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERROR] Fallo la configuracion con CMake.
    exit /b 1
)

echo.
echo [2/2] Compilando con Ninja y MSVC (/W4 /WX /sdl /guard:cf /CETCOMPAT /MT)...
"%CMAKE_BIN%" --build build --config Release
if errorlevel 1 (
    echo [ERROR] Fallo la compilacion.
    exit /b 1
)

echo.
echo ===================================================
echo   Retro Notepad compilado con exito!
echo   Ubicacion: build\RetroNotepad.exe
echo ===================================================
