"""Small HTTP client for Ollama. No server or model runs inside this module."""
import json
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import ProxyHandler, Request, build_opener


def chat(url, payload, *, timeout=180):
    """Devuelve solo el texto del modelo (message.content)."""
    return chat_detalle(url, payload, timeout=timeout)[0]


def chat_detalle(url, payload, *, timeout=180):
    """Devuelve (texto del modelo, respuesta completa de Ollama).

    La respuesta completa incluye, por ejemplo, prompt_eval_count: cuántos tokens
    ocupó el pedido (instrucciones + historial + mensaje).
    """
    if urlsplit(url).scheme not in ("http", "https"):
        raise ValueError("La URL de Ollama debe empezar con http:// o https://")
    request = Request(url.rstrip("/") + "/api/chat",
                      data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
                      headers={"Content-Type": "application/json"}, method="POST")
    try:
        # The classroom connects locally; ignore inherited HTTP proxies.
        with build_opener(ProxyHandler({})).open(request, timeout=timeout) as response:
            raw = response.read(1024 * 1024 + 1)
    except HTTPError as exc:
        detail = exc.read(4096).decode("utf-8", errors="replace")
        raise RuntimeError(f"Ollama HTTP {exc.code}: {detail}") from exc
    except (URLError, OSError) as exc:
        raise RuntimeError(f"No se pudo consultar Ollama en {url}. Abrir Ollama y revisar el modelo. {exc}") from exc
    if len(raw) > 1024 * 1024:
        raise ValueError("Respuesta de Ollama demasiado grande")
    try:
        body = json.loads(raw)
    except (ValueError, UnicodeDecodeError) as exc:
        raise ValueError("Ollama no devolvio JSON valido") from exc
    if not isinstance(body, dict):
        raise ValueError("Respuesta de Ollama invalida")
    if body.get("error"):
        raise RuntimeError(f"Ollama: {body['error']}")
    message = body.get("message")
    if body.get("done") is not True or not isinstance(message, dict) or not isinstance(message.get("content"), str):
        raise ValueError("Ollama no devolvio un mensaje completo")
    if body.get("done_reason") == "length":
        raise ValueError("El modelo alcanzo el limite de tokens; acortar el pedido o aumentar num_predict")
    if not message["content"].strip():
        raise ValueError("El modelo devolvio texto vacio")
    return message["content"], body
