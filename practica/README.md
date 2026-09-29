# Práctica · LLM generativo desde Python

> **Opcional.** Lo central de la clase es openFrameworks (`llmPoster` y `llmStage`).
> Esta práctica repite el mismo recorrido en Python para quien quiera explorarlo,
> no hace falta para usar OF.

Python consulta **directamente a Ollama**, igual que lo hará OF. Completar la
[instalación](../README.md) y mantener Ollama abierto. Ejecutar desde la raíz
de la clase; no hace falta iniciar un servidor Python.

## Interfaz visual con Gradio

```sh
.venv/bin/python practica/interfaz.py
```

Abre **http://127.0.0.1:7860**. Dejar esa terminal abierta y mantener Ollama
iniciado. **Generar con Qwen** consulta el modelo; **Ver ejemplo grabado** muestra
una respuesta manual marcada SIN INFERENCIA. Si falla la consulta o el contrato,
se conserva el afiche anterior y se muestra el motivo.

La interfaz tiene **tres pestañas**, con los mismos recorridos que los ejemplos de OF:

| Pestaña | Equivale a | Qué hace |
| --- | --- | --- |
| **AFICHE** | `llmPoster`, modo AFICHE | Texto → 5 campos → afiche |
| **CHAT** | `llmPoster`, modo CHAT | Conversación con memoria configurable (4 por defecto) + afiche que sigue el tono |
| **ESCENARIO** | `llmStage` | Texto → 9 campos → escenario con física; el mouse atrae o repele |

Las paletas, formas y velocidades las programamos en `vista.py`, `estilo.css` y
`escenario.js`; el modelo elige categorías y números, y genera texto.
El campo «Estado» distingue la respuesta real de la grabada.

- **CHAT:** el modelo no recuerda nada. En cada mensaje le reenviamos los turnos
  anteriores (`modos.mensajes_chat()`). **Nuevo chat** borra esa lista. Probar:
  «Me llamo …» → «¿Cómo me llamo?» → Nuevo chat → repetir la pregunta.
  «Escribiendo…» es un efecto local: la respuesta llega completa por HTTP.
  **Contexto:** el panel «Contexto» cambia la **memoria** (intercambios reenviados,
  0 = sin memoria) y **num_ctx** (ventana de Ollama en tokens). Los valores
  iniciales salen de `llmPoster/bin/data/chat-config.json`, el mismo archivo que usa
  OF. **Borrar contexto** vacía el historial. El estado muestra cuántos tokens ocupó
  el último pedido.
- **ESCENARIO:** usa el prompt y el ejemplo grabado de `llmStage/bin/data`, y envía
  un JSON Schema en `format`. Incluye la paleta `fuego` (rojo, enojo). La física es
  un motor mínimo en `escenario.js`, sin bibliotecas externas: funciona sin
  internet. Es una simplificación de Box2D (los cuerpos no giran).

Gradio está incluido en el instalador. Si tu entorno es anterior, ejecutar una
vez `.venv/bin/python -m pip install -r requirements.txt`. Se puede cambiar
el puerto con `--port 7861` y evitar abrir automáticamente el navegador con
`--no-browser`. Ctrl+C detiene la interfaz. Solo escucha en este equipo;
no publica un enlace externo.

## Versión de consola

```sh
# 1. Entender el formato, sin modelo
.venv/bin/python practica/clasificar_textos.py --recorded

# 2. Generar una respuesta real
.venv/bin/python practica/clasificar_textos.py --text "El reloj acelera mis pasos"

# 3. Comparar seis entradas
.venv/bin/python practica/clasificar_textos.py --dataset
```

En Windows: `.venv\Scripts\python.exe` en lugar de `.venv/bin/python`.
El primer comando muestra SIN INFERENCIA. Los otros consultan Qwen y muestran
el texto de entrada, la propuesta humana si la hay y la respuesta validada.

## Qué leer

| Archivo | Función |
| --- | --- |
| [interfaz.py](interfaz.py) | Componentes de Gradio y evento del botón Generar |
| [vista.py](vista.py) / [estilo.css](estilo.css) | Convertir datos validados en composición, color y movimiento |
| [modos.py](modos.py) | Pedidos y validación de CHAT (con memoria) y ESCENARIO |
| [escenario.js](escenario.js) | Física y dibujo del escenario en el navegador (equivale a `StagePhysics.h`) |
| [clasificar_textos.py](clasificar_textos.py) | Construye los mensajes y usa la respuesta del LLM |
| [ollama_client.py](ollama_client.py) | Envía HTTP y comprueba errores de transporte/respuesta |
| [contrato.py](contrato.py) | Contratos `Afiche`, `Chat` y `Escena` (los mismos que `PosterSpec.h`, `ChatSession.h` y `SceneSpec.h`) |
| [textos.csv](textos.csv) | Seis textos inventados y propuestas humanas para debatir |
| [system-prompt.txt](../llmPoster/bin/data/system-prompt.txt) | Instrucción compartida con el ejemplo OF |

El nombre `clasificar_textos.py` describe el ejercicio: pedimos a un LLM
**generativo** que proponga categorías y escriba un título y un motivo. No se
entrena un clasificador ni se modifican los pesos. El CSV es una entrada para
comparar interpretaciones, no un dataset de entrenamiento ni un benchmark.

En `make_request()`, observar `model`, los roles `system/user`, `content`,
`format: "json"` y `stream: false`. En este ejemplo esperamos la respuesta
completa. La salida está en `message.content`; después se valida con Pydantic.

La configuración se lee de `llmPoster/bin/data/connection.json`. Se puede
sobrescribir con `--url http://127.0.0.1:11434` y `--model NOMBRE_EN_OLLAMA`.

**Ejercicio:** cambiar un texto y comparar el resultado. Después, agregar una
categoría: modificar prompt y `contrato.py`; en OF actualizar también
`PosterSpec.h` y `PosterView.h`. Un JSON válido no prueba que la interpretación
sea correcta ni que esté libre de sesgos.

## Consigna visual para probar

1. Probar el ejemplo grabado y cambiar una paleta en `estilo.css`.
2. Reiniciar la interfaz y comprobar qué cambió sin usar el modelo.
3. Generar dos respuestas y comparar el JSON con la representación.
4. Modificar `crear_interfaz()` para construir otra disposición o agregar un control.
5. Trasladar esa decisión a `PosterView.h` en OF: cambia la biblioteca visual,
   se mantiene el contrato de datos.
6. En ESCENARIO, cambiar `RADIO_MOUSE` o `FUERZA_MOUSE` en `escenario.js`, o agregar
   una paleta en `PALETAS` (y en `contrato.py` y el prompt de `llmStage`).

Ver también [docs/GUIA_ALUMNOS_ESCENARIOS.md](../docs/GUIA_ALUMNOS_ESCENARIOS.md).

Referencia: [Gradio](https://www.gradio.app/docs/gradio/blocks).
