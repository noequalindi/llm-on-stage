# llmStage · texto → escenografía interactiva con Box2D

Escribir una escena: Qwen responde con texto y datos. OF valida esos datos y
construye un pequeño escenario de palabras físicas. El mouse las atrae o repele.
La respuesta se muestra con escritura progresiva, después de recibir el JSON completo.

**El LLM genera la descripción y los parámetros. Box2D calcula movimiento y
colisiones; OF dibuja.** No se generan imágenes, videos ni código ejecutable.
Cada solicitud crea una escena nueva; este ejemplo no conserva historial de chat.

| Escena base | Generada por Qwen: «escenario rojo de enojo» |
| --- | --- |
| ![Escena base](../docs/img/llmStage_base.png) | ![Escena fuego](../docs/img/llmStage_qwen_fuego.png) |

## Preparación manual

1. Ejecutar el [instalador de la clase](../README.md#1-instalar-python-ollama-y-qwen)
   y mantener Ollama activo. Se usa el mismo `qwen2.5:1.5b-instruct` de `llmPoster`.
2. Copiar la carpeta actualizada **ofxLocalLLM** a **OF/addons/ofxLocalLLM**.
3. Descargar [ofxBox2d de Vanderlin](https://github.com/vanderlin/ofxBox2d) y colocar
   su contenido en **OF/addons/ofxBox2d**. Deben quedar `src/` y `libs/` dentro.
   También se puede clonar desde la carpeta de OF:

   ```sh
   git clone https://github.com/vanderlin/ofxBox2d.git addons/ofxBox2d
   ```

   Si ya existe, no volver a clonar. Este ejemplo usa la API de ese addon,
   que incluye su biblioteca Box2D; no instalar Box2D por separado.
4. Copiar **llmStage/** completo a **OF/apps/myApps/llmStage/**.
5. En Project Generator importar esa carpeta. Verificar:

   | Campo | Valor |
   | --- | --- |
   | Project path | `OF/apps/myApps` |
   | Project name | `llmStage` |
   | Addons | `ofxLocalLLM`, `ofxBox2d` |
   | Additional source paths | vacío |

   Pulsar **Update**. No crear una subcarpeta `mySketch`: compilar `llmStage/src`.
6. Compilar con el IDE del SDK. Con Make en macOS/Linux:

   ```sh
   cd RUTA_A_OF/apps/myApps/llmStage
   make Release -j4
   make RunRelease
   ```

No requiere OSC, Docker, Gradio ni un servidor Python activo. OF consulta Ollama
por HTTP/JSON. URL/modelo se editan en `bin/data/connection.json` de este ejemplo.

Para mostrar ñ, tildes y ¿¡ el ejemplo usa `src/TextStyle.h` y
`bin/data/LiberationMono-Regular.ttf` (la fuente bitmap de OF solo tiene ASCII).
Copiarlos junto con el resto de `src/` y `bin/data/`; si falta el `.ttf`, vuelve a
la fuente bitmap.

## Qué probar

Al abrir hay una escena **GRABADA / SIN INFERENCIA** que permite probar la física.
Hacer click en el editor, escribir y pulsar **CREAR ESCENA / Enter**:

- «Un escenario lunar con palabras flotando y el mouse como imán».
- «Una lluvia de cajas cálidas que rebotan mucho y huyen del mouse».
- «Palabras tranquilas como piedras azules, con poco rebote y caída lenta».
- «Un escenario rojo de enojo: cajas furiosas que caen fuerte y huyen del mouse».

El modelo interpreta esas ideas dentro del contrato disponible. No puede inventar
objetos arbitrarios: ofrece círculos o cajas con etiquetas.

| Control | Acción |
| --- | --- |
| Mouse presionado dentro del escenario | Atraer o repeler cuerpos cercanos, según los datos |
| REARMAR ESCENA | Reiniciar posiciones con los mismos datos; no consulta al LLM |
| EJEMPLO GRABADO | Cargar `recorded.json`, sin inferencia |
| JSON | Inspeccionar la respuesta estructurada |
| VER COMPLETA / Enter durante la escritura | Mostrar toda la respuesta inmediatamente |
| Esc, luego E | Salir del editor y copiar diagnóstico |

Mientras espera HTTP muestra **Pensando…**; al llegar la respuesta válida muestra
**Escribiendo…**. La física continúa en ambos casos. Una respuesta rechazada
conserva la escena anterior. Ventana fija de 1280 × 800 para esta composición.

## Recorrido para leer en clase

```text
texto del alumno
  → ofApp::send() → ofxLocalLLM → HTTP / Ollama
  → ofApp::response() → SceneSpec::parse()
  → StagePhysics::build() → cuerpos y gravedad
  → update() / draw() → física, mouse y respuesta escrita
```

Los comentarios numerados en `ofApp.cpp` siguen ese recorrido.

| Archivo | Qué pueden modificar |
| --- | --- |
| [src/ofApp.cpp](src/ofApp.cpp) | Entrada, petición, manejo de respuesta y controles |
| [src/ofApp.h](src/ofApp.h) | Estado de conexión, escritura y escena |
| [src/SceneSpec.h](src/SceneSpec.h) | Contrato y límites antes de crear cuerpos |
| [src/StagePhysics.h](src/StagePhysics.h) | Plataformas, cuerpos, fuerzas del mouse y dibujo |
| [bin/data/system-prompt.txt](bin/data/system-prompt.txt) | Cómo se pide al modelo que interprete una escena |
| [bin/data/recorded.json](bin/data/recorded.json) | Datos para diseñar sin esperar al modelo |
| [../ofxLocalLLM/src/ofxTextReveal.h](../ofxLocalLLM/src/ofxTextReveal.h) | Animación de escritura reutilizable |

### Contrato: nueve campos

| Campo | Valores | Efecto |
| --- | --- | --- |
| `respuesta` | Texto hasta 700 caracteres | Explicación escrita progresivamente |
| `titulo` | Texto hasta 48 caracteres | Nombre del escenario |
| `paleta` | `mar`, `sol`, `noche`, `fuego` | Colores y fondo; `fuego` es rojo y el fondo late (enojo) |
| `gravedad` | Número de −5 a 15 | Negativo sube; cero flota; positivo cae |
| `rebote` | Número de 0 a 1 | Restitución de los cuerpos |
| `cantidad` | Entero de 6 a 24 | Cantidad de cuerpos |
| `forma` | `circulos`, `cajas` | Geometría física y visual |
| `interaccion` | `atraer`, `repeler` | Fuerza aplicada con el mouse |
| `palabras` | 1–6 textos de hasta 10 caracteres | Etiquetas que se repiten en los cuerpos |

### Reutilizar la conexión, cambiar la obra

En `response()`, `event["body"]["message"]["content"]` contiene el texto del modelo.
Ese texto es JSON y se valida en `SceneSpec::parse()` antes de modificar la escena:

```cpp
const auto datos = SceneSpec::parse(textoDelModelo);
stage.build(datos); // Sustituir por la representación propia del alumno.
typing.start(datos.respuesta, ofGetElapsedTimef(), 40);
```

1. Primero cambiar `draw()` de `StagePhysics`: color, fondo, texturas o tipografía.
2. Cambiar plataformas y posiciones iniciales en `setup()` / `build()`.
3. Modificar radio y fuerza del mouse en `update()`.
4. Para agregar un campo nuevo, modificar juntos el prompt, `SceneSpec::schema()`,
   `SceneSpec::parse()` y
   `recorded.json`; después usarlo en la física o en el dibujo.

No enviar una petición en cada frame: llamar `send()` desde una acción elegida.
`llm.update()` sí corre en cada frame para recibir los eventos en el hilo principal.
La solicitud incluye un [JSON Schema en `format`](https://ollama.com/blog/structured-outputs)
para guiar la generación. La validación local conserva la escena anterior si el
modelo incumple un límite; el esquema no sustituye esa comprobación.
La simulación usa un paso fijo de 1/60 s y un máximo de 24 cuerpos.

Después de editar, actualizar la copia en el SDK y recompilar C++; los cambios de
prompt/configuración se leen al reiniciar. Este ejemplo no instala dependencias
al ejecutarse. La validación nativa documentada corresponde a macOS / OF 0.12.1;
Windows y Linux requieren compilar y comprobar con sus respectivos SDK.
