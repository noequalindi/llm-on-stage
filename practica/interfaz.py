"""Visual Python example. Run from the root: .venv/bin/python practica/interfaz.py

Tres pestañas, los mismos recorridos que los ejemplos de OF:
    AFICHE     texto -> 5 campos -> afiche          (llmPoster, modo AFICHE)
    CHAT       conversación con memoria + afiche   (llmPoster, modo CHAT)
    ESCENARIO  texto -> 9 campos -> física          (llmStage)
Cambia la biblioteca visual (Gradio + HTML/CSS/JS en vez de C++); se mantienen
el pedido HTTP a Ollama y los contratos de datos.
"""
import argparse
import csv
from html import escape
import json
from pathlib import Path
import time

import gradio as gr

from clasificar_textos import DATA, make_request
from contrato import validar
import modos
from ollama_client import chat
from vista import dibujar, dibujar_escena


FOLDER = Path(__file__).resolve().parent
ERRORES = (OSError, ValueError, RuntimeError, KeyError)


# ============================================================================
# AFICHE: texto -> 5 campos validados -> afiche (igual que el modo AFICHE de OF)
# ============================================================================
def ejemplo_grabado():
    afiche = validar((DATA / "recorded.json").read_text(encoding="utf-8"))
    return afiche.model_dump(), dibujar(afiche, grabada=True), "Sin inferencia. Escribí una idea y pulsá Generar con Qwen."


def generar(texto):
    # Same request and validator as the console practice; only the interface changes.
    if not texto or not texto.strip():
        return gr.skip(), gr.skip(), "Escribí una idea antes de generar. Se conserva el afiche anterior."
    if len(texto) > 2000:
        return gr.skip(), gr.skip(), "Usá hasta 2000 caracteres para esta práctica. Se conserva el afiche anterior."
    try:
        config = json.loads((DATA / "connection.json").read_text(encoding="utf-8"))
        system = (DATA / "system-prompt.txt").read_text(encoding="utf-8")
        raw = chat(config["ollama_url"], make_request(config["model"], system, texto))
        afiche = validar(raw)
        return afiche.model_dump(), dibujar(afiche), "Respuesta real validada. El modelo propone; nuestro código decide cómo dibujar."
    except ERRORES as error:
        return gr.skip(), gr.skip(), f"No se actualizó el afiche: {error}"


# ============================================================================
# CHAT: la memoria es la lista de turnos (gr.State) que reenviamos cada vez.
# ============================================================================
def burbujas(turnos):
    """Turnos guardados -> mensajes que muestra gr.Chatbot."""
    mensajes = []
    for turno in turnos:
        mensajes += [{"role": "user", "content": turno["user"]}, {"role": "assistant", "content": turno["answer"]}]
    return mensajes


def nuevo_chat():
    # BORRAR CONTEXTO: tirar el "cuaderno". El modelo no cambia; solo lo que le reenviamos.
    datos, afiche, _ = ejemplo_grabado()
    return [], [], afiche, datos, "Contexto borrado: el próximo mensaje llega sin historial."


def cambiar_memoria(memoria, turnos):
    # Bajar la memoria olvida los intercambios más viejos en el momento.
    recortados = modos.recortar(turnos, int(memoria))
    return burbujas(recortados), recortados, f"Memoria: {int(memoria)} intercambios (0 = sin memoria)."


def estado_contexto(turnos, memoria, tokens, num_ctx):
    texto = f"Memoria: {len(turnos)} de {int(memoria)} intercambios · el último pedido ocupó {tokens} de {int(num_ctx)} tokens."
    if tokens > int(num_ctx) * 0.9:
        texto += " ⚠️ Contexto casi lleno: borrá el contexto o subí num_ctx."
    return texto


def enviar_chat(texto, turnos, memoria=modos.MEMORIA, num_ctx=modos.NUM_CTX):
    # Generador: cada yield actualiza la pantalla. Salidas, en orden:
    # chatbot, memoria, afiche, JSON, estado, caja de texto.
    if not texto or not texto.strip():
        yield gr.skip(), gr.skip(), gr.skip(), gr.skip(), "Escribí un mensaje primero.", gr.skip()
        return
    if len(texto) > 950:
        yield gr.skip(), gr.skip(), gr.skip(), gr.skip(), "Usá hasta 950 caracteres por mensaje.", gr.skip()
        return
    anteriores = burbujas(turnos)
    yield (anteriores + [{"role": "user", "content": texto}, {"role": "assistant", "content": "Pensando…"}],
           gr.skip(), gr.skip(), gr.skip(), "Consultando a Qwen… (HTTP a Ollama, sin streaming)", "")
    try:
        nuevos, respuesta, crudo, tokens = modos.conversar(texto, turnos, int(memoria), int(num_ctx))
    except ERRORES as error:
        # Rechazada: no se guarda el turno, el afiche no cambia y recuperamos el texto escrito.
        yield anteriores, gr.skip(), gr.skip(), gr.skip(), f"Respuesta rechazada, se conserva la conversación: {error}", texto
        return
    afiche, datos = dibujar(respuesta.visual), json.loads(crudo)
    # "Escribiendo…": la respuesta ya llegó completa y ya está en la memoria.
    # Solo la mostramos de a poco (40 caracteres por segundo, como ofxTextReveal).
    base = burbujas(nuevos[:-1] if nuevos else []) + [{"role": "user", "content": texto}]
    completo = respuesta.respuesta
    for fin in range(0, len(completo), 2):
        yield base + [{"role": "assistant", "content": completo[:fin] + "▍"}], nuevos, afiche, datos, "Escribiendo…", gr.skip()
        time.sleep(0.05)
    yield (base + [{"role": "assistant", "content": completo}], nuevos, afiche, datos,
           estado_contexto(nuevos, memoria, tokens, num_ctx), gr.skip())


# ============================================================================
# ESCENARIO: texto -> 9 campos validados -> física en el navegador (escenario.js)
# ============================================================================
def escena_grabada_ui():
    escena = modos.escena_grabada()
    return dibujar_escena(escena, grabada=True), escena.model_dump(), escena.respuesta, "Escena grabada, sin inferencia."


def crear_escena_ui(texto):
    if not texto or not texto.strip():
        return gr.skip(), gr.skip(), gr.skip(), "Escribí una escena primero."
    if len(texto) > 950:
        return gr.skip(), gr.skip(), gr.skip(), "Usá hasta 950 caracteres."
    try:
        escena, _ = modos.crear_escena(texto)
    except ERRORES as error:
        return gr.skip(), gr.skip(), gr.skip(), f"Respuesta rechazada, se conserva la escena: {error}"
    return (dibujar_escena(escena), escena.model_dump(), escena.respuesta,
            "Escena generada. Mantené el mouse presionado dentro del escenario.")


# ============================================================================
# INTERFAZ: componentes y eventos. Esta es la parte que cada obra reemplaza.
# ============================================================================
def crear_interfaz():
    datos, dibujo, estado = ejemplo_grabado()
    escena_html, escena_datos, escena_texto, escena_estado = escena_grabada_ui()
    config = json.loads((DATA / "connection.json").read_text(encoding="utf-8"))
    with (FOLDER / "textos.csv").open(encoding="utf-8", newline="") as stream:
        ejemplos = [[row["texto"]] for row in csv.DictReader(stream)]
    with gr.Blocks(title="UNA · Laboratorio LLM", analytics_enabled=False) as app:
        gr.HTML('''<header class="cabecera"><div class="marca">UNA / INFORMÁTICA APLICADA 2</div>
          <div class="edicion">26 · LLM LOCAL</div><h1>Del lenguaje a la imagen.</h1>
          <p>Escribí una idea. Observá lo que genera el modelo. Decidí cómo representarlo.</p></header>''')
        with gr.Tabs():
            # ---------------------------------------------------------------- AFICHE
            with gr.Tab("AFICHE"):
                with gr.Row(equal_height=False):
                    with gr.Column(scale=5, min_width=300):
                        gr.Markdown("### 01 / Tu idea")
                        texto = gr.Textbox(label="Texto para interpretar", lines=4, max_lines=6,
                                           value="La ciudad respira despacio mientras vuelve el sol.")
                        with gr.Row():
                            enviar = gr.Button("Generar con Qwen", variant="primary")
                            grabada = gr.Button("Ver ejemplo grabado", variant="secondary")
                        estado_ui = gr.Textbox(value=estado, label="Estado", interactive=False, lines=3)
                        with gr.Accordion("Probar otras ideas", open=False):
                            gr.Examples(examples=ejemplos, inputs=texto, label="Textos para comparar")
                        gr.Markdown("### 02 / La respuesta del modelo")
                        salida = gr.JSON(value=datos, label="JSON validado", open=True)
                        gr.HTML(f'<p class="nota">Modelo local: <strong>{escape(config["model"])}</strong><br>'
                                'El JSON propone título, ánimo, ritmo, paleta y motivo. No entrena con estos textos.</p>')
                    with gr.Column(scale=6, min_width=300):
                        gr.Markdown("### 03 / Nuestra representación")
                        afiche_ui = gr.HTML(value=dibujo)
                        gr.Markdown("El LLM genera texto; **la composición y el movimiento están programados**. "
                                    "Después hacemos este mismo recorrido en openFrameworks.")

            # ---------------------------------------------------------------- CHAT
            with gr.Tab("CHAT"):
                memoria = gr.State([])  # Lista de turnos: vive en esta pestaña del navegador, no en el modelo.
                with gr.Row(equal_height=False):
                    with gr.Column(scale=5, min_width=300):
                        gr.Markdown("### Conversación")
                        conversacion = gr.Chatbot(label="Conversación", height=380)
                        mensaje = gr.Textbox(label="Tu mensaje", lines=2, max_lines=4,
                                             value="¡Hola! ¿Me ayudás a imaginar una experiencia interactiva?")
                        with gr.Row():
                            enviar_msg = gr.Button("Enviar", variant="primary")
                            reiniciar = gr.Button("Borrar contexto", variant="secondary")
                        # CONTEXTO parametrizable. Valores iniciales: llmPoster/bin/data/chat-config.json.
                        with gr.Accordion("Contexto: memoria y num_ctx", open=True):
                            memoria_ui = gr.Slider(0, modos.MEMORIA_MAX, value=modos.MEMORIA, step=1,
                                                   label="Memoria: intercambios que se reenvían (0 = sin memoria)")
                            num_ctx_ui = gr.Dropdown(sorted({2048, 4096, 8192, 16384, 32768, modos.NUM_CTX}), value=modos.NUM_CTX,
                                                     label="num_ctx: ventana de contexto de Ollama (tokens)",
                                                     allow_custom_value=False)
                        estado_chat = gr.Textbox(label="Estado", interactive=False, lines=2,
                                                 value="Probá: «Me llamo …» → «¿Cómo me llamo?» → Nuevo chat → repetí la pregunta.")
                    with gr.Column(scale=6, min_width=300):
                        gr.Markdown("### El tono de la respuesta, como afiche")
                        afiche_chat = gr.HTML(value=dibujo)
                        with gr.Accordion("JSON de la última respuesta", open=False):
                            json_chat = gr.JSON(value=datos, label="respuesta + visual")

            # ---------------------------------------------------------------- ESCENARIO
            with gr.Tab("ESCENARIO"):
                with gr.Row(equal_height=False):
                    with gr.Column(scale=4, min_width=300):
                        gr.Markdown("### Tu escena")
                        pedido = gr.Textbox(label="Describí una escena", lines=3, max_lines=5,
                                            value="Un escenario lunar con palabras flotando y el mouse como imán")
                        with gr.Row():
                            crear = gr.Button("Crear escena", variant="primary")
                            grabada_escena = gr.Button("Ejemplo grabado", variant="secondary")
                        estado_escena = gr.Textbox(value=escena_estado, label="Estado", interactive=False, lines=2)
                        respuesta_escena = gr.Textbox(value=escena_texto, label="Respuesta del modelo", interactive=False, lines=4)
                        with gr.Accordion("Probar otras escenas", open=False):
                            gr.Examples(examples=[["Un escenario rojo de enojo: cajas furiosas que caen fuerte y huyen del mouse"],
                                                  ["Palabras tranquilas como piedras azules, con poco rebote y caída lenta"],
                                                  ["Una lluvia de cajas cálidas que rebotan mucho"]],
                                        inputs=pedido, label="Ideas")
                        with gr.Accordion("JSON validado (9 campos)", open=False):
                            json_escena = gr.JSON(value=escena_datos, label="escena")
                    with gr.Column(scale=6, min_width=300):
                        gr.Markdown("### Nuestra física · escenario.js")
                        escena_ui = gr.HTML(value=escena_html)
                        gr.Markdown("El LLM elige **números y palabras**; la física, los choques y los colores "
                                    "están programados en `escenario.js`. Es el mismo contrato que `llmStage` en OF.")

        with gr.Accordion("Para modificar el ejemplo", open=False):
            gr.Markdown("**interfaz.py:** componentes y eventos · **modos.py:** pedidos de CHAT y ESCENARIO · "
                        "**vista.py / estilo.css / escenario.js:** representación · "
                        "**contrato.py:** campos permitidos · **bin/data/*.txt:** instrucciones al modelo.\n\n"
                        "Primero cambiá una paleta manteniendo el JSON; después probá otra instrucción. "
                        "Un JSON válido no garantiza una interpretación correcta.")

        # EVENTOS. concurrency_id="llm": una sola consulta a Ollama por vez, entre todas las pestañas.
        for boton, funcion, entradas, nombre in ((enviar, generar, texto, "generar"),
                                                  (grabada, ejemplo_grabado, None, "grabado")):
            boton.click(funcion, inputs=entradas, outputs=[salida, afiche_ui, estado_ui],
                        api_name=nombre, concurrency_id="llm", concurrency_limit=1)
        salidas_chat = [conversacion, memoria, afiche_chat, json_chat, estado_chat, mensaje]
        for disparador in (enviar_msg.click, mensaje.submit):
            disparador(enviar_chat, inputs=[mensaje, memoria, memoria_ui, num_ctx_ui], outputs=salidas_chat,
                       api_name=False, concurrency_id="llm", concurrency_limit=1)
        reiniciar.click(nuevo_chat, outputs=salidas_chat[:5], api_name=False)
        memoria_ui.change(cambiar_memoria, inputs=[memoria_ui, memoria],
                          outputs=[conversacion, memoria, estado_chat], api_name=False)
        salidas_escena = [escena_ui, json_escena, respuesta_escena, estado_escena]
        crear.click(crear_escena_ui, inputs=pedido, outputs=salidas_escena,
                    api_name="escena", concurrency_id="llm", concurrency_limit=1)
        grabada_escena.click(escena_grabada_ui, outputs=salidas_escena, api_name=False)
    return app.queue(default_concurrency_limit=1, max_size=8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=7860)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("Elegir un puerto entre 1024 y 65535")
    app = crear_interfaz()
    # escenario.js va inline en <head>: funciona sin internet (no usa CDN).
    fisica = (FOLDER / "escenario.js").read_text(encoding="utf-8")
    app.launch(server_name="127.0.0.1", server_port=args.port, share=False,
               inbrowser=not args.no_browser, show_error=False,
               css=(FOLDER / "estilo.css").read_text(encoding="utf-8"),
               head=f"<script>{fisica}</script>",
               theme=gr.themes.Base(primary_hue="blue", font=["Arial", "sans-serif"]))


if __name__ == "__main__":
    main()
