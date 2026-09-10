# ==============================================================================
# Retro Notepad - Generador de Suite de Pruebas Sintéticas QA & Seguridad
# ==============================================================================
param (
    [string]$OutputDir = "$PSScriptRoot\test_suite"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host " Generando Suite Sintetica de Pruebas QA para Retro Notepad" -ForegroundColor Cyan
Write-Host " Directorio destino: $OutputDir" -ForegroundColor Gray
Write-Host "==================================================================" -ForegroundColor Cyan

$spanishText = @"
Retro Notepad - Archivo de Prueba de Caracteres en Espanol
----------------------------------------------------------
Vocales con tilde: a, e, i, o, u, A, E, I, O, U
Letra enye: n, N
Dieresis: u, U
Signos de apertura: ? !
Simbolos y puntuacion: (C), (R), TM, EUR, $, 100%, 1/2, << >>
Texto de prueba: El murcielago hindu comia feliz cardillo y kiwi. La ciguena tocaba el saxofon detras del palenque de paja.
"@

# 1. UTF-8 con BOM (0xEF, 0xBB, 0xBF)
$utf8BomPath = Join-Path $OutputDir "test_utf8_bom.txt"
$utf8BomEncoding = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText($utf8BomPath, $spanishText, $utf8BomEncoding)
Write-Host " [+] Generado: test_utf8_bom.txt (UTF-8 con BOM)" -ForegroundColor Green

# 2. UTF-8 sin BOM
$utf8NoBomPath = Join-Path $OutputDir "test_utf8_nobom.txt"
$utf8NoBomEncoding = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($utf8NoBomPath, $spanishText, $utf8NoBomEncoding)
Write-Host " [+] Generado: test_utf8_nobom.txt (UTF-8 sin BOM con acentos)" -ForegroundColor Green

# 3. ANSI (Windows-1252 / Codepage 1252)
$ansiPath = Join-Path $OutputDir "test_ansi.txt"
$ansiEncoding = [System.Text.Encoding]::GetEncoding(1252)
[System.IO.File]::WriteAllText($ansiPath, $spanishText, $ansiEncoding)
Write-Host " [+] Generado: test_ansi.txt (ANSI Windows-1252 con acentos)" -ForegroundColor Green

# 4. UTF-16 LE con BOM (0xFF, 0xFE)
$utf16LEPath = Join-Path $OutputDir "test_utf16le.txt"
$utf16LEEncoding = [System.Text.Encoding]::Unicode
[System.IO.File]::WriteAllText($utf16LEPath, $spanishText, $utf16LEEncoding)
Write-Host " [+] Generado: test_utf16le.txt (UTF-16 Little Endian con BOM)" -ForegroundColor Green

# 5. UTF-16 BE con BOM (0xFE, 0xFF)
$utf16BEPath = Join-Path $OutputDir "test_utf16be.txt"
$utf16BEEncoding = [System.Text.Encoding]::BigEndianUnicode
[System.IO.File]::WriteAllText($utf16BEPath, $spanishText, $utf16BEEncoding)
Write-Host " [+] Generado: test_utf16be.txt (UTF-16 Big Endian con BOM)" -ForegroundColor Green

# 6. UTF-16 LE sin BOM (Prueba para IsTextUnicode)
$utf16LENoBomPath = Join-Path $OutputDir "test_utf16le_nobom.txt"
$rawBytesLE = [System.Text.Encoding]::Unicode.GetBytes($spanishText)
# WriteAllBytes writes raw bytes without BOM
[System.IO.File]::WriteAllBytes($utf16LENoBomPath, $rawBytesLE)
Write-Host " [+] Generado: test_utf16le_nobom.txt (UTF-16 LE sin BOM)" -ForegroundColor Green

# 7. Unix Line Endings (LF puro '\n')
$unixLfPath = Join-Path $OutputDir "test_unix_lf.txt"
$unixText = "Linea 1`nLinea 2`nLinea 3 con acentos: Murcielago`nLinea 4 final`n"
[System.IO.File]::WriteAllBytes($unixLfPath, [System.Text.Encoding]::UTF8.GetBytes($unixText))
Write-Host " [+] Generado: test_unix_lf.txt (Terminador de linea Unix LF '\n')" -ForegroundColor Green

# 8. Macintosh Clásico Line Endings (CR puro '\r')
$macCrPath = Join-Path $OutputDir "test_mac_cr.txt"
$macText = "Linea 1`rLinea 2`rLinea 3 con acentos: Murcielago`rLinea 4 final`r"
[System.IO.File]::WriteAllBytes($macCrPath, [System.Text.Encoding]::UTF8.GetBytes($macText))
Write-Host " [+] Generado: test_mac_cr.txt (Terminador de linea Mac CR '\r')" -ForegroundColor Green

# 9. Mezcla Heterogénea de Terminadores de Línea (CRLF + LF + CR)
$mixedPath = Join-Path $OutputDir "test_mixed_endings.txt"
$mixedText = "Linea 1 CRLF`r`nLinea 2 LF puro`nLinea 3 CR aislado`rLinea 4 CRLF final`r`n"
[System.IO.File]::WriteAllBytes($mixedPath, [System.Text.Encoding]::UTF8.GetBytes($mixedText))
Write-Host " [+] Generado: test_mixed_endings.txt (Mezcla de CRLF, LF y CR)" -ForegroundColor Green

# 10. Archivo Vacío (0 Bytes)
$emptyPath = Join-Path $OutputDir "test_empty.txt"
[System.IO.File]::WriteAllBytes($emptyPath, [byte[]]@())
Write-Host " [+] Generado: test_empty.txt (0 bytes / archivo vacio)" -ForegroundColor Green

# 11. Archivo UTF-16 Truncado con Número Impar de Bytes
$oddPath = Join-Path $OutputDir "test_odd_bytes.txt"
$oddBytes = [System.Text.Encoding]::Unicode.GetBytes("Texto truncado impar")
# Truncar o agregar 1 byte huérfano
$oddBytesWithBOM = @(0xFF, 0xFE) + $oddBytes + @(0x41) # 1 byte extra
[System.IO.File]::WriteAllBytes($oddPath, $oddBytesWithBOM)
Write-Host " [+] Generado: test_odd_bytes.txt (UTF-16 con byte huérfano impar)" -ForegroundColor Green

# 12. Archivo Grande para Pruebas de Estrés (50 MB)
$largePath = Join-Path $OutputDir "test_large_50mb.txt"
Write-Host " [*] Generando test_large_50mb.txt (50 MB, por favor espere)..." -ForegroundColor Yellow
$lineSample = "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz Retro Notepad Stress Test Line `r`n"
$lineBytes = [System.Text.Encoding]::UTF8.GetBytes($lineSample)
$targetBytes = 50 * 1024 * 1024 # 50 MB
$fileStream = [System.IO.File]::Create($largePath)
try {
    $written = 0
    while ($written -lt $targetBytes) {
        $fileStream.Write($lineBytes, 0, $lineBytes.Length)
        $written += $lineBytes.Length
    }
} finally {
    $fileStream.Close()
}
$largeFileInfo = Get-Item $largePath
$largeMb = [math]::Round($largeFileInfo.Length / 1MB, 2)
Write-Host " [+] Generado: test_large_50mb.txt ($largeMb MB)" -ForegroundColor Green

Write-Host "`n==================================================================" -ForegroundColor Cyan
Write-Host " Suite generada exitosamente. Total de archivos: 12" -ForegroundColor Cyan
Write-Host "==================================================================" -ForegroundColor Cyan
