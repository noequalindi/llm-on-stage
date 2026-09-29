"""Representation is our code: the LLM only supplies validated fields."""
from html import escape
from contrato import Afiche, Escena


def dibujar(afiche: Afiche, *, grabada=False):
    # Never execute HTML returned by the model. Only these validated categories
    # enter CSS class names; the generated text is always escaped.
    fuente = "EJEMPLO GRABADO · SIN INFERENCIA" if grabada else "RESPUESTA DEL LLM · VALIDADA"
    return f'''<article class="afiche {afiche.paleta} {afiche.animo} {afiche.ritmo}">
      <div class="orbita" aria-hidden="true"></div>
      <div class="afiche-contenido">
        <div class="afiche-fuente">{fuente}</div>
        <div class="afiche-numero" aria-hidden="true">26 /</div>
        <h2>{escape(afiche.titulo)}</h2>
        <p class="afiche-motivo">{escape(afiche.motivo)}</p>
        <div class="afiche-etiquetas"><span>{afiche.animo}</span><span>{afiche.ritmo}</span><span>{afiche.paleta}</span></div>
        <div class="afiche-pie">TEXTO GENERADO POR EL MODELO / DISEÑO PROGRAMADO</div>
      </div>
    </article>'''


def dibujar_escena(escena: Escena, *, grabada=False):
    # Python no dibuja la física: deja los datos validados en data-spec y
    # escenario.js arma la escena en el navegador (como StagePhysics en OF).
    # escape(quote=True) impide que el texto del modelo "rompa" el atributo HTML.
    fuente = "EJEMPLO GRABADO · SIN INFERENCIA" if grabada else "RESPUESTA DEL LLM · VALIDADA"
    datos = escape(escena.model_dump_json(), quote=True)
    return f'''<section class="escena-panel">
      <div class="escena-barra"><span>{fuente}</span>
        <button class="rearmar" type="button">REARMAR ESCENA</button></div>
      <div class="escena" data-spec="{datos}"></div>
      <p class="escena-ayuda">Mantené el mouse presionado sobre el escenario: <strong>{escena.interaccion}</strong>
        · forma: {escena.forma} · cuerpos: {escena.cantidad} · gravedad: {escena.gravedad:g} · rebote: {escena.rebote:g}</p>
    </section>'''
