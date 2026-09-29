# Validación · conexión directa a Ollama

## Versión de esta entrega

La clase contiene el cliente Python, `ofxLocalLLM` y `llmPoster`. Ambos clientes
consultan Ollama directamente. No incluye el servidor Model Video Explorer,
adaptadores multimedia, perfiles MVE, polling de trabajos ni OSC. Gradio
incorpora sus dependencias web para la interfaz visual; OF no depende de ella.
Las comprobaciones de versiones anteriores con un servidor intermedio no se
usan como evidencia de este recorrido.

## Verificado en macOS Apple Silicon — 27 de septiembre de 2026

- Instalador ejecutado con un entorno `.venv` nuevo: Python 3.11 y Pydantic.
  Después se agregó Gradio 6.28.0 y sus dependencias para la interfaz visual.
  No se instala `model_video_explorer` ni PyTorch. Se reutilizó Ollama instalado
  y se verificó Qwen descargado.
- Qwen precargado mediante Ollama: este equipo informó 100% GPU y 2.1 GB. Es
  ubicación en memoria, no utilización del procesador ni un benchmark.
- **36 pruebas aprobadas** de instalación, cliente Python e interfaz: arranque y reutilización,
  errores de descarga, conservación de entornos incompletos, precarga, URL/pedido
  nativo de Ollama y rechazo de HTTP fallido, mensajes incompletos o categorías inválidas.
- Project Generator actualizado con solo `ofxLocalLLM`; compilación y enlace de
  `llmPoster` con OF 0.12.1, sin `ofxOsc` ni el addon anterior.
- **OF → Ollama → Qwen real → JSON validado por C++**, usando los datos de una
  copia temporal independiente. No se inició un servicio Python intermedio.
- Los seis textos de la práctica Python de esa copia recibieron y validaron
  respuestas del modelo real directamente desde Ollama.
- Prueba HTTP sintética del ejecutable OF: éxito y rechazo de HTTP 404, cuerpo
  no JSON, respuesta incompleta y JSON que incumple el contrato del afiche.
- Enlaces de documentación revisados. Instaladores Bash con LF y permiso
  ejecutable; Windows conserva CRLF y PowerShell UTF-8 con BOM.

## Interfaz Gradio verificada

- Arranque local en 127.0.0.1:7860, sin enlace público ni llamada al modelo al abrir.
- Navegación con Chrome en macOS: consulta real desde el botón, respuesta de Qwen,
  JSON validado y afiche actualizado.
- Entrada vacía conserva el resultado anterior; botón de ejemplo grabado vuelve
  a una composición claramente marcada SIN INFERENCIA.
- Revisión visual de escritorio y ancho móvil; corrección de contraste del afiche.
- Pruebas de generación válida, respuesta inválida, modo grabado sin inferencia y
  escape de texto HTML en título y motivo.

## Repetir pruebas

Desde la raíz, después de instalar:

```sh
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python -m pytest tests -q
```

Para comprobar el ejecutable OF, ya compilado:

```sh
# Fixture HTTP local: no ejecuta un modelo
.venv/bin/python tools/check_lesson26.py /ruta/al/ejecutable/llmPoster

# Ollama abierto y Qwen descargado: inferencia real
.venv/bin/python tools/check_lesson26.py /ruta/al/ejecutable/llmPoster --real
```

El primer comando inicia únicamente un servidor de prueba temporal. El segundo
usa Ollama existente. Ambos copian la práctica y los datos OF a una carpeta
independiente; la evidencia se guarda en `.local/validation/`, excluida de Git.
No modifican los datos de tu OF ni cierran Ollama.

En Windows usar `.venv\Scripts\python.exe`. Las pruebas unitarias de Bash necesitan
ese intérprete. La prueba sintética abre un puerto local y ejecuta ventanas OF.

## Límites

No se probó una instalación nativa desde cero en Windows/Linux ni sus builds OF.
Tampoco se reinstaló Ollama desde cero en este Mac. Las variantes CPU/GPU mixtas
se comprueban con dobles en las pruebas; no se midió CUDA/ROCm en otro hardware.

El addon recibe respuestas completas (`stream: false`); no entrega tokens en
streaming. Cerrar el cliente interrumpe la transferencia, pero Ollama puede
terminar una inferencia ya iniciada. La validación comprueba el contrato, no
calidad creativa, precisión ni ausencia de sesgos.

## Nombres de los instaladores

Entradas para alumnos: `install_env_mac.command`, `install_env_linux.sh` e
`install_env_windows.cmd`. Los helpers compartidos están en `scripts/`.
Tras el cambio pasaron las 20 pruebas de orquestación del instalador, incluida
la delegación de ambas entradas Unix. Se comprobaron sintaxis Bash, permisos
y formatos LF/CRLF/BOM; no se ejecutó el instalador nativo de Windows.


## Chat y tono visual (2026-09-27)

- `tools/check_of_headers.py`: sintaxis C++ del addon y sketch correcta con OF 0.12.1 macOS.
- `tests/chat_session.cpp`: validacion de salida, rechazo sin modificar estado,
  historial ordenado, limite de cuatro intercambios y reinicio correctos.
  (Ver la sección 2026-09-28: el límite ahora es configurable.)
- Compilacion y enlace de una copia temporal con el core de OF ya compilado:
  `ReleaseNoOF`; apertura y captura del modo CHAT con respuesta grabada verificadas.
- Dos turnos HTTP reales con Qwen y el mismo formato del sketch: ambos JSON
  aceptados por `ChatSession`, con cambio de calma/lento a alegria/rapido.
  Esto valida el transporte/contrato; no implica exactitud semantica general.
- Con el prompt final, Qwen respondio correctamente a "Cuanto es 2+2?" y
  recupero "Noelia" del turno anterior al preguntar "Como me llamo?". Ambas
  respuestas pasaron la validacion C++.
- La copia del proyecto del usuario dentro del SDK no se actualizo automaticamente.

Para repetir el test del historial (reemplazar RUTA_A_OF):

```sh
clang++ -std=c++17 -I llmPoster/src -I RUTA_A_OF/libs/json/include tests/chat_session.cpp -o /tmp/test-chat-session
/tmp/test-chat-session
```

### Indicador de espera

Sintaxis, compilacion y enlace correctos con OF 0.12.1 macOS. Verificacion visual
en una copia temporal con solicitud pendiente simulada: dos capturas en distintos
frames muestran los puntos animados en el panel y el boton, sin tapar el texto.
El estado usa `pending`, que se limpia tanto en respuesta como en error.


## Escritura progresiva y llmStage / Box2D (2026-09-27)

- `tests/text_reveal.cpp`: límites UTF-8 (ñ, emoji), tiempo transcurrido,
  finalización anticipada, reinicio, texto vacío y limpieza correctos.
- `tests/scene_spec.cpp`: tipos, campos requeridos, categorías, cantidades,
  rangos físicos, etiquetas y rechazo de salidas inválidas correctos.
  También siguen pasando los contratos de chat y afiche.
- Sintaxis del addon y ambos sketches comprobada con `check_of_headers.py --stage`.
- Compilación y enlace de ambos ejemplos en copias temporales con OF 0.12.1 macOS
  (`ReleaseNoOF`, reutilizando el core instalado). Las bibliotecas incluidas en
  ofxBox2d emiten advertencias del compilador; no impidieron el enlace.
- Verificación visual de la conversación parcialmente escrita, cursor, indicador
  y botón VER COMPLETA; escena Box2D y etiquetas comprobadas con capturas nativas.
- Prueba nativa temporal de física: veinte reconstrucciones alternando círculos,
  cajas y 6/24 cuerpos sin acumular cuerpos en el mundo; fuerza del mouse con
  dirección correcta tanto para atraer como para repeler.
- Primera consulta real sin esquema: Qwen devolvió gravedad −10 y fue rechazada.
  La solicitud final incorpora `SceneSpec::schema()` y temperatura 0. La consulta
  real final con `qwen2.5:1.5b-instruct` pasó (`STAGE_CONTRACT_OK bodies=8`). Esto
  verifica un caso de transporte, contrato y creación; no garantiza que cualquier
  pedido respete el contrato ni que la interpretación sea siempre acertada.
- Código de `ofxLocalLLM`, `llmPoster` y `llmStage` sincronizado con el SDK local.
  Proyecto `llmStage` generado con ambos addons, sin subcarpeta de plantilla.
  Los fuentes instalados se compararon byte por byte con los de la clase.

La escritura es una simulación posterior a la respuesta completa (`stream: false`).
No se comprobó compilación nativa en Windows/Linux ni se midió rendimiento allí.
Las copias temporales de QA no forman parte de la entrega.

Para repetir desde la raíz de la clase (reemplazar RUTA_A_OF):

```sh
clang++ -std=c++17 -I ofxLocalLLM/src tests/text_reveal.cpp -o /tmp/test-text-reveal
/tmp/test-text-reveal
clang++ -std=c++17 -I llmStage/src -I RUTA_A_OF/libs/json/include tests/scene_spec.cpp -o /tmp/test-scene-spec
/tmp/test-scene-spec
python3 tools/check_of_headers.py RUTA_A_OF --stage
```

`--stage` requiere `ofxBox2d` instalado en el SDK; sin esa opción se comprueba
solo el addon de conexión y `llmPoster`. La herramienta de sintaxis está orientada
al SDK de macOS. Los ejemplos se compilan con el proyecto generado para cada SO.

## Fuente, escenario fuego, Gradio y contexto (2026-09-28)

- `llmPoster` y `llmStage`: `make Release` con OF 0.12.1 macOS; capturas con
  títulos, respuestas y etiquetas con ñ, tildes y ¿¡ (`TextStyle.h` + Liberation Mono).
- `llmStage`: paleta `fuego` aceptada por `SceneSpec` (`tests/scene_spec.cpp`) y
  dibujada en rojo; Qwen real eligió `fuego` para «un escenario rojo de enojo».
- `tests/chat_session.cpp`: memoria configurable (bajar el límite olvida lo más
  viejo, memoria 0 envía solo system + mensaje, tope 20, `clear()`).
- `tests/test_modos.py` (Python): memoria, `num_ctx` en el pedido, memoria 0,
  configuración compartida con OF, contratos `Chat`/`Escena` y escape de HTML.
  `pytest`: 52 pruebas correctas.
- Gradio: la interfaz levanta con las pestañas AFICHE, CHAT y ESCENARIO; la física
  de `escenario.js` se verificó con capturas de Chrome headless (caída, apilado,
  plataformas, paletas `mar` y `fuego`).
- Qwen real, CHAT: memoria 4 recordó el nombre; memoria 0 respondió otro nombre.
  `prompt_eval_count`: 453 → 539 tokens con memoria, 453 → 458 sin memoria.
- Sin probar: `num_ctx` muy chico (≤ 1024); con `num_predict` 768 puede no quedar lugar.
