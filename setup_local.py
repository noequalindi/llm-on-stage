"""Second stage of the install_env_mac / install_env_linux / install_env_windows launchers.

Can also run with an existing Python >=3.11 and Ollama installation.
Starts local Ollama if needed, prepares the Python practice, and downloads/verifies Qwen.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from urllib.error import URLError
from urllib.request import ProxyHandler, Request, build_opener


MODEL = "qwen2.5:1.5b-instruct"
OLLAMA_URL = "http://127.0.0.1:11434"


def find_ollama():
    candidates = [shutil.which("ollama"),
                  "/Applications/Ollama.app/Contents/Resources/ollama",
                  Path.home() / "Applications/Ollama.app/Contents/Resources/ollama",
                  "/usr/local/bin/ollama", "/usr/bin/ollama"]
    if os.environ.get("LOCALAPPDATA"):
        candidates.append(Path(os.environ["LOCALAPPDATA"]) / "Programs/Ollama/ollama.exe")
    for candidate in candidates:
        if candidate and Path(candidate).is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    raise RuntimeError("No se encontro Ollama. Ejecutar install_env_mac.command, install_env_linux.sh o install_env_windows.cmd de la raiz del repositorio.")


def ollama_request(route, payload=None, timeout=2):
    # The classroom config is local: do not inherit proxy or remote Ollama hosts.
    request = Request(OLLAMA_URL + route,
                      data=json.dumps(payload).encode() if payload is not None else None,
                      headers={"Content-Type": "application/json"})
    with build_opener(ProxyHandler({})).open(request, timeout=timeout) as response:
        return json.load(response)


def ollama_tags():
    data = ollama_request("/api/tags")
    if not isinstance(data, dict) or not isinstance(data.get("models"), list):
        raise ValueError("La respuesta en 11434 no corresponde a Ollama")
    return data


def check_model(ollama):
    """Preload using Ollama's scheduler, then show its actual placement report."""
    print("Cargando Qwen en memoria; Ollama elige GPU/CPU segun hardware y memoria...", flush=True)
    # No num_gpu override: allow full GPU, partial offload or CPU as appropriate.
    # Empty prompt preloads without generating text; do not keep memory forever.
    loaded = ollama_request("/api/generate", {
        "model": MODEL, "stream": False, "keep_alive": "5m",
    }, timeout=300)
    if not isinstance(loaded, dict) or loaded.get("done") is not True:
        raise RuntimeError("Ollama no confirmo la carga de Qwen en memoria")
    running = ollama_request("/api/ps")
    models = running.get("models", []) if isinstance(running, dict) else []
    if not isinstance(models, list) or not any(
        isinstance(model, dict) and model.get("name") == MODEL for model in models
    ):
        raise RuntimeError("Qwen no aparece cargado en Ollama. Repetir --check-model y revisar sus logs")
    print("Distribucion actual (columna PROCESSOR):", flush=True)
    subprocess.run([ollama, "ps"], check=True, timeout=15,
                   env=dict(os.environ, OLLAMA_HOST=OLLAMA_URL))
    print("100% GPU = cargado en GPU; 100% CPU = memoria de CPU; CPU/GPU = carga mixta.")
    print("Son datos de ubicacion en memoria, no porcentajes de uso ni una medicion de velocidad.")
    print("La precarga dura 5 minutos sin uso. OF vuelve a cargar el modelo al consultarlo si se libero.")


def ready():
    try:
        ollama_tags()
        return True
    except (URLError, OSError, ValueError):
        return False


def ensure_ollama(ollama, server, timeout=60):
    """Reuse an existing server; leave a successfully started own process running."""
    if ready():
        return
    log_path = server / ".local/ollama-setup.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, OLLAMA_HOST=OLLAMA_URL)
    options = {"start_new_session": True} if sys.platform != "win32" else {
        "creationflags": subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP}
    print("Iniciando Ollama local en 127.0.0.1:11434...", flush=True)
    with log_path.open("ab") as log:
        process = subprocess.Popen([ollama, "serve"], stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=log, env=env, **options)
    deadline = time.monotonic() + timeout
    try:
        # An app/service may be starting concurrently. Recheck readiness even if
        # our process exits because that app already acquired the port.
        while time.monotonic() < deadline:
            if ready():
                return
            time.sleep(1)
        raise RuntimeError(f"Ollama no respondio en {OLLAMA_URL}. Revisar {log_path}")
    except BaseException:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        raise


def repository_directory(lesson):
    """Resolve only this repository, regardless of its name or parent directory."""
    server = Path(lesson).resolve()
    if (server / "requirements.txt").is_file() and (server / "practica/clasificar_textos.py").is_file():
        return server
    raise RuntimeError("No se encontro la practica. Mantener la estructura completa del repositorio.")



def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-model", action="store_true",
                        help="Precargar Qwen y mostrar CPU/GPU sin reinstalar ni descargar")
    args = parser.parse_args(argv)
    if sys.version_info < (3, 11):
        raise SystemExit("Se necesita Python 3.11 o posterior")
    root = Path(__file__).resolve().parent
    server = repository_directory(root)
    ollama = find_ollama()
    ensure_ollama(ollama, server)
    if args.check_model:
        check_model(ollama)
        return
    environment = server / ".venv"
    python = environment / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python")
    if not python.is_file():
        if environment.exists():
            raise RuntimeError(f"{environment} esta incompleto o fue movido. Renombrarlo y repetir la instalacion.")
        subprocess.run([sys.executable, "-m", "venv", str(environment)], check=True)
    subprocess.run([str(python), "-c", "import sys; assert sys.version_info >= (3, 11), 'Se necesita Python 3.11+'"], check=True)
    print("3/4 Instalando dependencias de la practica Python...", flush=True)
    subprocess.run([str(python), "-m", "pip", "install", "-r", str(server / "requirements.txt")], check=True)
    print(f"4/4 Descargando/verificando {MODEL}...", flush=True)
    subprocess.run([ollama, "pull", MODEL], check=True,
                   env=dict(os.environ, OLLAMA_HOST=OLLAMA_URL))
    if not any(model.get("name") == MODEL for model in ollama_tags()["models"]):
        raise RuntimeError("La descarga termino pero Qwen no aparece en el Ollama local. Repetir la preparacion.")
    check_model(ollama)
    print("Preparado. Python, Ollama y Qwen disponibles.")
    print("Interfaz Python: ejecutar practica/interfaz.py con el Python de .venv.")
    print("Abrir llmPoster: Enviar en CHAT o Interpretar en AFICHE. OF conecta directamente con Ollama.")
    print("Ollama queda iniciado. Despues de reiniciar la computadora: abrir Ollama o repetir el instalador.")
    print("No mover la carpeta instalada: .tools y .venv contienen rutas locales.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f"Error: {error}. La preparacion NO termino.") from error
