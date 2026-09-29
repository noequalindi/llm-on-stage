"""Direct transport failures must not become a valid poster response."""
import json
from pathlib import Path
import sys
from unittest.mock import Mock
from urllib.error import HTTPError, URLError
from io import BytesIO

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "practica"))
import ollama_client
from clasificar_textos import make_request
from contrato import validar


def response(monkeypatch, body):
    stream = Mock()
    stream.__enter__ = Mock(return_value=stream)
    stream.__exit__ = Mock(return_value=False)
    stream.read.return_value = body if isinstance(body, bytes) else json.dumps(body).encode()
    opener = Mock()
    opener.open.return_value = stream
    monkeypatch.setattr(ollama_client, "build_opener", Mock(return_value=opener))
    return opener


def test_direct_endpoint_and_native_request(monkeypatch):
    reply = '{"titulo":"Una idea","animo":"calma","ritmo":"lento","paleta":"mar","motivo":"Texto generado"}'
    opener = response(monkeypatch, {"done": True, "message": {"content": reply}})
    payload = make_request("qwen2.5:1.5b-instruct", "Devolver JSON", "Una ciudad")
    assert validar(ollama_client.chat("http://127.0.0.1:11434/", payload)).titulo == "Una idea"
    request = opener.open.call_args.args[0]
    assert request.full_url == "http://127.0.0.1:11434/api/chat"
    assert request.get_method() == "POST"
    sent = json.loads(request.data)
    assert sent["messages"][1] == {"role": "user", "content": "Una ciudad"}
    assert sent["stream"] is False and sent["format"] == "json"
    assert sent["options"]["num_predict"] == 256
    assert "task" not in sent and "job" not in sent


@pytest.mark.parametrize("body", [b'not JSON', [], {"done": False},
    {"done": True, "message": {"content": 23}},
    {"done": True, "message": {"content": " "}},
    {"done": True, "message": {"content": "{}"}, "done_reason": "length"},
    b'x' * (1024 * 1024 + 1)])
def test_incomplete_or_invalid_response_rejected(monkeypatch, body):
    response(monkeypatch, body)
    with pytest.raises(ValueError): ollama_client.chat("http://localhost:11434", {})


def test_ollama_error(monkeypatch):
    response(monkeypatch, {"error": "model not found"})
    with pytest.raises(RuntimeError, match="model not found"):
        ollama_client.chat("http://localhost:11434", {})


@pytest.mark.parametrize("error,expected", [
    (URLError("connection refused"), "Abrir Ollama"),
    (HTTPError("http://localhost", 404, "missing", {}, BytesIO(b'{"error":"model missing"}')), "HTTP 404")])
def test_connection_and_http_errors(monkeypatch, error, expected):
    opener = Mock(); opener.open.side_effect = error
    monkeypatch.setattr(ollama_client, "build_opener", Mock(return_value=opener))
    with pytest.raises(RuntimeError, match=expected):
        ollama_client.chat("http://localhost:11434", {})


def test_rejects_invented_category():
    with pytest.raises(ValueError):
        validar('{"titulo":"Una idea","animo":"inventado","ritmo":"lento","paleta":"mar","motivo":""}')
