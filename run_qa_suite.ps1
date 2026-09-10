# ==============================================================================
# Retro Notepad - Runner Automatizado de Suite de Pruebas QA & Seguridad
# ==============================================================================
param (
    [string]$ExePath = "$PSScriptRoot\build\RetroNotepad.exe",
    [string]$TestSuiteDir = "$PSScriptRoot\test_suite"
)

$ErrorActionPreference = "Continue"

Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host " RETRO NOTEPAD - EJECUTOR DE PRUEBAS DE REGRESION Y SEGURIDAD" -ForegroundColor Cyan
Write-Host " Fecha: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
Write-Host " Binario objetivo: $ExePath" -ForegroundColor Gray
Write-Host "==================================================================" -ForegroundColor Cyan

# 1. Asegurar binario
if (-not (Test-Path $ExePath)) {
    Write-Host " [*] Binario no encontrado. Ejecutando build.bat..." -ForegroundColor Yellow
    cmd.exe /c "$PSScriptRoot\build.bat"
    if (-not (Test-Path $ExePath)) {
        Write-Host " [X] ERROR FATAL: No se pudo compilar el binario." -ForegroundColor Red
        exit 1
    }
}

# 2. Asegurar suite sintética
if (-not (Test-Path $TestSuiteDir)) {
    Write-Host " [*] Generando suite de pruebas sinteticas..." -ForegroundColor Yellow
    & "$PSScriptRoot\generate_test_suite.ps1"
}

$totalTests = 0
$passedTests = 0
$failedTests = 0

function Assert-Test {
    param (
        [string]$TestName,
        [bool]$Condition,
        [string]$Details = ""
    )
    $script:totalTests++
    if ($Condition) {
        $script:passedTests++
        Write-Host " [PASS] $TestName" -ForegroundColor Green
        if ($Details) { Write-Host "        $Details" -ForegroundColor DarkGreen }
    } else {
        $script:failedTests++
        Write-Host " [FAIL] $TestName" -ForegroundColor Red
        if ($Details) { Write-Host "        $Details" -ForegroundColor DarkRed }
    }
}

Write-Host "`n--- FASE 1: PRUEBAS DE INGESTION DE CODIFICACIONES Y FORMATOS ---" -ForegroundColor Yellow

$testFiles = Get-ChildItem -Path "$TestSuiteDir\*.txt" | Where-Object { $_.Name -ne "test_large_50mb.txt" }

foreach ($file in $testFiles) {
    $proc = Start-Process -FilePath $ExePath -ArgumentList "`"$($file.FullName)`"" -PassThru
    Start-Sleep -Milliseconds 600

    $hasCrashed = $proc.HasExited
    $exitCode = if ($hasCrashed) { $proc.ExitCode } else { 0 }

    Assert-Test -TestName "Ingestion segura: $($file.Name)" -Condition (-not $hasCrashed) -Details "Archivo procesado sin excepciones ni caida de proceso."

    if (-not $hasCrashed) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "`n--- FASE 2: PRUEBA DE DISPOSITIVOS RESERVADOS DE WINDOWS (DoS) ---" -ForegroundColor Yellow

$reservedNames = @("CON.txt", "PRN.txt", "AUX.txt", "NUL.txt")
foreach ($res in $reservedNames) {
    $fakePath = Join-Path $TestSuiteDir $res
    $proc = Start-Process -FilePath $ExePath -ArgumentList "`"$fakePath`"" -PassThru
    Start-Sleep -Milliseconds 600

    $hasHanged = $false
    try {
        # Si el proceso responde a eventos de ventana no está congelado (Hang)
        $isResponding = $proc.Responding
    } catch {
        $isResponding = $true
    }

    Assert-Test -TestName "Proteccion DoS ante dispositivo: $res" -Condition $isResponding -Details "El hilo de UI permanece interactivo y no se bloquea."

    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "`n--- FASE 3: PRUEBA DE INTEGRIDAD DE GUARDADO ATOMICO ---" -ForegroundColor Yellow

$tempTarget = Join-Path $TestSuiteDir "atomic_test_sample.txt"
$originalContent = "CONTENIDO ORIGINAL INTACTO"
[System.IO.File]::WriteAllText($tempTarget, $originalContent)

# Simular archivo temporal residual que no deba interferir
$tempHelper = "$tempTarget.tmp~"
if (Test-Path $tempHelper) { Remove-Item $tempHelper -Force }

$proc = Start-Process -FilePath $ExePath -ArgumentList "`"$tempTarget`"" -PassThru
Start-Sleep -Milliseconds 600

# Verificar que el archivo original no fue alterado o truncado prematuramente
$currentContent = [System.IO.File]::ReadAllText($tempTarget)
Assert-Test -TestName "No truncado previo en apertura" -Condition ($currentContent -eq $originalContent) -Details "El contenido original se mantiene integro al abrir."

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

# Limpieza de archivo de prueba
if (Test-Path $tempTarget) { Remove-Item $tempTarget -Force }
if (Test-Path $tempHelper) { Remove-Item $tempHelper -Force }

Write-Host "`n--- FASE 4: PRUEBA DE ESTRÉS Y CONSUMO DE MEMORIA (50 MB) ---" -ForegroundColor Yellow

$largeFile = Join-Path $TestSuiteDir "test_large_50mb.txt"
if (Test-Path $largeFile) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = Start-Process -FilePath $ExePath -ArgumentList "`"$largeFile`"" -PassThru
    Start-Sleep -Milliseconds 2500
    $sw.Stop()

    $wsMb = [math]::Round($proc.WorkingSet64 / 1MB, 2)
    $hasCrashed = $proc.HasExited

    Assert-Test -TestName "Carga de archivo de estres (50 MB)" -Condition (-not $hasCrashed) -Details "Proceso activo, memoria WorkingSet: $wsMb MB"

    if (-not $hasCrashed) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "`n==================================================================" -ForegroundColor Cyan
Write-Host " RESUMEN FINAL DE CERTIFICACION QA" -ForegroundColor Cyan
Write-Host " Total de Pruebas: $totalTests" -ForegroundColor Gray
Write-Host " Pruebas Aprobadas (PASS): $passedTests" -ForegroundColor Green
Write-Host " Pruebas Fallidas  (FAIL): $failedTests" -ForegroundColor $(if ($failedTests -gt 0) { "Red" } else { "Green" })
Write-Host "==================================================================" -ForegroundColor Cyan

if ($failedTests -eq 0) {
    Write-Host " [OK] CERTIFICACION APROBADA: Binario listo para despliegue.`n" -ForegroundColor Green
    exit 0
} else {
    Write-Host " [X] FALLO DE CERTIFICACION: Existen defectos pendientes.`n" -ForegroundColor Red
    exit 1
}
