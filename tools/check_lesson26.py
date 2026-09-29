"""Check the compiled OF client and Python practice directly against Ollama.

--real uses the installed local model. Default uses a synthetic HTTP fixture.
Copies lesson files to an unrelated directory; no MVE or Python API server.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from threading import Thread


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--real", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    calls = []
    failure_case = ""
    recorded = json.loads((root / "llmPoster/bin/data/recorded.json").read_text())

    class Fixture(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            calls.append((self.path, body))
            assert self.path == "/api/chat" and body["stream"] is False
            assert body["messages"][1]["content"]
            data = json.dumps({"done": True, "message": {"role": "assistant", "content": json.dumps(recorded)}}).encode()
            if failure_case == "http": data = b'{"error":"test model missing"}'
            if failure_case == "json": data = b'not JSON'
            if failure_case == "incomplete": data = b'{"done":false}'
            if failure_case == "contract": data = b'{"done":true,"message":{"content":"{}"}}'
            self.send_response(404 if failure_case == "http" else 200); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data))); self.end_headers(); self.wfile.write(data)

    server = None
    try:
        url = "http://127.0.0.1:11434"
        if not args.real:
            server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
            Thread(target=server.serve_forever, daemon=True).start()
            url = f"http://127.0.0.1:{server.server_port}"
        with tempfile.TemporaryDirectory(prefix="clase LLM independiente ") as temporary:
            folder = Path(temporary)
            for name in ("practica", "llmPoster"):
                shutil.copytree(root / name, folder / name, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
            data = folder / "llmPoster/bin/data"
            config = json.loads((data / "connection.json").read_text())
            config["ollama_url"] = url
            (data / "connection.json").write_text(json.dumps(config))
            of = subprocess.run([str(args.executable.resolve()), "--check", str(data)],
                                cwd=folder, capture_output=True, text=True, timeout=200)
            assert of.returncode == 0 and "POSTER_CONTRACT_OK" in of.stdout + of.stderr, of.stdout + of.stderr
            print("OF -> Ollama -> JSON valido OK", flush=True)
            practice = subprocess.run([sys.executable, str(folder / "practica/clasificar_textos.py"), "--dataset"],
                                      cwd=folder, capture_output=True, text=True, timeout=600)
            assert practice.returncode == 0, practice.stdout + practice.stderr
            if not args.real:
                assert len(calls) == 7
                for failure_case, expected in (("http", "HTTP 404"), ("json", "objeto JSON"),
                                               ("incomplete", "incompleta"), ("contract", "cinco campos")):
                    failed = subprocess.run([str(args.executable.resolve()), "--check", str(data)],
                                            cwd=folder, capture_output=True, text=True, timeout=30)
                    assert failed.returncode != 0 and expected in failed.stdout + failed.stderr, failed.stdout + failed.stderr
                print("OF: HTTP errors, malformed/incomplete responses and invalid poster rejected")
            evidence = root / ".local/validation" / ("real" if args.real else "synthetic")
            evidence.mkdir(parents=True, exist_ok=True)
            (evidence / "of.txt").write_text(of.stdout + of.stderr)
            (evidence / "python.txt").write_text(practice.stdout)
            print("Python -> Ollama: seis textos validados; " + ("Qwen REAL" if args.real else "respuesta sintetica, sin inferencia"))
    finally:
        if server: server.shutdown(); server.server_close()


if __name__ == "__main__": main()
