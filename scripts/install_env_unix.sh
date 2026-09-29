#!/bin/bash
# Preparacion compartida: invocada por install_env_mac.command e install_env_linux.sh.
set -euo pipefail
trap 'echo "Error: la preparacion no termino. Corregi el error anterior y repeti este comando." >&2' ERR

kit_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
lesson_dir="$kit_dir"
if [[ ! -f "$lesson_dir/requirements.txt" || ! -f "$lesson_dir/practica/clasificar_textos.py" ]]; then
    echo "No se encontro la practica. Mantener la estructura completa del repositorio." >&2
    exit 1
fi
case "$(uname -s)" in
    Darwin|Linux) ;;
    *) echo "En Windows ejecutar install_env_windows.cmd." >&2; exit 1 ;;
esac
if ! command -v curl >/dev/null; then
    echo "Falta curl. En Ubuntu/Debian: sudo apt install curl" >&2
    exit 1
fi

tools_dir="$lesson_dir/.tools"
mkdir -p "$tools_dir/downloads"
export UV_UNMANAGED_INSTALL="$tools_dir/uv"
export UV_PYTHON_INSTALL_DIR="$tools_dir/python"
export UV_PYTHON_BIN_DIR="$tools_dir/bin"
export UV_CACHE_DIR="$tools_dir/cache"
export PATH="$UV_PYTHON_BIN_DIR:$PATH"  # Only this process; no shell profile edits.
uv_bin="$tools_dir/uv/uv"
echo "1/4 Preparando Python 3.11 para este kit..."
if [[ ! -x "$uv_bin" ]]; then
    curl --fail --show-error --location --retry 3 https://astral.sh/uv/install.sh -o "$tools_dir/downloads/uv-install.sh"
    sh "$tools_dir/downloads/uv-install.sh"
fi
venv_python="$lesson_dir/.venv/bin/python"
if [[ ! -x "$venv_python" ]]; then
    # Never clear a pre-existing, broken or moved environment automatically.
    if [[ -d "$lesson_dir/.venv" ]]; then
        echo "El entorno $lesson_dir/.venv esta incompleto o fue movido. Renombralo y repeti." >&2
        exit 1
    fi
    "$uv_bin" python install 3.11
    "$uv_bin" venv --python 3.11 --managed-python --seed "$lesson_dir/.venv"
fi

echo "2/4 Comprobando Ollama..."
ollama_bin="$(command -v ollama || true)"
if [[ -z "$ollama_bin" ]]; then
    for candidate in /Applications/Ollama.app/Contents/Resources/ollama "$HOME/Applications/Ollama.app/Contents/Resources/ollama" /usr/local/bin/ollama /usr/bin/ollama; do
        if [[ -x "$candidate" ]]; then ollama_bin="$candidate"; break; fi
    done
fi
if [[ -z "$ollama_bin" ]]; then
    if [[ "$(uname -s)" == Darwin && -d /Applications/Ollama.app ]]; then
        echo "Hay una app Ollama en /Applications pero no se encontro su ejecutable. Revisar esa instalacion antes de repetir." >&2
        exit 1
    fi
    echo "Descargando y ejecutando el instalador oficial de Ollama. Puede pedir permisos del sistema."
    curl --fail --show-error --location --retry 3 https://ollama.com/install.sh -o "$tools_dir/downloads/ollama-install.sh"
    sh "$tools_dir/downloads/ollama-install.sh"
fi

"$venv_python" "$kit_dir/setup_local.py"
