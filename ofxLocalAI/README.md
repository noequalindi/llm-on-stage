# ofxLocalAI

Addon pequeño para **consultar Ollama directamente desde openFrameworks**.
No ejecuta modelos, no inicia Python y no depende de otros addons. Usa libcurl,
incluido en el SDK OF utilizado para la prueba.

## Actualizar desde el nombre anterior

El addon antes se llamaba `ofxLocalLLM`. Ahora la carpeta, la clase C++ y el
archivo principal se llaman **ofxLocalAI**.

1. Copiar `ofxLocalAI/` a `OF/addons/ofxLocalAI/`.
2. Usar `ofxLocalAI` en `addons.make` y seleccionar ese addon en Project
   Generator; quitar la selección del nombre anterior.
3. Incluir `ofxLocalAI.h` y declarar el cliente como `ofxLocalAI`.
4. Pulsar Update en Project Generator y recompilar.

`llmPoster` y `llmStage` ya usan el nuevo nombre. Los métodos y contratos HTTP
conservan su comportamiento. Un proyecto antiguo puede seguir necesitando la
carpeta anterior hasta que se migren sus fuentes.

## Uso

- Agregar `ofxLocalAI` con Project Generator e incluir `ofxLocalAI.h`.
- Crear un miembro `ofxLocalAI llm` en `ofApp`.
- En `setup()`: registrar listeners de `llm.response` y `llm.error`; llamar
  `llm.setup("http://127.0.0.1:11434")`.
- Enviar con `llm.chat(request)` desde un evento de interacción.
- Llamar `llm.update()` en cada `ofApp::update()`.
- En `exit()`: `llm.close()` y retirar los listeners.

```cpp
ofJson request = {
    {"model", "qwen2.5:1.5b-instruct"},
    {"messages", ofJson::array({
        {{"role", "user"}, {"content", "Inventá una frase sobre la lluvia"}}
    })}
};
llm.chat(request);
```

El evento `response` contiene `request: "chat"` y `body`, la respuesta de Ollama.
El texto está en `event["body"]["message"]["content"]`. Se puede pedir texto
libre o agregar `format: "json"` como hace `llmPoster`. Validar la salida antes
de usarla en la experiencia.

`listModels()` consulta `/api/tags` y emite `request: "models"`.
`chat()` envía `/api/chat` con `stream: false`. `chat()` y `listModels()` devuelven
`false` si no se configuró el cliente o ya hay una solicitud pendiente.
El consumidor debe llamar `update()` para recibir y liberar esa solicitud.

## Hilos, límites y cierre

La red corre en un worker; los eventos se entregan en `update()` desde el hilo
principal. Llamar a la API del addon desde ese hilo. Una solicitud a la vez,
sin reenvíos automáticos. Conexión: hasta 5 s; catálogo: 10 s; chat: 180 s;
respuesta: hasta 1 MiB. Los errores HTTP y las respuestas incompletas se notifican
por `error`. `close()` interrumpe la transferencia y espera al worker; no envía
una orden para detener Ollama ni garantiza cancelar su inferencia.

El [ejemplo completo](../llmPoster/README.md) muestra el contrato JSON del afiche.
Referencia: [API de chat de Ollama](https://docs.ollama.com/api/chat).

## Escritura progresiva opcional

`ofxTextReveal.h` no depende de OF ni consulta la red. Permite animar la respuesta
completa después de validarla, sin cortar caracteres UTF-8:

```cpp
// Miembro de ofApp (incluir ofxTextReveal.h):
ofxTextReveal typing;

// Al recibir un texto válido:
typing.start(respuesta, ofGetElapsedTimef(), 40); // caracteres por segundo

// En update():
typing.update(ofGetElapsedTimef());

// En draw():
ofDrawBitmapString(typing.visible(), 30, 100);
// typing.active(): quedan caracteres. typing.finish(): mostrar todo.
```

Esto **simula escritura**; no es streaming de tokens. Conservar la respuesta
completa para el historial y los datos visuales. Los ejemplos comentados
[llmPoster](../llmPoster/README.md) y [llmStage](../llmStage/README.md) muestran dos
representaciones distintas sobre la misma conexión.
