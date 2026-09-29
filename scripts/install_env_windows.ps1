# Windows PowerShell 5.1+. La entrada para alumnos es install_env_windows.cmd.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Fallo $Executable (codigo $LASTEXITCODE)." }
}

function Find-Ollama {
    $command = Get-Command ollama.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $candidate = Join-Path $env:LOCALAPPDATA 'Programs\Ollama\ollama.exe'
    if (Test-Path $candidate) { return $candidate }
    return $null
}

try {
    $kitDir = Split-Path -Parent $PSScriptRoot
    $lessonDir = $kitDir
    if (-not (Test-Path (Join-Path $lessonDir 'requirements.txt')) -or -not (Test-Path (Join-Path $lessonDir 'practica\clasificar_textos.py'))) {
        throw 'No se encontro la practica. Mantener la estructura completa del repositorio.'
    }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $toolsDir = Join-Path $lessonDir '.tools'
    $downloads = Join-Path $toolsDir 'downloads'
    New-Item -ItemType Directory -Force -Path $downloads | Out-Null
    $env:UV_UNMANAGED_INSTALL = Join-Path $toolsDir 'uv'
    $env:UV_PYTHON_INSTALL_DIR = Join-Path $toolsDir 'python'
    $env:UV_PYTHON_BIN_DIR = Join-Path $toolsDir 'bin'
    $env:UV_CACHE_DIR = Join-Path $toolsDir 'cache'
    $env:PATH = $env:UV_PYTHON_BIN_DIR + ';' + $env:PATH
    $uvBin = Join-Path $toolsDir 'uv\uv.exe'
    Write-Host '1/4 Preparando Python 3.11 para este kit...'
    if (-not (Test-Path $uvBin)) {
        $uvInstaller = Join-Path $downloads 'uv-install.ps1'
        Invoke-WebRequest -UseBasicParsing 'https://astral.sh/uv/install.ps1' -OutFile $uvInstaller
        Invoke-Checked 'powershell.exe' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $uvInstaller)
    }
    $venvDir = Join-Path $lessonDir '.venv'
    $python = Join-Path $venvDir 'Scripts\python.exe'
    if (-not (Test-Path $python)) {
        if (Test-Path $venvDir) { throw "$venvDir esta incompleto o fue movido. Renombrarlo y repetir." }
        Invoke-Checked $uvBin @('python', 'install', '3.11')
        Invoke-Checked $uvBin @('venv', '--python', '3.11', '--managed-python', '--seed', $venvDir)
    }
    Write-Host '2/4 Comprobando Ollama...'
    if (-not (Find-Ollama)) {
        $ollamaInstaller = Join-Path $downloads 'OllamaSetup.exe'
        Write-Host 'Descargando Ollama. Completar su asistente de instalacion cuando se abra.'
        Invoke-WebRequest -UseBasicParsing 'https://ollama.com/download/OllamaSetup.exe' -OutFile $ollamaInstaller
        $install = Start-Process -FilePath $ollamaInstaller -Wait -PassThru
        if ($install.ExitCode -ne 0) { throw "Instalacion de Ollama cancelada o fallida ($($install.ExitCode))." }
        # Refresh PATH in this process after the official installer updates it.
        $env:PATH = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
        if (-not (Find-Ollama)) { throw 'No se encontro Ollama. Completar la instalacion y repetir.' }
    }
    Invoke-Checked $python @((Join-Path $kitDir 'setup_local.py'))
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
exit 0
