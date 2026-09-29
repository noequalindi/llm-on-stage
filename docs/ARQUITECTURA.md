# Arquitectura · un LLM, dos clientes

```text
Python: practica/interfaz.py (Gradio) o clasificar_textos.py (consola)
    → HTTP/JSON → Ollama → Qwen → respuesta generada

OF: llmPoster + ofxLocalLLM
    → HTTP/JSON → Ollama → Qwen → respuesta → afiche interactivo
```

Python y OF son clientes independientes. **OF funciona sin ejecutar la práctica
Python.** Ollama es el servicio local que carga los pesos y ejecuta el modelo.
Esta clase no incluye Model Video Explorer ni una API intermedia propia.

## Responsabilidades

| Componente | Qué hace |
| --- | --- |
| Qwen | Genera texto a partir de los mensajes |
| Ollama | Administra los pesos y ejecuta inferencia; recibe HTTP en 11434 |
| Gradio / `practica/interfaz.py` | Interfaz en el navegador: pestañas AFICHE, CHAT y ESCENARIO |
| `practica/modos.py` | Pedidos de CHAT (memoria, `num_ctx`, tokens) y ESCENARIO (JSON Schema) |
| `practica/escenario.js` | Física y dibujo del escenario en el navegador (equivale a `StagePhysics.h`) |
| `practica/vista.py` y `estilo.css` | Representación programada de la respuesta validada |
| `practica/ollama_client.py` | Conexión HTTP desde Python |
| `practica/contrato.py` | Contratos `Afiche`, `Chat` y `Escena` en Python |
| `ofxLocalLLM` | Conexión HTTP en un hilo; entrega respuestas a OF |
| `llmPoster/src/ChatSession.h` | Valida el chat y conserva los últimos `memoria` intercambios (4 por defecto) |
| `llmPoster/bin/data/chat-config.json` | Contexto del chat: `memoria` y `num_ctx`; compartido con Python |
| `src/TextStyle.h` (llmPoster y llmStage) | Fuente TrueType con ñ y tildes; si falta, vuelve a la bitmap |
| `llmPoster/src/PosterSpec.h` | Valida el contrato en C++ |
| `llmPoster/src/PosterView.h` | Convierte los datos en gráficos y movimiento |

## Contrato de entrada y salida

Ambos clientes hacen `POST /api/chat`, con:

```json
{
  "model": "qwen2.5:1.5b-instruct",
  "messages": [
    {"role": "system", "content": "Instrucción que pide el JSON del afiche"},
    {"role": "user", "content": "La ciudad respira despacio"}
  ],
  "format": "json",
  "stream": false,
  "options": {"temperature": 0, "num_predict": 256}
}
```

La respuesta HTTP contiene `message.content`: un string con el texto generado.
El afiche interpreta ese texto como JSON y valida `titulo`, `animo`, `ritmo`,
`paleta` y `motivo`. Pedir JSON no sustituye validar las categorías y límites.
Si falla la consulta o la validación, OF conserva la representación anterior.

La práctica reutiliza los archivos de `llmPoster/bin/data/`. OF lee los de su
copia instalada. `connection.json` indica URL y nombre real del modelo; no hay
alias propios, IDs de trabajos ni polling. L consulta `GET /api/tags` para
comprobar conexión y disponibilidad del modelo; no inicia otro servicio.

## Modo chat en OF

El modo **AFICHE** y la practica Python mantienen el contrato anterior. El modo
**CHAT** usa `chat-prompt.txt` y devuelve un objeto con `respuesta` (texto libre
breve, validado hasta 700 caracteres) y `visual` (los cinco campos del afiche).
`ChatSession::accept()` valida ambos antes de modificar conversacion y escena.

Cada pedido incluye system + hasta cuatro pares user/assistant + el nuevo mensaje.
La aplicacion reenvia ese historial: no entrena ni modifica los pesos. Se guarda
solo en RAM, se descartan pares antiguos completos y Nuevo chat lo borra.
El chat solicita hasta 768 tokens y usa temperatura 0.4; el afiche conserva 256
y temperatura 0. Las respuestas llegan completas, no por streaming.

Calma, alegria y tension seleccionan movimientos programados. El modelo interpreta
el tono del texto; no reconoce emociones reales ni genera video. OF puede seguir
dibujando y recibir escritura mientras espera; cambiar de modo o borrar historial
requiere que termine la solicitud. Una respuesta invalida conserva el estado previo.

## Instalación y ejecución

`install_env_mac.command`, `install_env_linux.sh` e `install_env_windows.cmd`
preparan Python y Ollama. La implementación compartida está en `scripts/`.
`setup_local.py` instala Pydantic y Gradio, descarga/verifica Qwen y lo precarga. Ollama
elige su distribución CPU/GPU; `setup_local.py --check-model` muestra el estado.
Python no carga los pesos de Qwen en esta práctica.

OF mantiene el dibujo mientras un worker espera HTTP. El cliente espera la
respuesta completa (`stream: false`). Streaming de tokens puede ser una
extensión posterior; no se requiere para este recorrido inicial.

`.venv` guarda dependencias, `.tools` herramientas y `.local` logs/comprobaciones.
Los pesos los administra Ollama. Esos directorios no se publican. El único archivo
de conexión de la clase es `llmPoster/bin/data/connection.json`.

[API de Ollama](https://docs.ollama.com/api/chat) · [README](../README.md)

## Interfaz visual Python

Gradio inicia automáticamente una interfaz web en `127.0.0.1:7860`. Utiliza
la misma petición y validación que la consola. Es un cliente local de Ollama;
OF continúa conectando directamente a 11434 y no depende de Gradio. Las
dependencias web que incorpora Gradio implementan esa interfaz, no MVE.

El modelo devuelve texto, nunca HTML ejecutable. Se valida el contrato y se
escapan título y motivo antes de insertarlos en la composición. La cola visual
ejecuta una acción por vez y conserva la salida anterior ante errores. El
ejemplo grabado no consulta Ollama.

## El modelo y el contexto

| | DistilBERT (clase de datos) | **Qwen2.5 1.5B Instruct** (esta clase) |
| --- | --- | --- |
| Tipo | Encoder: lee y clasifica | Decoder: genera texto palabra por palabra |
| Parámetros (pesos) | 66 millones | **1.543 millones** |
| Vocabulario (tokens posibles) | 30.522 | **151.936** |
| Contexto máximo | 512 tokens | **32.768 tokens** |
| Dimensiones por token | 768 | 1.536 |
| Capas | 6 | 28 |
| Tamaño en disco | 268 MB | ~990 MB (cuantizado Q4_K_M) |

Datos de `ollama show qwen2.5:1.5b-instruct`. El **contexto máximo** es el techo de
`num_ctx`: Ollama usa por defecto una ventana menor, y nuestro `chat-config.json`
pide 4.096. Un token es un pedazo de palabra: en español, en promedio, algo menos
que una palabra.

Cada pedido del chat envía `options.num_ctx` y Ollama devuelve `prompt_eval_count`
(tokens del pedido completo). OF lo muestra en el encabezado de la conversación
y Gradio en el estado. Solo las instrucciones de `chat-prompt.txt` ocupan ~450 tokens.
