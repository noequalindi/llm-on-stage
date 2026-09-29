// ESCENARIO con física en el navegador. Equivalente web de llmStage/src/StagePhysics.h.
//
// Python (vista.py) solo deja en la página un <div class="escena" data-spec="...">
// con los datos YA VALIDADOS por contrato.py. Este script los lee y arma la escena.
// El modelo no escribe este código ni lo ejecuta: solo eligió números y palabras.
//
// Es un motor de física mínimo, escrito para leerlo en clase (Box2D es mucho más
// completo). Simplificaciones: los cuerpos no giran y las cajas chocan con las
// plataformas como si fueran círculos. Se usan las mismas medidas que en OF.
(() => {
  const PX = 30;                    // píxeles por metro, igual que ofxBox2d
  const PASO = 1 / 60;              // paso fijo: la física no depende de los FPS
  const RADIO_MOUSE = 280;          // EJERCICIO: radio de la fuerza del mouse
  const FUERZA_MOUSE = 20;          // EJERCICIO: intensidad (m/s²)
  const UMBRAL_REBOTE = 30;         // debajo de 30 px/s no rebota (evita temblores)

  // paleta -> colores [tinta, fondo]. Mismos valores RGB que StagePhysics::draw().
  // Para agregar una paleta: sumarla acá, en contrato.py y en el prompt de llmStage.
  const PALETAS = {
    mar:   ["rgb(144,215,255)", "rgb(17,40,73)"],
    sol:   ["rgb(255,211,126)", "rgb(66,37,42)"],
    noche: ["rgb(208,182,255)", "rgb(30,24,58)"],
    fuego: ["rgb(255,120,95)",  "rgb(78,14,18)"],  // el escenario rojo del enojo
  };

  let actual = null;  // Solo una escena corre a la vez.

  function crear(contenedor) {
    const spec = JSON.parse(contenedor.dataset.spec);
    const lienzo = document.createElement("canvas");
    lienzo.width = 720; lienzo.height = 520;   // mismo tamaño que stageArea en OF
    contenedor.replaceChildren(lienzo);
    const ctx = lienzo.getContext("2d");
    const W = lienzo.width, H = lienzo.height;

    // Plataformas fijas: centro, ancho y ángulo como en StagePhysics::setup().
    const plataformas = [[W * .28, H * .62, 12], [W * .72, H * .76, -12]].map(([x, y, grados]) => {
      const a = grados * Math.PI / 180, dx = Math.cos(a) * 90, dy = Math.sin(a) * 90;
      return { x1: x - dx, y1: y - dy, x2: x + dx, y2: y + dy };
    });

    // build(): un cuerpo por "cantidad", en grilla de 6 columnas.
    // Las palabras se repiten si hay más cuerpos que palabras.
    let cuerpos = [];
    function armar() {
      const circulos = spec.forma === "circulos";
      cuerpos = Array.from({ length: spec.cantidad }, (_, i) => ({
        x: 70 + (i % 6) * 115, y: 78 + Math.floor(i / 6) * 74,
        vx: ((i % 3) - 1) * 1.5 * PX, vy: -1 * PX,
        r: circulos ? 34 : 24,           // radio (en cajas, solo contra plataformas)
        hw: circulos ? 34 : 46, hh: circulos ? 34 : 24,
        etiqueta: spec.palabras[i % spec.palabras.length],
      }));
    }
    armar();

    // Mouse: mientras está apretado, atrae o repele (dato "interaccion").
    const mouse = { x: 0, y: 0, apretado: false };
    const posicion = (e) => {
      const caja = lienzo.getBoundingClientRect();
      mouse.x = (e.clientX - caja.left) * W / caja.width;
      mouse.y = (e.clientY - caja.top) * H / caja.height;
    };
    lienzo.addEventListener("pointerdown", (e) => { posicion(e); mouse.apretado = true; lienzo.setPointerCapture(e.pointerId); });
    lienzo.addEventListener("pointermove", posicion);
    lienzo.addEventListener("pointerup", () => { mouse.apretado = false; });
    lienzo.addEventListener("pointercancel", () => { mouse.apretado = false; });

    // Choque: separar y, si se acercan, invertir la velocidad normal según "rebote".
    function rebotar(a, b, nx, ny, solape) {
      const vn = (b ? b.vx - a.vx : -a.vx) * nx + (b ? b.vy - a.vy : -a.vy) * ny;
      if (b) { a.x -= nx * solape / 2; a.y -= ny * solape / 2; b.x += nx * solape / 2; b.y += ny * solape / 2; }
      else { a.x -= nx * solape; a.y -= ny * solape; }
      if (vn >= 0) return;  // ya se están alejando
      const e = -vn > UMBRAL_REBOTE ? spec.rebote : 0;
      const j = -(1 + e) * vn / (b ? 2 : 1);
      a.vx -= j * nx; a.vy -= j * ny;
      if (b) { b.vx += j * nx; b.vy += j * ny; }
    }

    function paso() {
      for (const c of cuerpos) {
        c.vy += spec.gravedad * PX * PASO;        // gravedad: negativa sube, 0 flota
        if (mouse.apretado) {
          const dx = mouse.x - c.x, dy = mouse.y - c.y, d = Math.hypot(dx, dy);
          if (d > 1 && d < RADIO_MOUSE) {
            const signo = spec.interaccion === "atraer" ? 1 : -1;
            const a = FUERZA_MOUSE * PX * (1 - d / RADIO_MOUSE) * signo;  // más cerca = más fuerza
            c.vx += dx / d * a * PASO; c.vy += dy / d * a * PASO;
          }
        }
        const freno = 1 / (1 + PASO * .18);         // "rozamiento con el aire"
        c.vx *= freno; c.vy *= freno;
        c.x += c.vx * PASO; c.y += c.vy * PASO;
      }
      for (let vuelta = 0; vuelta < 4; vuelta++) {  // varias vueltas = choques más estables
        for (const c of cuerpos) {
          // Paredes del escenario.
          if (c.x - c.hw < 0) rebotar(c, null, -1, 0, c.hw - c.x);
          if (c.x + c.hw > W) rebotar(c, null, 1, 0, c.x + c.hw - W);
          if (c.y - c.hh < 0) rebotar(c, null, 0, -1, c.hh - c.y);
          if (c.y + c.hh > H) rebotar(c, null, 0, 1, c.y + c.hh - H);
          // Plataformas: punto más cercano del segmento al centro del cuerpo.
          for (const p of plataformas) {
            const sx = p.x2 - p.x1, sy = p.y2 - p.y1;
            const t = Math.max(0, Math.min(1, ((c.x - p.x1) * sx + (c.y - p.y1) * sy) / (sx * sx + sy * sy)));
            const dx = p.x1 + sx * t - c.x, dy = p.y1 + sy * t - c.y, d = Math.hypot(dx, dy);
            if (d > 0 && d < c.r + 7) rebotar(c, null, dx / d, dy / d, c.r + 7 - d);
          }
        }
        // Cuerpo contra cuerpo: círculos por distancia, cajas por superposición.
        for (let i = 0; i < cuerpos.length; i++) for (let k = i + 1; k < cuerpos.length; k++) {
          const a = cuerpos[i], b = cuerpos[k], dx = b.x - a.x, dy = b.y - a.y;
          if (spec.forma === "circulos") {
            const d = Math.hypot(dx, dy);
            if (d > 0 && d < a.r + b.r) rebotar(a, b, dx / d, dy / d, a.r + b.r - d);
          } else {
            const sx = a.hw + b.hw - Math.abs(dx), sy = a.hh + b.hh - Math.abs(dy);
            if (sx > 0 && sy > 0) {
              if (sx < sy) rebotar(a, b, Math.sign(dx) || 1, 0, sx);
              else rebotar(a, b, 0, Math.sign(dy) || 1, sy);
            }
          }
        }
      }
    }

    // APARIENCIA: el lugar más fácil para empezar a modificar (como StagePhysics::draw()).
    function dibujar(t) {
      const [tinta, fondo] = PALETAS[spec.paleta];
      const fuego = spec.paleta === "fuego";
      ctx.clearRect(0, 0, W, H);
      ctx.fillStyle = fondo; ctx.beginPath(); ctx.roundRect(0, 0, W, H, 16); ctx.fill();
      // Fondo de escenografía. Con "fuego" late y tiembla: decisión nuestra, no del modelo.
      ctx.fillStyle = tinta; ctx.strokeStyle = tinta;
      ctx.globalAlpha = fuego ? (35 + 20 * Math.sin(t * 6)) / 255 : 25 / 255;
      ctx.beginPath();
      ctx.arc(W - 105, 96, spec.paleta === "noche" ? 50 : fuego ? 75 + 6 * Math.sin(t * 6) : 75, 0, Math.PI * 2);
      ctx.fill();
      ctx.lineWidth = 1;
      for (let i = 0; i < 7; i++) {
        const dy = fuego ? 2.5 * Math.sin(t * 23 + i * 1.7) : 0;
        ctx.beginPath(); ctx.moveTo(18, 80 + i * 64 + dy); ctx.lineTo(W - 18, 80 + i * 64 - dy); ctx.stroke();
      }
      ctx.globalAlpha = 100 / 255; ctx.lineWidth = 14; ctx.lineCap = "butt";
      for (const p of plataformas) { ctx.beginPath(); ctx.moveTo(p.x1, p.y1); ctx.lineTo(p.x2, p.y2); ctx.stroke(); }
      // Cuerpos con su palabra.
      ctx.font = "11px Menlo, Consolas, monospace"; ctx.textAlign = "center"; ctx.textBaseline = "middle";
      for (const c of cuerpos) {
        ctx.globalAlpha = 215 / 255; ctx.fillStyle = tinta; ctx.beginPath();
        if (spec.forma === "circulos") ctx.arc(c.x, c.y, 34, 0, Math.PI * 2);
        else ctx.roundRect(c.x - 46, c.y - 24, 92, 48, 5);
        ctx.fill();
        ctx.globalAlpha = 1; ctx.fillStyle = fondo; ctx.fillText(c.etiqueta, c.x, c.y + 1);
      }
      ctx.globalAlpha = 1; ctx.fillStyle = tinta; ctx.textAlign = "left";
      ctx.font = "13px Menlo, Consolas, monospace"; ctx.fillText(spec.titulo, 18, 25);
    }

    // Bucle: acumulador de tiempo + paso fijo, igual que StagePhysics::update().
    const escena = { vivo: true, armar };
    let anterior = performance.now(), acumulado = 0;
    function cuadro(ahora) {
      if (!escena.vivo || !lienzo.isConnected) return;  // Gradio reemplazó la escena
      acumulado += Math.min((ahora - anterior) / 1000, .1); anterior = ahora;
      while (acumulado >= PASO) { paso(); acumulado -= PASO; }
      dibujar(ahora / 1000);
      requestAnimationFrame(cuadro);
    }
    requestAnimationFrame(cuadro);
    return escena;
  }

  // Gradio cambia el HTML cuando llega una respuesta nueva. Observamos la página
  // y armamos la escena cada vez que aparece un <div class="escena"> nuevo.
  function revisar() {
    document.querySelectorAll(".escena[data-spec]").forEach((el) => {
      if (el.dataset.lista === el.dataset.spec) return;
      el.dataset.lista = el.dataset.spec;
      if (actual) actual.vivo = false;
      try { actual = crear(el); } catch (error) { el.textContent = "No se pudo armar la escena: " + error; }
    });
  }
  // REARMAR ESCENA: mismos datos, posiciones iniciales. No consulta al modelo.
  document.addEventListener("click", (e) => {
    if (e.target.closest && e.target.closest(".rearmar") && actual) actual.armar();
  });
  new MutationObserver(revisar).observe(document.documentElement, { childList: true, subtree: true });
  document.addEventListener("DOMContentLoaded", revisar);
})();
