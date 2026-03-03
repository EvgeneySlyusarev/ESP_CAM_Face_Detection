# flash.ps1
# One-click build + flash script for ESP32-CAM project with ESP-IDF

param(
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"

# --- Путь к ESP-IDF ---
$IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5"

if (-not (Test-Path $IDF_PATH)) {
    throw "ESP-IDF not found: $IDF_PATH"
}

# --- Подбираем рабочее Python-окружение ESP-IDF ---
$pythonEnvRoot = "C:\Espressif\python_env"
$preferredEnvs = @(
    "idf5.5_py3.13_env",
    "idf5.5_py3.11_env",
    "idf5.5_py3.12_env",
    "idf5.5_py3.14_env"
)

$selectedEnv = $null
foreach ($envName in $preferredEnvs) {
    $candidateEnv = Join-Path $pythonEnvRoot $envName
    $candidatePython = Join-Path $candidateEnv "Scripts\python.exe"
    if (-not (Test-Path $candidatePython)) {
        continue
    }

    try {
        & $candidatePython -c "import rich" *> $null
        if ($LASTEXITCODE -eq 0) {
            $selectedEnv = $candidateEnv
            break
        }
    }
    catch {
    }
}

if (-not $selectedEnv) {
    throw "No compatible ESP-IDF Python environment found in $pythonEnvRoot. Run C:\Espressif\frameworks\esp-idf-v5.5\install.ps1 first."
}

$env:IDF_PYTHON_ENV_PATH = $selectedEnv
Write-Host "Using IDF_PYTHON_ENV_PATH=$selectedEnv"

# --- Активируем ESP-IDF ---
Write-Host "Activating ESP-IDF environment..."
. "$IDF_PATH\export.ps1"

$PROJECT_PATH = Get-Location
Write-Host "Build + flash project in $PROJECT_PATH ..."

if ($Port) {
    idf.py -p $Port build flash
}
elseif ($env:ESPPORT) {
    idf.py -p $env:ESPPORT build flash
}
else {
    idf.py build flash
}

Write-Host "Flash finished!"