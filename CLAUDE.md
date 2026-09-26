# AUGUR-5 "3340" — Sintetizador VST3/AU de modelado analógico

## 0. Identidad del producto

- **Nombre:** AUGUR-5, modelo **"3340"** (subtítulo que se muestra en la GUI).
- **Nombre de binario / PRODUCT_NAME:** `AUGUR-5` (sin comillas en nombres de archivo).
- **Empresa:** **TONAL LAB** (fabricante que muestran los DAWs; leyenda "A TONAL LAB INSTRUMENT" en el panel).
- **Códigos:** fabricante `Tnlb`, plugin `Au53`, bundle `com.tonallab.augur5` (AU: fabricante ≥1 mayúscula, plugin exactamente 1 mayúscula). **No cambiarlos nunca a partir de ahora**: los DAWs identifican el plugin y sus presets guardados por estos códigos.
- **Formatos objetivo:** VST3 (Windows + macOS), AU (macOS/Logic), Standalone. Futuro opcional: CLAP, AAX (Pro Tools, requiere SDK de Avid + firma PACE).
- **DAWs objetivo:** todos los principales — Ableton Live, Logic Pro, Cubase, FL Studio, Reaper, Bitwig, Studio One.

---

## 1. Rol y objetivo

Actúa como ingeniero senior de DSP y desarrollo de plugins de audio, con experiencia en modelado de circuitos analógicos (virtual analog), C++ en tiempo real y JUCE.

**Meta del proyecto:** construir un sintetizador polifónico en formato **VST3** (y AU en macOS), que funcione con **MIDI** en **Windows y macOS** y que sea instalable con un instalador en cada sistema. Su sonido tiene que acercarse lo más posible a un polysynth analógico clásico de 5 voces (2 VCO por voz, mixer, filtro de 4 polos, 2 envolventes, poly-mod).

**Referencias a superar:** u-he Repro-5 y Arturia Prophet V. El criterio de éxito es **no poder distinguirlo del hardware en una prueba a ciegas A/B**, no "que suene bonito".

**Prioridad #1:** el VCO. Primero construimos el oscilador más fiel posible y lo validamos con medición. Después vienen los demás componentes y las modulaciones.

**Nota legal:** el producto tiene **nombre y diseño propios** (AUGUR-5). No usar las marcas "Prophet" ni "Sequential" en el nombre, la interfaz ni los presets. Internamente se puede referir a los circuitos por sus chips (CEM3340, SSM2030, CEM3320, SSM2040, CEM3310).

---

## 2. Stack técnico (obligatorio)

- **Lenguaje:** C++20.
- **Framework:** JUCE 8 (VST3, AU y Standalone). Verificar la licencia de JUCE y del VST3 SDK antes de distribuir.
- **Build:** CMake (`juce_add_plugin`) con `CMakePresets.json`; nada de Projucer.
- **Dependencias:** git submodules en `external/` (JUCE, Catch2), fijados a tags.
- **Editor:** VS Code con CMake Tools y C/C++. Depuración: `cppvsdbg` en Windows, CodeLLDB en macOS.
  - Windows: MSVC (Visual Studio Build Tools).
  - macOS: Xcode Command Line Tools, binario universal (arm64 + x86_64).
- **Tests:** Catch2 v3 para el DSP y **pluginval** (nivel estricto 10) para validar el plugin.
- **Análisis:** Python 3 con numpy, scipy, matplotlib y soundfile, en `tools/` (venv en `tools/.venv`).
- **CI:** GitHub Actions que compile y pruebe en Windows y macOS en cada push.
- **Instaladores:**
  - Windows: Inno Setup → `C:\Program Files\Common Files\VST3`.
  - macOS: `pkgbuild` + `productbuild`, con firma y notarización (documentadas aunque aún no haya certificado) → `/Library/Audio/Plug-Ins/VST3` y `/Library/Audio/Plug-Ins/Components`.

---

## 3. Arquitectura del repositorio

```
/CMakeLists.txt  /CMakePresets.json  /CLAUDE.md
/external/                ← submodules (JUCE, Catch2)
/dsp/                     ← núcleo DSP en C++ PURO, sin dependencias de JUCE
    Oscillator/ Filter/ Envelope/ VCA/ Mixer/ Modulation/ Voice/ Analog/ Util/
/plugin/                  ← capa JUCE: AudioProcessor, parámetros, MIDI, editor (GUI)
/tests/                   ← Catch2: pruebas unitarias y "golden tests" del DSP
/tools/                   ← Python: renderizado offline, análisis espectral, comparación
/tools/render_cli/        ← CLI que usa /dsp para renderizar WAVs sin DAW
/reference/               ← grabaciones de referencia (fuera de git si pesan)
/installer/win/ /installer/mac/
/.github/workflows/
/docs/                    ← decisiones de diseño, mediciones y resultados
```

**Regla clave:** todo el DSP vive en `/dsp` y se puede probar y renderizar a WAV sin abrir un DAW. El plugin es solo una capa delgada encima.

---

## 4. Reglas de calidad (no negociables)

1. **Seguro en tiempo real:** en el hilo de audio no puede haber `new`/`malloc`, locks, llamadas al sistema, logging ni excepciones. Todo se reserva en `prepareToPlay`.
2. **Independiente del sample rate:** tiene que sonar igual a 44.1, 48, 88.2, 96 y 192 kHz. Los coeficientes se calculan a partir del sample rate.
3. **Anti-aliasing:** aliasing por debajo de **−100 dB** dentro de 20 Hz–20 kHz, medido en todo el rango de teclado y también con sync y FM.
4. **Precisión:** acumuladores de fase y estados sensibles en `double`. Protección contra denormales (FTZ/DAZ y `juce::ScopedNoDenormals`).
5. **Suavizado de parámetros:** ningún cambio de parámetro puede producir clicks ni zipper noise.
6. **CPU:** menos del 5% de un núcleo moderno con 5 voces a 48 kHz (medido con benchmarks).
7. **Sin magia:** cada modelo analógico se documenta en `/docs` con su justificación (circuito, ecuación y fuente).
8. **Cada fase termina con:** tests en verde, pluginval en verde, mediciones guardadas en `/docs` y un commit claro.
9. **Compatibilidad con hosts:** IDs de parámetros estables (nunca renombrar ni reordenar), estado versionado, funcionamiento correcto con cualquier tamaño de bloque (incluido 1 y bloques variables), sin suponer que `prepareToPlay` se llama una sola vez.

---

## 5. Fases del proyecto

### Fase 0 — Esqueleto que ya carga en un DAW
- Proyecto CMake + JUCE que compila VST3, AU y Standalone en Windows y macOS desde VS Code.
- `tasks.json` y `launch.json` de VS Code para compilar, depurar el Standalone y correr los tests.
- El plugin recibe MIDI y toca una senoidal monofónica (solo para probar la tubería).
- CI en GitHub Actions + pluginval.
- **Aceptación:** se carga en un DAW y suena con un teclado MIDI.

### Fase 1 — EL VCO (la fase más importante)
Modela el núcleo tipo **CEM3340** (Rev 3) con una variante **SSM2030** (Rev 1/2) seleccionable.

**Modelo del núcleo:**
- Núcleo de rampa: capacitor cargado mediante un convertidor exponencial, con reset por comparador.
- **Curvatura de la rampa** configurable.
- **Tiempo de reset finito**: la muesca redondeada al final del ciclo, que escala con la frecuencia como en el chip real.
- **Triángulo** derivado de la rampa con un waveshaper de asimetría leve.
- **Pulso** por comparador sobre la rampa, con PWM de 5% a 95% y flancos de pendiente finita.
- Offset de DC y pequeñas diferencias de nivel entre formas de onda.

**Anti-aliasing:**
- Implementar y comparar: PolyBLEP (línea base), **minBLEP/BLAMP con tabla** y **ADAA** (con oversampling 2x si hace falta).
- Elegir según medición (aliasing, CPU, fidelidad) y documentar.
- **Hard sync** del OSC A por el OSC B con corrección BLEP en el instante exacto del reset, dentro del sample.

**Comportamiento analógico:**
- **Fase libre** por defecto, con opción de "reset suave".
- **Drift de afinación:** ruido 1/f + caminata aleatoria lenta, de unos pocos cents, con intensidad. Nada de LFO senoidal.
- **Error de tracking** en agudos y escalado de afinación por voz.
- **Micro-jitter** por ciclo, sutil y medido.
- **Variación por voz:** "huella" por voz (afinación, curvatura, ancho de pulso, DC), con semilla reproducible.

**Herramientas:** `render_cli` (notas, barridos, sync a WAV) y `tools/analyze_vco.py` (espectro, aliasing, forma de onda promediada, histograma de drift, comparación contra referencia).

**Aceptación:** `/docs/vco_report.md` con gráficas, aliasing < −100 dB, CPU medido, comparación contra saw ideal y PolyBLEP básico.

### Fase 2 — Mixer, ruido y saturación
- Mixer OSC A + OSC B + ruido (rosa/blanco) con **saturación suave** a la entrada del filtro.
- Fuga sutil entre osciladores (bleed), con parámetro.

### Fase 3 — Filtro de 4 polos (24 dB/oct)
- Modelos **CEM3320** (Rev 3) y **SSM2040** (Rev 1/2).
- **ZDF / TPT** con no linealidades por etapa, resolviendo el lazo con Newton-Raphson o equivalente estable.
- Resonancia hasta autooscilación con caída de graves.
- Keyboard tracking (off / medio / completo), cutoff modulable a audio rate.
- Oversampling solo donde lo exija la medición.

### Fase 4 — Envolventes y VCA
- Dos ADSR tipo **CEM3310**: curvas RC exponenciales reales (ataque con overshoot cortado).
- Velocidad opcional hacia filtro y VCA.
- VCA con algo de color y sin clicks en ataques rápidos.

### Fase 5 — Voces y MIDI completo
- 5 voces ampliable a 8/10/16; modos rotate, reset, low/high/last note priority.
- **Unison** con detune y spread; glide con curva analógica.
- MIDI: note on/off, velocity, pitch bend, mod wheel, aftertouch, sustain (CC64), MIDI learn, All Notes Off/panic.
- MPE opcional posterior.

### Fase 6 — Modulaciones
- **Poly-Mod:** fuentes Filter Env y OSC B; destinos freq A, PW A, cutoff. FM exponencial a audio rate sin aliasing audible.
- **Wheel-Mod:** LFO (tri, saw, square) y ruido hacia freq A/B, PW A/B y filtro.
- LFO con fase libre y ligera variación analógica.
- Pequeña matriz de modulación adicional.

### Fase 7 — "Slop" analógico global
- Control único de **"edad/calibración"** que escala todas las imperfecciones.
- Semilla reproducible por preset.

### Fase 8 — Interfaz (GUI)
- Editor escalable (100–200%), HiDPI, distribución clásica de paneles, diseño propio.
- Arrastre fino (Shift), doble click para default, tooltips.

### Fase 9 — Presets y estado
- `getStateInformation` con versión de formato de preset.
- Banco inicial y navegador simple.

### Fase 10 — Validación final y distribución
- Pruebas a ciegas A/B con script en `tools/`.
- pluginval estricto; pruebas en Reaper, Ableton Live, FL Studio, Cubase, Logic (AU).
- Instaladores de Windows y macOS + README de instalación.

---

## 6. Forma de trabajar

1. **Una fase a la vez.** Antes de escribir código, mostrar un plan corto (archivos, clases, pruebas) y esperar aprobación.
2. Explicar en español y de forma breve cada decisión de DSP importante. Código y comentarios en inglés.
3. Después de cada cambio: compilar, correr tests y decir exactamente qué comando ejecutar en VS Code.
4. Si algo no se puede medir, no está terminado. **Las mediciones mandan sobre las opiniones.**
5. Con dos opciones razonables: pros, contras y recomendación.
6. Mantener `/docs/decisions.md` actualizado.
7. Nunca romper la seguridad en tiempo real para "arreglar rápido".
