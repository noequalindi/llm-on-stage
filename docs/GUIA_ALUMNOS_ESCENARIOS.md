# Dónde se definen los escenarios · llmPoster y llmStage

Guía para modificar los ejemplos de la clase 26 y agregar opciones propias:
una paleta, un ánimo, una forma o un movimiento.

## La idea en una frase

> **El prompt le dice al modelo qué puede elegir.
> El contrato controla que haya elegido bien.
> El dibujo decide cómo se ve.**

El modelo (Qwen) nunca dibuja ni escribe código. Solo devuelve **palabras y números**
dentro de un JSON, por ejemplo `"paleta": "mar"`. Qué significa `"mar"` en pantalla
lo decide **nuestro código**. Es como una partitura: el modelo escribe "piano" o
"allegro", y la orquesta (openFrameworks) decide cómo suena.

Por eso cada opción aparece en **tres lugares**, y hay que cambiar los tres juntos:

| Capa | Qué hace | Tipo de archivo |
|---|---|---|
| **1. Prompt** | Le dice al modelo qué opciones existen y cuándo usar cada una | `.txt` en `bin/data/` |
| **2. Contrato** | Acepta o rechaza la respuesta del modelo | `.h` en `src/` |
| **3. Dibujo** | Convierte la opción elegida en colores, formas y movimiento | `.h` en `src/` |

Qué pasa si se olvidan de una capa:

- **Solo en el prompt:** el modelo elige la opción nueva, pero el contrato la **rechaza**
  y aparece *"Respuesta rechazada"*.
- **En prompt y contrato, pero no en el dibujo:** se acepta, pero se ve igual que otra
  opción. El código usa la que esté en el último `else` (el caso por defecto).
- **Solo en el dibujo:** nunca aparece, porque el modelo no sabe que existe.

---

## llmPoster · afiche y chat

El modelo elige cinco campos: `titulo`, `animo`, `ritmo`, `paleta` y `motivo`.

### 1. Prompt · `llmPoster/bin/data/`

Este ejemplo tiene **dos prompts**, uno por modo, y hay que actualizar los dos:

| Archivo | Modo | Dónde están las opciones |
|---|---|---|
| `system-prompt.txt` | AFICHE | líneas 4 a 6 |
| `chat-prompt.txt` | CHAT | líneas 7 a 9 |

```text
"animo": uno de "calma", "alegria", "tension";
"ritmo": uno de "lento", "medio", "rapido";
"paleta": uno de "mar", "sol", "noche";
```

Son archivos de texto: **no hace falta recompilar**, alcanza con reiniciar la app.

### 2. Contrato · `llmPoster/src/PosterSpec.h`

En la función `parse()`, las listas cerradas (alrededor de la línea 36):

```cpp
if (value.animo!="calma" && value.animo!="alegria" && value.animo!="tension") throw ...
if (value.ritmo!="lento" && value.ritmo!="medio" && value.ritmo!="rapido") throw ...
if (value.paleta!="mar" && value.paleta!="sol" && value.paleta!="noche") throw ...
```

Si la respuesta trae un valor que no está en la lista, se rechaza **toda la respuesta**
y el afiche anterior queda como estaba. El modo CHAT usa este mismo contrato para su
parte visual (`ChatSession.h`), así que no hay que cambiar nada más.

### 3. Dibujo · `llmPoster/src/PosterView.h`

Todo está en la función `drawPoster()`, dividido en tres **mapeos**:

| Mapeo | Líneas aprox. | Qué controla |
|---|---|---|
| `paleta` → colores | 15-16 | `background` (fondo) e `ink` (tinta), en RGB |
| `ritmo` → velocidad | 18 | `speed`: qué tan rápido se mueve todo |
| `animo` → movimiento | 20 y 31-47 | `amplitude` (cuánto se mueve) y el gesto de cada ánimo |

Los gestos de cada ánimo:

- **calma:** tres elipses que respiran.
- **alegria:** cinco círculos que rebotan.
- **tension:** una línea en zigzag. Además el título se inclina (línea 64).

### Ejemplo: agregar la paleta `"bosque"` en llmPoster

1. **Prompt:** en `system-prompt.txt` y en `chat-prompt.txt`, cambiar la línea de paleta:
   ```text
   "paleta": uno de "mar", "sol", "noche", "bosque";
   ```
2. **Contrato:** en `PosterSpec.h`, sumarla a la lista:
   ```cpp
   if (value.paleta!="mar" && value.paleta!="sol" && value.paleta!="noche" && value.paleta!="bosque") throw ...
   ```
3. **Dibujo:** en `PosterView.h`, darle colores antes del caso por defecto:
   ```cpp
   const ofColor background=spec.paleta=="mar" ? ofColor(17,40,73) : spec.paleta=="sol" ? ofColor(84,37,24)
       : spec.paleta=="bosque" ? ofColor(18,48,30) : ofColor(31,23,60);
   const ofColor ink=spec.paleta=="mar" ? ofColor(142,214,255) : spec.paleta=="sol" ? ofColor(255,212,123)
       : spec.paleta=="bosque" ? ofColor(150,230,160) : ofColor(228,178,255);
   ```

---

## llmStage · escenario con física (Box2D)

El modelo elige nueve campos. Algunos son categorías (`paleta`, `forma`, `interaccion`)
y otros son **números físicos** (`gravedad`, `rebote`, `cantidad`). Box2D calcula caídas
y choques; OF dibuja.

### 1. Prompt · `llmStage/bin/data/system-prompt.txt`

Hay una línea por campo. La paleta está en la línea 5:

```text
paleta: "mar", "sol", "noche" o "fuego". Usa "fuego" (rojo) para enojo, furia, peligro o calor intenso.
```

Fíjense que además de listar la opción, el prompt le explica **cuándo** usarla.
Eso ayuda mucho a un modelo chico como Qwen 1.5B.

### 2. Contrato · `llmStage/src/SceneSpec.h`

Acá hay **dos lugares**, porque este ejemplo también manda un esquema a Ollama:

| Función | Línea aprox. | Para qué |
|---|---|---|
| `schema()` | 37 | Viaja en el pedido HTTP y guía al modelo (ayuda, no garantía) |
| `parse()` | 68 | Control final: si no cumple, se rechaza y se conserva la escena anterior |

```cpp
{"paleta",{{"type","string"},{"enum",json::array({"mar","sol","noche","fuego"})}}},   // schema()
if (s.paleta!="mar" && s.paleta!="sol" && s.paleta!="noche" && s.paleta!="fuego") throw ... // parse()
```

Los **rangos numéricos** también están acá (por ejemplo, gravedad de −5 a 15 y
cantidad de 6 a 24). Existen para proteger la obra: sin ellos el modelo podría pedir
10.000 cuerpos y la compu se colgaría.

### 3. Dibujo y física · `llmStage/src/StagePhysics.h`

| Qué | Función · líneas aprox. |
|---|---|
| Colores de cada paleta (`ink` y `bg`) | `draw()` · 87-88 |
| Efecto especial de "fuego" (el fondo late y las líneas tiemblan) | `draw()` · 90-96 |
| Forma de cada cuerpo (círculo o caja) | `draw()` · 110-111 |
| Plataformas fijas (las dos barras inclinadas) | `setup()` · 25 |
| Gravedad, rebote, cantidad y posición inicial | `build()` · 37 |
| Fuerza del mouse (radio `280`, intensidad `20`) | `update()` · 64-75 |

El escenario **"fuego"** es un buen ejemplo para mirar: el modelo solo elige la palabra
`"fuego"`. Que el fondo sea rojo, que lata y que las líneas tiemblen lo decidimos
nosotros en `draw()`.

### Ejemplo: agregar la paleta `"bosque"` en llmStage

1. **Prompt** (`system-prompt.txt`, línea 5):
   ```text
   paleta: "mar", "sol", "noche", "fuego" o "bosque". Usa "bosque" para naturaleza, calma verde o selva.
   ```
2. **Contrato** (`SceneSpec.h`): sumar `"bosque"` en `schema()` **y** en `parse()`.
3. **Dibujo** (`StagePhysics.h`, dentro de `draw()`): sumar el caso antes del color por defecto:
   ```cpp
   const ofColor ink = ... : fuego ? ofColor(255,120,95) : current.paleta=="bosque" ? ofColor(150,230,160) : ofColor(144,215,255);
   const ofColor bg  = ... : fuego ? ofColor(78,14,18)   : current.paleta=="bosque" ? ofColor(18,48,30)   : ofColor(17,40,73);
   ```

---

## En la práctica de Python (Gradio)

La interfaz `practica/interfaz.py` tiene las mismas tres capas, en otros archivos:

| Capa | AFICHE / CHAT | ESCENARIO |
|---|---|---|
| Prompt | los mismos `.txt` de `llmPoster/bin/data/` | el mismo `.txt` de `llmStage/bin/data/` |
| Contrato | `practica/contrato.py` → `Afiche`, `Chat` | `practica/contrato.py` → `Escena` y `esquema_escena()` |
| Dibujo | `practica/vista.py` + `practica/estilo.css` (`.afiche.mar`, `.afiche.sol`…) | `practica/escenario.js` → `PALETAS` y `dibujar()` |

Los prompts son **compartidos**: si agregan una opción en el `.txt`, agréguenla en el
contrato de OF **y** en el de Python, o la práctica va a rechazar esas respuestas.

## El contexto del chat (memoria y num_ctx)

El modelo **no recuerda nada** entre mensajes: nuestro código le reenvía la
conversación cada vez. Cuánto le reenviamos es un parámetro que pueden cambiar:

| Qué | Dónde | Efecto |
|---|---|---|
| `memoria` | `llmPoster/bin/data/chat-config.json` · teclas **+ / −** en OF · control en Gradio | Cuántos intercambios se reenvían. `0` = cada mensaje llega solo |
| `num_ctx` | el mismo archivo · desplegable en Gradio | Cuántos tokens entran en cada pedido. Si no alcanza, Ollama borra lo más viejo |
| Borrar contexto | tecla **C** o **NUEVO CHAT** en OF · botón en Gradio | Vacía el historial; el modelo no cambia |

¿Cuánto entra? Datos de Qwen2.5 1.5B, el modelo que usamos:

| | DistilBERT (clase de datos) | Qwen2.5 1.5B |
|---|---|---|
| Contexto máximo | 512 tokens | **32.768 tokens** |
| Vocabulario | 30.522 tokens | **151.936 tokens** |
| Parámetros | 66 millones | **1.543 millones** |

Un **token** es un pedazo de palabra (en español, en promedio, algo menos que una
palabra). Las instrucciones del chat ya ocupan unos **450 tokens** antes de que
escriban nada. `num_ctx` puede llegar hasta 32.768, pero cuanto más grande, más
memoria RAM usa la compu.

Experimentos para la entrega:

1. **Memoria 0:** «Me llamo …» y después «¿Cómo me llamo?». ¿Qué responde? (Spoiler:
   suele inventar un nombre.)
2. **Memoria 1 vs 4:** contarle tres datos en tres mensajes y preguntar por el primero.
3. **Mirar los tokens:** solo las instrucciones ocupan ~450. ¿Cuánto crece cada mensaje?
4. **`num_ctx` chico** (por ejemplo 1024) con memoria alta: ¿cuándo empieza a olvidar?

## Probar sin esperar al modelo

Cada ejemplo tiene una respuesta **grabada** en `bin/data/recorded.json`: es lo que
aparece al abrir la app. Para probar la opción nueva sin depender de Qwen:

1. Abrir `recorded.json` y cambiar el valor, por ejemplo `"paleta": "bosque"`.
2. Reiniciar la app. En llmPoster también sirve la tecla **R** (con Esc antes, para
   salir del editor). En llmStage, el botón **EJEMPLO GRABADO**.
3. Si aparece **"JSON rechazado"** o **"Error en recorded.json"**, falta agregar la
   opción en el contrato.

Así separan dos problemas distintos: **¿mi código funciona?** (se prueba con
`recorded.json`) y **¿el modelo elige bien?** (se prueba con el modelo).

## Probar con el modelo

- Escriban pedidos que "llamen" a la opción nueva: *"un bosque húmedo y silencioso"*.
- Usen el botón **JSON** para ver qué eligió el modelo y compararlo con lo que se ve.
- Si falla: **Esc** y después **E** copia el diagnóstico completo al portapapeles.
- Un modelo chico no siempre elige lo que esperamos. Registren un caso que funcionó
  y uno que no: es parte de la consigna.

## Antes de compilar

- Editen los archivos **de la carpeta que compilan**: `OF/apps/myApps/llmPoster` o
  `OF/apps/myApps/llmStage`. Si editan otra copia, no van a ver los cambios.
- Los cambios en `bin/data/*.txt` y `recorded.json` solo necesitan reiniciar la app.
- Los cambios en `src/*.h` necesitan **recompilar**.
- Los números de línea son aproximados y cambian a medida que editan. Busquen por
  nombre de función: `parse()`, `schema()`, `draw()`, `drawPoster()`.

## Resumen rápido

| Quiero cambiar... | llmPoster | llmStage |
|---|---|---|
| Qué opciones conoce el modelo | `bin/data/system-prompt.txt` y `chat-prompt.txt` | `bin/data/system-prompt.txt` |
| Qué valores se aceptan | `src/PosterSpec.h` → `parse()` | `src/SceneSpec.h` → `schema()` y `parse()` |
| Colores | `src/PosterView.h` → `background`, `ink` | `src/StagePhysics.h` → `draw()` → `bg`, `ink` |
| Movimiento | `src/PosterView.h` → `speed`, `amplitude`, gestos | `src/StagePhysics.h` → `build()` y `update()` |
| Formas | `src/PosterView.h` → gestos de cada ánimo | `src/StagePhysics.h` → `build()` y `draw()` |
| La escena de prueba | `bin/data/recorded.json` | `bin/data/recorded.json` |

**Pregunta para la entrega:** ¿qué decidió el modelo y qué decidieron ustedes?
