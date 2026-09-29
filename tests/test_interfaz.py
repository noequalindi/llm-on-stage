"""Validate the boundary between model output and the visual application."""
import json
from pathlib import Path
import sys
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "practica"))
import interfaz
from contrato import Afiche
from vista import dibujar


def test_grabado_does_not_call_model(monkeypatch):
    chat = Mock(side_effect=AssertionError("no inference expected"))
    monkeypatch.setattr(interfaz, "chat", chat)
    data, html, status = interfaz.ejemplo_grabado()
    assert data["titulo"] and "SIN INFERENCIA" in html
    chat.assert_not_called()


def test_valid_reply_updates_data_and_view(monkeypatch):
    data = interfaz.ejemplo_grabado()[0]
    data["titulo"] = "Una respuesta nueva"
    chat = Mock(return_value=json.dumps(data))
    monkeypatch.setattr(interfaz, "chat", chat)
    result, html, status = interfaz.generar("Una idea nueva")
    assert result == data and "Una respuesta nueva" in html
    assert "SIN INFERENCIA" not in html and "Respuesta real" in status
    assert chat.call_args.args[1]["messages"][1]["content"] == "Una idea nueva"


def test_error_preserves_previous_outputs(monkeypatch):
    monkeypatch.setattr(interfaz, "chat", Mock(return_value='{"titulo":"incompleto"}'))
    data, html, status = interfaz.generar("Una idea")
    assert data == html == {"__type__": "update"}
    assert "No se actualizó" in status


def test_empty_input_does_not_consult_model(monkeypatch):
    chat = Mock(side_effect=AssertionError("no inference expected"))
    monkeypatch.setattr(interfaz, "chat", chat)
    assert "Escribí" in interfaz.generar("  ")[2]
    chat.assert_not_called()


def test_model_text_cannot_inject_html():
    afiche = Afiche(titulo='<img src=x onerror=alert(1)>', animo="calma", ritmo="lento",
                    paleta="mar", motivo="<script>alert(1)</script>")
    html = dibujar(afiche)
    assert '<img' not in html and '<script>' not in html
    assert '&lt;img' in html and '&lt;script&gt;' in html
