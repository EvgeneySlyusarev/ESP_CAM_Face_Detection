# build.ps1
# PowerShell script for building ESP32-CAM project with ESP-IDF

# --- Путь к твоему ESP-IDF ---
$IDF_PATH = "C:\Users\Evgenei\esp\v5.5\esp-idf"

# --- Активируем ESP-IDF ---
Write-Host "Activating ESP-IDF environment..."
& "$IDF_PATH\export.bat"

# --- Проверяем, что idf.py доступен ---
if (-Not (Get-Command "$IDF_PATH\tools\idf.py" -ErrorAction SilentlyContinue)) {
    Write-Error "idf.py not found! Check your ESP-IDF installation."
    exit 1
}

# --- Путь к проекту ---
$PROJECT_PATH = Get-Location

# --- Сборка проекта ---
Write-Host "Building project in $PROJECT_PATH ..."
python "$IDF_PATH\tools\idf.py" build --verbose

Write-Host "Build finished!"
