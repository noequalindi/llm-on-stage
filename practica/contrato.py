"""Contratos de datos: los mismos que usan los ejemplos de OF.

El modelo devuelve texto; estos modelos de Pydantic deciden si ese texto
se puede usar. Si algo no cumple, se lanza un error y la interfaz conserva
lo anterior. Es el "control de calidad en la puerta".

    Afiche  <-> llmPoster/src/PosterSpec.h   (modo AFICHE, 5 campos)
    Chat    <-> llmPoster/src/ChatSession.h  (modo CHAT: respuesta + visual)
    Escena  <-> llmStage/src/SceneSpec.h     (escenario con física, 9 campos)

Para agregar una opción (por ejemplo una paleta), cambiarla acá, en el prompt
de bin/data y en el dibujo (vista.py / estilo.css / escenario.js).
"""
from typing import Annotated, Literal
from pydantic import BaseModel, ConfigDict, Field

# Texto de una sola línea, sin tabulaciones ni caracteres de control.
UnaLinea = r"^[^\x00-\x1f]+$"
# Texto que puede tener saltos de línea, con al menos un carácter visible.
_Libre = r"[^\x00-\x08\x0b\x0c\x0e-\x1f]*"
VariasLineas = "^" + _Libre + r"[^\s\x00-\x1f]" + _Libre + "$"


class Afiche(BaseModel):
    # extra="forbid": campos de más = rechazo. strict=True: "5" no vale como 5.
    model_config = ConfigDict(extra="forbid", strict=True)
    titulo: str = Field(min_length=1, max_length=60, pattern=r"^[^\r\n\t]+$")
    # Literal = lista cerrada: el modelo solo puede elegir estas opciones.
    animo: Literal["calma", "alegria", "tension"]
    ritmo: Literal["lento", "medio", "rapido"]
    paleta: Literal["mar", "sol", "noche"]
    motivo: str = Field(max_length=200)


class Chat(BaseModel):
    """Respuesta del modo CHAT: texto para conversar + un afiche que acompaña el tono."""
    model_config = ConfigDict(extra="forbid", strict=True)
    respuesta: str = Field(min_length=1, max_length=700, pattern=VariasLineas)
    visual: Afiche  # Reutiliza el contrato del afiche, igual que ChatSession.h.


class Escena(BaseModel):
    """Escenario físico: el modelo propone números y categorías; la física es nuestra."""
    model_config = ConfigDict(extra="forbid", strict=True)
    respuesta: str = Field(min_length=1, max_length=700, pattern=VariasLineas)
    titulo: str = Field(min_length=1, max_length=48, pattern=UnaLinea)
    paleta: Literal["mar", "sol", "noche", "fuego"]  # "fuego": el escenario rojo del enojo.
    # Rangos: protegen la obra (sin ellos podría pedir 10.000 cuerpos).
    gravedad: float = Field(ge=-5, le=15)  # negativa sube, 0 flota, positiva cae
    rebote: float = Field(ge=0, le=1)
    cantidad: int = Field(ge=6, le=24)
    forma: Literal["circulos", "cajas"]
    interaccion: Literal["atraer", "repeler"]
    palabras: list[Annotated[str, Field(min_length=1, max_length=10, pattern=UnaLinea)]] = Field(min_length=1, max_length=6)


def validar(texto: str) -> Afiche:
    if len(texto.encode("utf-8")) > 4096:
        raise ValueError("Respuesta demasiado larga")
    return Afiche.model_validate_json(texto)


def validar_chat(texto: str) -> Chat:
    if len(texto.encode("utf-8")) > 8192:
        raise ValueError("Respuesta de chat demasiado larga")
    return Chat.model_validate_json(texto)


def validar_escena(texto: str) -> Escena:
    if len(texto.encode("utf-8")) > 8192:
        raise ValueError("Escena demasiado larga")
    return Escena.model_validate_json(texto)


def esquema_escena() -> dict:
    """JSON Schema que viaja en "format": copia de SceneSpec::schema() en C++.

    Guía al modelo, pero no reemplaza validar_escena(): siempre validar lo que llega.
    Si cambian una opción en Escena, cámbienla también acá.
    """
    texto = lambda maximo: {"type": "string", "minLength": 1, "maxLength": maximo}
    opciones = lambda *valores: {"type": "string", "enum": list(valores)}
    return {
        "type": "object", "additionalProperties": False,
        "required": ["respuesta", "titulo", "paleta", "gravedad", "rebote", "cantidad", "forma", "interaccion", "palabras"],
        "properties": {
            "respuesta": texto(400),
            "titulo": texto(48),
            "paleta": opciones("mar", "sol", "noche", "fuego"),
            "gravedad": {"type": "number", "minimum": -5, "maximum": 15},
            "rebote": {"type": "number", "minimum": 0, "maximum": 1},
            "cantidad": {"type": "integer", "minimum": 6, "maximum": 24},
            "forma": opciones("circulos", "cajas"),
            "interaccion": opciones("atraer", "repeler"),
            "palabras": {"type": "array", "minItems": 1, "maxItems": 6, "items": texto(10)},
        },
    }
