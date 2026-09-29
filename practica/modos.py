"""CHAT y ESCENARIO: la misma lógica que llmPoster (modo chat) y llmStage, en Python.

No hay interfaz acá: solo se arma el pedido a Ollama y se valida la respuesta.
interfaz.py decide cómo mostrarlo. Así se puede probar sin abrir el navegador.
"""
import json
from pathlib import Path

from contrato import Chat, Escena, esquema_escena, validar_chat, validar_escena
from ollama_client import chat, chat_detalle

RAIZ = Path(__file__).resolve().parents[1]
POSTER = RAIZ / "llmPoster/bin/data"   # chat-prompt.txt, connection.json
STAGE = RAIZ / "llmStage/bin/data"     # system-prompt.txt y recorded.json del escenario
MEMORIA_MAX, NUM_CTX_MIN, NUM_CTX_MAX = 20, 512, 32768   # mismos límites que ofApp::loadChatConfig()


def config_contexto():
    """Valores iniciales de CONTEXTO: el mismo chat-config.json que usa llmPoster en OF.

    memoria = intercambios que se reenvían (0 = sin memoria).
    num_ctx = ventana de Ollama en tokens (instrucciones + historial + mensaje + respuesta).
    """
    archivo = POSTER / "chat-config.json"
    datos = json.loads(archivo.read_text(encoding="utf-8")) if archivo.exists() else {}
    memoria = min(max(int(datos.get("memoria", 4)), 0), MEMORIA_MAX)
    num_ctx = min(max(int(datos.get("num_ctx", 4096)), NUM_CTX_MIN), NUM_CTX_MAX)
    return memoria, num_ctx


MEMORIA, NUM_CTX = config_contexto()


def _config():
    return json.loads((POSTER / "connection.json").read_text(encoding="utf-8"))


# ----------------------------------------------------------------------------
# CHAT: el modelo no recuerda nada. La "memoria" es reenviar la conversación.
# ----------------------------------------------------------------------------
def mensajes_chat(system, turnos, texto):
    # [system] instrucciones + [user/assistant] x N turnos + [user] mensaje nuevo.
    # Como assistant reenviamos el JSON completo que devolvió el modelo.
    mensajes = [{"role": "system", "content": system}]
    for turno in turnos:
        mensajes.append({"role": "user", "content": turno["user"]})
        mensajes.append({"role": "assistant", "content": turno["json"]})
    mensajes.append({"role": "user", "content": texto})
    return mensajes


def recortar(turnos, memoria):
    """Conservar solo los últimos `memoria` intercambios (0 = ninguno)."""
    return turnos[-memoria:] if memoria > 0 else []


def conversar(texto, turnos, memoria=None, num_ctx=None):
    """Devuelve (turnos_nuevos, respuesta_validada, json_crudo, tokens_del_pedido).

    Lanza error si la respuesta no cumple el contrato (y entonces no se guarda el turno).
    """
    memoria = MEMORIA if memoria is None else memoria
    num_ctx = NUM_CTX if num_ctx is None else num_ctx
    turnos = recortar(turnos, memoria)
    config = _config()
    system = (POSTER / "chat-prompt.txt").read_text(encoding="utf-8")
    pedido = {"model": config["model"], "stream": False, "format": "json",
              # temperature .4: algo de variedad para conversar (el afiche usa 0).
              # num_ctx: si el pedido no entra, Ollama descarta lo más viejo sin avisar.
              "options": {"temperature": 0.4, "num_predict": 768, "num_ctx": num_ctx},
              "messages": mensajes_chat(system, turnos, texto)}
    crudo, cuerpo = chat_detalle(config["ollama_url"], pedido)
    respuesta: Chat = validar_chat(crudo)  # Si no cumple, error: no se guarda el turno.
    nuevos = recortar(turnos + [{"user": texto, "answer": respuesta.respuesta, "json": crudo}], memoria)
    return nuevos, respuesta, crudo, cuerpo.get("prompt_eval_count", 0)


# ----------------------------------------------------------------------------
# ESCENARIO: sin historial. Cada pedido crea una escena nueva.
# ----------------------------------------------------------------------------
def escena_grabada() -> Escena:
    return validar_escena((STAGE / "recorded.json").read_text(encoding="utf-8"))


def crear_escena(texto):
    """Devuelve (escena_validada, json_crudo). Lanza error si no cumple."""
    config = _config()
    system = (STAGE / "system-prompt.txt").read_text(encoding="utf-8")
    pedido = {"model": config["model"], "stream": False,
              # format con JSON Schema: guía al modelo; validar_escena() igual controla.
              "format": esquema_escena(),
              "options": {"temperature": 0, "num_predict": 768},
              "messages": [{"role": "system", "content": system},
                           {"role": "user", "content": texto}]}
    crudo = chat(config["ollama_url"], pedido)
    return validar_escena(crudo), crudo
