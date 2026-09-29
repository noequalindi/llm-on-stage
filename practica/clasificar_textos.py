"""Texto -> LLM generativo local -> propuesta visual. No entrena un clasificador."""
import argparse
import csv
import json
from pathlib import Path

from contrato import validar
from ollama_client import chat


DATA = Path(__file__).resolve().parents[1] / "llmPoster/bin/data"


def make_request(model, system, text):
    # The same native Ollama request is built by ofApp::send() in C++.
    return {"model": model, "stream": False, "format": "json",
            "messages": [{"role": "system", "content": system},
                         {"role": "user", "content": text}],
            "options": {"temperature": 0, "num_predict": 256}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", help="URL de Ollama; por defecto, la de connection.json")
    parser.add_argument("--model", help="Nombre real del modelo en Ollama")
    parser.add_argument("--text", default="La ciudad respira despacio mientras vuelve el sol.")
    parser.add_argument("--dataset", action="store_true", help="Comparar seis textos inventados; no es un benchmark")
    parser.add_argument("--recorded", action="store_true", help="Validar una respuesta escrita a mano; sin inferencia")
    args = parser.parse_args()
    if args.recorded:
        print("RESPUESTA ESCRITA A MANO / SIN INFERENCIA")
        print(validar((DATA / "recorded.json").read_text(encoding="utf-8")).model_dump_json(indent=2))
        return
    config = json.loads((DATA / "connection.json").read_text(encoding="utf-8"))
    url, model = args.url or config["ollama_url"], args.model or config["model"]
    system = (DATA / "system-prompt.txt").read_text(encoding="utf-8")
    rows = [{"texto": args.text, "etiqueta_propuesta": ""}]
    if args.dataset:
        with Path(__file__).with_name("textos.csv").open(encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
    for row in rows:
        print(f"Consultando {model} directamente en Ollama...", flush=True)
        raw = chat(url, make_request(model, system, row["texto"]))
        afiche = validar(raw)  # A model response is input to validate, not trusted code.
        print(json.dumps({"texto": row["texto"], "propuesta_humana": row["etiqueta_propuesta"],
                          "modelo": afiche.model_dump()}, ensure_ascii=False, indent=2))
    if args.dataset:
        print("Seis textos inventados para discutir: no prueban generalizacion ni ausencia de sesgos.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        raise SystemExit(f"Error: {error}") from error
