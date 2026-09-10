# 📝 Retro Notepad

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6.svg)](https://microsoft.com/windows)
[![Architecture](https://img.shields.io/badge/Architecture-Pure%20Win32%20API-success.svg)]()
[![Security](https://img.shields.io/badge/Hardening-CFG%20%7C%20CET%20%7C%20ASLR%20%7C%20DEP-red.svg)]()
[![QA](https://img.shields.io/badge/QA%20Tests-17%2F17%20Passed-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**Retro Notepad** es una recreación nativa 1:1, ultra rápida y de alta seguridad del clásico **Bloc de notas de Windows**, desarrollada en **C++20 estándar** utilizando exclusivamente la **API Win32 pura**.

Diseñado bajo la filosofía **Zero-Dependency** (sin frameworks pesados ni librerías de terceros), el ejecutable final pesa **menos de 300 KB**, arranca de manera instantánea y cuenta con un blindaje moderno contra fallas de seguridad y corrupción de datos.

---

## ✨ Características Principales

* 🎨 **Tema Dinámico (Claro / Oscuro / Acorde al Sistema):**
  * Soporte nativo para *Immersive Dark Mode* en la barra de título (DWM).
  * Renderizado personalizado no-cliente (`UAHMENU`) para menús oscuros continuos sin bordes blancos.
  * Barra de estado con repintado adaptativo (`NM_CUSTOMDRAW`).
* 🌐 **Soporte Multilenguaje Instantáneo:**
  * Conmutación en caliente entre **Español** e **Inglés** para menús, diálogos, atajos y barra de estado.
* 📊 **Barra de Estado Interactiva de 5 Paneles:**
  * **Línea y Columna:** Cálculo en tiempo real con sanitización ante saltos de línea y EOF.
  * **Conteo de Caracteres Reactivo:** Visualización del total y conteo dinámico durante la selección de texto (`X de Y caracteres`).
  * **Zoom:** Escala porcentual configurable con límites seguros (10% a 500%).
  * **Formato de Fin de Línea:** Detección de Windows (`CRLF`), Unix (`LF`) y Macintosh (`CR`).
  * **Codificación:** Detección automática de `UTF-8`, `UTF-8 con BOM`, `UTF-16 LE`, `UTF-16 BE` y `ANSI`.
* 🛡️ **Guardado Atómico Transaccional:**
  * Protección contra pérdida de información ante cortes eléctricos, discos llenos o fallos de red: escribe a un temporal intermedio (`.tmp~`) con `FlushFileBuffers` y reemplaza atómicamente con `ReplaceFileW` / `MoveFileExW`.
* 💾 **Protección de Sesión del Sistema Operativo:**
  * Manejo estricto de `WM_QUERYENDSESSION` y `WM_ENDSESSION`. Si hay cambios sin guardar al apagar o reiniciar el equipo, se solicita confirmación al usuario antes de permitir el cierre.
* 🔍 **Búsqueda y Reemplazo Modeless (No Modal):**
  * Diálogos nativos `FindTextW` y `ReplaceTextW` con soporte para *Wrap Around*, atajos F3 / Mayús+F3 y blindaje contra excepciones de desbordamiento de cadenas (`std::out_of_range`).
* 🔎 **Control de Zoom Preciso:**
  * Zoom mediante `Ctrl + Rueda de ratón`, `Ctrl + +`, `Ctrl + -` y restablecimiento con `Ctrl + 0`.
  * Clamp estricto para evitar glitches de tamaño en zooms mínimos.
* ↩️ **Sincronización Reactiva de Deshacer (`Ctrl + Z`):**
  * El asterisco `*` de modificación en la barra de título desaparece si el usuario deshace sus cambios hasta el estado original guardado (`EM_GETMODIFY`).
* 🖨️ **Impresión Segura con RAII:**
  * Integración con `PrintDlgW` encapsulando los handles del sistema (`HDC`, `HGLOBAL`) en tipos RAII libres de fugas de recursos.

---

## 🔒 Hardening de Seguridad y Mitigación de Exploits

El proyecto se compila bajo políticas de máxima exigencia de seguridad:

| Bandera | Mitigación |
| :--- | :--- |
| `/guard:cf` | **Control Flow Guard (CFG)** contra secuestros de flujo de ejecución indirecto. |
| `/CETCOMPAT` | **Intel CET / Shadow Stack** contra ataques de Programación Orientada al Retorno (ROP). |
| `/DYNAMICBASE` & `/HIGHENTROPYVA` | **ASLR Completo de 64 bits** (Address Space Layout Randomization). |
| `/NXCOMPAT` | **Prevención de Ejecución de Datos (DEP)**. |
| `/GS` & `/sdl` | Verificación de integridad de buffer en stack y SDL (Security Development Lifecycle). |
| `/W4` & `/WX` | Cero advertencias toleradas (cualquier advertencia interrumpe la compilación). |
| `/MT` | Enlazado estático de la CRT para ejecución 100% autónoma y portable. |

---

## 📁 Estructura del Código

```text
RETRO NOTEPAD/
├── main.cpp                     # Punto de entrada wWinMain, mensaje loop y aceleradores
├── MainWindow.hpp / .cpp        # Ventana principal Win32, subclassing del control EDIT, temas y comandos
├── EncodingDetector.hpp / .cpp  # Motor de codificaciones (BOM, UTF-8/16, ANSI, LF/CRLF, guardado atómico)
├── FindReplaceController.hpp/.cpp # Controlador de diálogos modeless de búsqueda y reemplazo
├── Localization.hpp / .cpp      # Diccionario y subsistema multilenguaje en tiempo real
├── SafeWin32.hpp                # Envoltorios RAII de cero coste (SafeHandle, SafeGdiObject, SafeDC, SafeModule)
├── resource.h / RetroNotepad.rc # Tabla de cadenas, aceleradores, iconos y manifiesto de recursos
├── manifest.xml                 # Manifiesto Per-Monitor V2 DPI y controles comunes v6
├── build.bat                    # Script de compilación desatendida de 1 clic (Release x64)
├── generate_test_suite.ps1      # Generador de batería de pruebas sintéticas (12 archivos)
└── run_qa_suite.ps1             # Runner automatizado de certificación QA y regresión (17 pruebas)
```

---

## 🛠️ Compilación y Construcción

### Requisitos Previos
* Windows 10 versión 1903 o superior / Windows 11 (x64).
* **Visual Studio 2022 / 2026** (con la carga de trabajo *"Desarrollo para el escritorio con C++"*).
* **CMake 3.20+** y **Ninja** (incluidos por defecto en el instalador de Visual Studio).

### Compilación Rápida (1 Clic)
Abre un terminal o el *Developer Command Prompt* en la carpeta del repositorio y ejecuta:

```cmd
build.bat
```

El script configurará CMake y compilará el ejecutable optimizado en:
```text
build\RetroNotepad.exe
```

### Compilación Manual con CMake
```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

---

## 🧪 Certificación y Pruebas Automatizadas

El proyecto incluye una batería de pruebas de regresión y seguridad automatizada. Para ejecutarla:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_qa_suite.ps1
```

La suite valida de forma automática:
1. **Fase 1 (Ingestión de Encodings):** Carga segura de UTF-8 (con/sin BOM), UTF-16 LE/BE, ANSI, Unix LF, Mac CR, mezclas de saltos de línea y bytes truncados.
2. **Fase 2 (Protección DoS):** Bloqueo y respuesta ante nombres reservados de Windows (`CON.txt`, `PRN.txt`, `AUX.txt`, `NUL.txt`).
3. **Fase 3 (Integridad Atómica):** Garantía de no truncamiento prematuro de archivos en disco.
4. **Fase 4 (Prueba de Estrés):** Carga y estabilidad con archivos de **50 MB**.

---

## 📄 Licencia

Este proyecto se distribuye bajo la licencia **MIT**. Consulta el archivo [LICENSE](LICENSE) para más detalles.

Desarrollado por **Daniel Valenzuela** (2026).
