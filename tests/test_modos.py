"""CHAT and ESCENARIO in the Gradio practice: same contracts as OF, model mocked."""
import json
from pathlib import Path
import sys
from unittest.mock import Mock

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "practica"))
import interfaz
import modos
from contrato import esquema_escena, validar_escena
from vista import dibujar_escena

ROOT = Path(__file__).resolve().parents[1]
AFICHE = json.loads((ROOT / "llmPoster/bin/data/recorded.json").read_text(encoding="utf-8"))
ESCENA = json.loads((ROOT / "llmStage/bin/data/recorded.json").read_text(encoding="utf-8"))


def respuesta_chat(texto):
    return json.dumps({"respuesta": texto, "visual": AFICHE}, ensure_ascii=False)


def ollama(*textos, tokens=123):
    return Mock(side_effect=[(t, {"prompt_eval_count": tokens}) for t in textos])


def test_chat_memory_resends_history_and_keeps_four_turns(monkeypatch):
    chat = ollama(*[respuesta_chat(f"R{i}") for i in range(6)])
    monkeypatch.setattr(modos, "chat_detalle", chat)
    turnos = []
    for i in range(6):
        turnos, respuesta, _, tokens = modos.conversar(f"U{i}", turnos, memoria=4, num_ctx=8192)
    assert len(turnos) == 4 and turnos[0]["user"] == "U2" and respuesta.respuesta == "R5" and tokens == 123
    pedido = chat.call_args.args[1]
    enviados = pedido["messages"]
    assert pedido["options"]["num_ctx"] == 8192
    assert enviados[0]["role"] == "system" and enviados[-1] == {"role": "user", "content": "U5"}
    assert [m["content"] for m in enviados if m["role"] == "user"] == ["U1", "U2", "U3", "U4", "U5"]


def test_memory_zero_sends_only_system_and_new_message(monkeypatch):
    chat = ollama(respuesta_chat("R"))
    monkeypatch.setattr(modos, "chat_detalle", chat)
    previos = [{"user": "viejo", "answer": "a", "json": "{}"}]
    turnos, *_ = modos.conversar("nuevo", previos, memoria=0)
    assert turnos == [] and len(chat.call_args.args[1]["messages"]) == 2


def test_context_config_is_shared_with_of():
    datos = json.loads((ROOT / "llmPoster/bin/data/chat-config.json").read_text(encoding="utf-8"))
    assert (modos.MEMORIA, modos.NUM_CTX) == (datos["memoria"], datos["num_ctx"])


def test_lowering_memory_forgets_oldest():
    turnos = [{"user": str(i), "answer": "", "json": "{}"} for i in range(4)]
    burbujas, recortados, estado = interfaz.cambiar_memoria(2, turnos)
    assert [t["user"] for t in recortados] == ["2", "3"] and len(burbujas) == 4


def test_chat_rejected_reply_does_not_store_turn(monkeypatch):
    monkeypatch.setattr(modos, "chat_detalle", Mock(return_value=('{"respuesta":"sin visual"}', {})))
    pasos = list(interfaz.enviar_chat("Hola", []))
    burbujas, memoria, afiche, _, estado, caja = pasos[-1]
    assert burbujas == [] and memoria == afiche == {"__type__": "update"}
    assert "rechazada" in estado and caja == "Hola"


def test_chat_ui_reveals_then_shows_full_answer(monkeypatch):
    monkeypatch.setattr(modos, "chat_detalle", ollama(respuesta_chat("Hola, ¿cómo estás?"), tokens=3900))
    monkeypatch.setattr(interfaz.time, "sleep", lambda _: None)
    pasos = list(interfaz.enviar_chat("Hola", [], 4, 4096))
    assert pasos[0][0][-1]["content"] == "Pensando…"
    final = pasos[-1]
    assert final[0][-1]["content"] == "Hola, ¿cómo estás?" and len(final[1]) == 1
    assert "3900 de 4096 tokens" in final[4] and "casi lleno" in final[4]


def test_scene_request_uses_schema_and_accepts_fuego(monkeypatch):
    escena = dict(ESCENA, paleta="fuego", palabras=["furia", "¡no!"])
    chat = Mock(return_value=json.dumps(escena, ensure_ascii=False))
    monkeypatch.setattr(modos, "chat", chat)
    resultado, _ = modos.crear_escena("Un escenario rojo de enojo")
    assert resultado.paleta == "fuego"
    assert chat.call_args.args[1]["format"] == esquema_escena()
    assert "fuego" in esquema_escena()["properties"]["paleta"]["enum"]


@pytest.mark.parametrize("campo,valor", [("paleta", "lava"), ("cantidad", 25), ("cantidad", 12.0),
                                         ("gravedad", "3"), ("palabras", ["01234567890"]), ("extra", 1)])
def test_scene_contract_rejects_like_scenespec(campo, valor):
    with pytest.raises(ValueError):
        validar_escena(json.dumps(dict(ESCENA, **{campo: valor})))


def test_scene_rejected_keeps_previous(monkeypatch):
    monkeypatch.setattr(modos, "chat", Mock(return_value='{"titulo":"incompleto"}'))
    escena, datos, texto, estado = interfaz.crear_escena_ui("Algo")
    assert escena == datos == texto == {"__type__": "update"} and "rechazada" in estado


def test_scene_model_text_cannot_break_html():
    escena = validar_escena(json.dumps(dict(ESCENA, titulo='"><script>alert(1)</script>')))
    html = dibujar_escena(escena)
    assert "<script>" not in html and "&lt;script&gt;" in html
