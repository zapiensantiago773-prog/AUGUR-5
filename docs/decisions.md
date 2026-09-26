# Decisiones de arquitectura — AUGUR-5 "3340"

Formato: contexto → decisión → consecuencias. Las más recientes van al final.

## D-001 · Dependencias como git submodules (2026-09-25)
JUCE 8.0.15 y Catch2 v3.16.0 en `external/`, fijados a tag. Alternativa descartada: `FetchContent` (descarga JUCE en cada build limpio y no funciona sin red). Actualizar = `git -C external/JUCE checkout <tag>` + commit.

## D-002 · CMake Presets + Ninja
`win-debug/win-release` (MSVC x64) y `mac-debug/mac-release` (universal arm64+x86_64). Ninja da builds rápidos y `compile_commands.json`. En Windows el preset usa `architecture.strategy = external`: CMake Tools carga el entorno de MSVC por su cuenta.

## D-003 · Runtime de MSVC estático (`/MT`)
El plugin se tiene que cargar en cualquier PC, aunque no tenga el VC++ Redistributable instalado (la computadora con Ableton del usuario, clientes). Costo: binario un poco más grande. Se aplica a todo el proyecto para evitar mezclar runtimes.

## D-004 · Identidad del plugin (inmutable)
`PRODUCT_NAME "AUGUR-5"`, empresa **TONAL LAB**, fabricante `Tnlb`, plugin `Au53`, bundle `com.tonallab.augur5` (cambiado el 2026-09-26, antes del primer uso en un DAW; antes era `Augr` / `com.auguraudio.augur5`). Los DAWs guardan proyectos y presets con estos valores; cambiarlos después del release rompe las sesiones de los usuarios. "3340" es el nombre del modelo y solo aparece en la GUI (las comillas no son válidas en nombres de archivo).

## D-005 · Formatos y cobertura de DAWs
- VST3: Ableton, Cubase/Nuendo, FL Studio, Reaper, Bitwig y Studio One (Windows y macOS).
- AU: Logic y GarageBand (y los demás hosts de macOS que lo prefieran).
- Standalone: para probar sin DAW.
- Pendiente de evaluar: **CLAP** (clap-juce-extensions; Bitwig, Reaper y FL lo aceptan) y **AAX** (Pro Tools; requiere SDK de Avid, cuenta de desarrollador y firma PACE/iLok). Windows ARM64 también queda para más adelante.

## D-006 · MIDI sample-accurate sin asignar memoria
El bloque se renderiza en tramos entre eventos MIDI. Los bytes MIDI se leen directamente de `MidiBufferIterator` sin construir `juce::MidiMessage`, así que no hay allocations en el hilo de audio. Se cumple con cualquier tamaño de bloque, incluidos bloques más grandes que el anunciado en `prepareToPlay`.

## D-007 · Depuración
Windows: `cppvsdbg` (lee los PDB de MSVC de forma nativa). macOS: CodeLLDB.

## D-008 · IDs de parámetros tomados del diseño de GUI
`plugin/Parameters.h` es la fuente única de IDs (vienen del diseño de GUI). Cada parámetro se registra en el APVTS en la fase donde su DSP existe, para que ningún control esté "muerto". Todos usan `ParameterID { id, 1 }`; la versión solo sube si cambia el significado. Desde 2026-09-25 están registrados todos, porque el motor completo ya existe.

## D-009 · Estado versionado
`getStateInformation` escribe `stateVersion` (hoy 1) en el ValueTree. `setStateInformation` ignora XML de otro tipo; las migraciones se agregan cuando cambie el formato.

## D-010 · Arquitectura del motor (2026-09-25)
- Todas las voces corren a una tasa interna ≥ 88.2 kHz: 2x cuando el host está a 44.1 o 48 kHz y 1x en 88.2, 96 y 192 kHz. La suma estéreo baja a la tasa del host con **un solo** decimador FIR halfband (127 taps, Kaiser β=10, ~100 dB). Así el filtro no lineal, el drive y el FM a audio rate no generan aliasing audible, y la decimación se paga una vez y no por voz.
- Los efectos (chorus BBD, delay de cinta y reverb FDN de 8 líneas) corren a la tasa del host.
- El control se actualiza cada 32 samples de host según un contador global. Las señales de control (parámetros suavizados y LFO) se calculan para el chunk completo, así que el render es **bit-idéntico** sin importar cómo corte el DAW los bloques. Un test lo verifica.
- Latencia reportada al host: (63 + 8) / 2 = 35 samples a 44.1/48 kHz, y 8 samples a 1x.

## D-011 · VCO: BLEP/BLAMP de tabla con eventos sub-sample
- Núcleo de rampa con curvatura cuadrática, **tiempo muerto de descarga** (CEM3340 ≈ 1.6 µs, SSM2030 ≈ 3 µs) y trim de HF. Triángulo por waveshaper y pulso por comparador, ambos derivados de la rampa.
- Eventos con posición exacta dentro del sample: reset, flanco del pulso, ápice del triángulo, fin del tiempo muerto y hard sync. Se corrigen con residuos BLEP/BLAMP (sinc de 16 taps, Kaiser β=11.2, 512 fases).
- **Bug encontrado por medición:** la interpolación lineal cruzaba el salto del residuo en t = 0 (−70 dB de aliasing y picos de 1.9). Se corrigió guardando la tabla por segmentos de tap.
- **Medido** (test `[aliasing]`, peor componente no armónica en 20 Hz–20 kHz, a la tasa interna):

| f0 | saw 88.2k | pulse 88.2k | saw 96k | pulse 96k |
|---|---|---|---|---|
| 1001.7 Hz | −150.6 dB | −152.7 dB | −152.3 dB | −153.3 dB |
| 3001.7 Hz | −136.6 dB | −140.2 dB | −138.0 dB | −141.9 dB |
| 7012.3 Hz | −117.9 dB | −144.1 dB | −119.7 dB | −139.7 dB |

  Objetivo (< −100 dB) cumplido. Falta: medir con FM y PWM a audio rate, y el sistema completo con el decimador (tools/analyze_vco.py).

## D-012 · Filtro: ZDF semi-implícito
Cuatro integradores `tanh(in − out)` con lazo de realimentación resuelto exactamente sobre las ganancias secantes del sample anterior (método Mystran). La versión de 2 iteraciones costaba 3.3 veces más CPU sin diferencia en los tests. La autooscilación empieza en RESONANCE ≈ 0.8 (CEM) y ≈ 0.83 (SSM), con 0.27 y 0.20 rms. Las constantes de los modelos son **provisionales** hasta calibrar contra grabaciones.

## D-013 · GUI nativa en JUCE, calcada del mockup
El paquete de diseño se declara "reference mockup". Replicamos sus valores (colores, tamaños, sombras y posiciones del artboard de 1536×1024) en componentes JUCE nativos, sin incrustar un WebView: es más estable en todos los DAWs, no depende de WebView2 y pesa menos. El lienzo se escala completo (60–150%, y redimensionable con proporción fija). Fuentes OFL incrustadas: Jost, Michroma y JetBrains Mono (licencias en `plugin/resources/fonts`). Desviación consciente: la barra AMOUNT de la matriz se rellena **desde el centro**, porque es bipolar.

## D-014 · Parámetros añadidos fuera del panel
`osc_model` (VCO REV 3 CEM3340 / REV 1 SSM2030) y `pb_range` (rango del pitch bend). Están en SETTINGS para no alterar el diseño.

## D-015 · CPU — pendiente (medido, fuera de objetivo)
`augur_render --bench`: 5 voces a 48 kHz = **24.9%** de un núcleo (objetivo < 5%). Costo por muestra interna: VCO ≈ 56–68 ns, filtro ≈ 115 ns. Plan: procesar 4 voces por instrucción SIMD (SSE/NEON) en VCO, filtro y envolventes; calcular el pitch por chunk cuando no hay modulación a audio rate; y medir otra vez.

## D-016 · VCO reconstruido a partir del hardware (2026-09-25)
Basado en [prophet5_vco_analysis.md](prophet5_vco_analysis.md) (manual de servicio Rev 3 + datasheet CEM3340).
- **CEM3340 = núcleo triangular** (`Cem3340Vco`): capacitor entre 0 y Vcc/3 con carga y descarga asimétricas (tolerancia de simetría de la unidad), retardo del comparador (el triángulo sobrepasa sus umbrales, así que los agudos se desafinan hacia abajo y hay escalón a mitad de la sierra), conversor tri→saw con errores de ganancia y offset, pulso por comparador con flanco de bajada más lento, y sync del Prophet por descarga del capacitor (PNP, retención de ~4.4 µs).
- **SSM2030 = núcleo de rampa** (`Ssm2030Vco`): el modelo anterior, ahora con la misma interfaz.
- **Ley exponencial por unidad** (`ExpoConverter`): error de afinación, error de escala y caída por resistencia de emisor.
- **Capa digital del Prophet:**
  - autotune simulado (conteo de periodo a 2.5 MHz, SAR sobre el DAC de 14 bits, medición en C3–C9 y extrapolación a C0–C2, interpolación por nota);
  - CV cuantizada a 1/128 de semitono;
  - droop del S/H (0.5 mV cada 6 ms);
  - ruido de CV en la base del convertidor exponencial.
- **Mixer CA3280 diferencial** (`ota::mix`): saw en (+), pulso en (−), triángulo level-shifted ±5 V, con los valores de resistencia del esquema. Acople AC con su DC exacto más un residuo aprendido.
- **Poly-mod OSC B** a través de su propio par diferencial, conservando su DC como en el hardware.
- **Acople AC entre VCF y VCA** (C4165), con carga que persiste entre notas: notas repetidas sin golpe de DC (< −60 dBFS).
- **OSC B LO FREQ corregido a −7.5 octavas**, con el rango de INIT FREQ duplicado.
- **Opción "Vintage 7-bit knobs"** (SETTINGS): digitalización de las perillas de panel a 128 pasos con la histéresis de dos pasos del software Rev 3.
- **PWM a audio rate limitado en banda:** umbral de PW interpolado con Catmull-Rom (un sample tarde) y cruces resueltos con Newton protegido.

**Mediciones** (tests `[aliasing]`, peor componente no armónica en 20 Hz–20 kHz, a la tasa interna):

| Caso | Resultado |
|---|---|
| CEM3340 saw / tri / pulso, 1–7 kHz, 88.2 y 96 kHz | −138 a −164 dB |
| PWM a audio rate (±35 % a 377 Hz) | −134.9 dB |
| Sync del Prophet | −142.5 dB |
| FM exponencial a audio rate (±3 semitonos a 377 Hz) | −102.3 dB (margen chico; el siguiente paso es interpolar el pitch dentro del sample) |
| SSM2030 | −100.8 a −152 dB |

- **Afinación tocada** (motor completo, edad 0): dentro de ±2 cents en C2, C4 y C6 con ambos modelos.
- **Sin autotune:** la unidad típica cae −2.2 cents en C7 y −8.5 en C9, emergente del comparador y la resistencia de emisor.

**CPU:** 5 voces a 48 kHz = **11.7–12.2 %** de un núcleo (antes 25 %). Objetivo < 5 %: sigue pendiente el SIMD.

**Pendiente conocido:** la primera nota después de cargar el plugin puede llevar un escalón de DC de hasta −45 dBFS por debajo de 25 Hz. Se debe a la saturación del filtro con la carga del C4165 aún no establecida. Desaparece desde la segunda nota.


## D-017 · Pitch dentro del sample, golpe de DC, asignación de voces y CPU (2026-09-26)
- **FM a audio rate:** la tasa de carga del VCO (CEM3340) y el incremento de fase (SSM2030) varían linealmente dentro de cada sample. Los tiempos de evento se resuelven con la ecuación cuadrática exacta. Aliasing de FM: **−102 → −141 dB**.
- **Golpe de la primera nota:** los capacitores de acople (DC del mixer y C4165 entre VCF y VCA) se "cargan" al preparar el plugin, al cargar una sesión y al cambiar de preset. Una voz se ejecuta en silencio y se mide la media real con ventana de Hann, no con un filtro de un polo, que seguía el rizado. Se copia la carga y el estado del filtro a las demás voces, y una voz que despierta toma el estado de una activa. Primera nota: **−40 → −61 dBFS** en el peor caso (filtro a 25 Hz, nivel máximo).
- **Bug corregido:** las notas del primer bloque, o justo después de cambiar VOICES/UNISON, se asignaban con parámetros viejos y luego se soltaban. La asignación usa ahora siempre los parámetros vigentes, y bajar VOICES solo suelta las voces sobrantes.
- **CPU** (5 voces a 48 kHz, 2x): **12.1 % → 8.5 %**; 8 voces 17.3 % → 12.2 %; reposo 1.8 % → 0.8 %.
  - Las voces en silencio solo avanzan el drift.
  - Caché de constantes por chunk.
  - OSC B se omite si es inaudible y nada lo usa.
  - Camino rápido en línea en los VCOs.
  - **La mitad posterior de la voz** (drive, exp2 del cutoff, filtro, acople, VCA, paneo) **corre con SIMD, 4 voces por instrucción** (SSE2/NEON), idéntica al filtro escalar (test: diferencia 0).
  - Medido sin mejora y descartado: `/fp:fast` e intercalar voces muestra a muestra.
  - Lo que falta para llegar a < 5 % es la mitad frontal: los VCOs, 2.6 %, orientados a eventos.

## D-018 · TONAL LAB y librería de fábrica (2026-09-26)
- Empresa **TONAL LAB**: fabricante en los DAWs, código `Tnlb`, bundle `com.tonallab.augur5`, leyenda "A TONAL LAB INSTRUMENT" en el panel y presets de usuario en `TONAL LAB/AUGUR-5/Presets`.
- **Librería base, 66 sonidos + Init**, enfocada a progressive y melodic techno:

| Categoría | Sonidos |
|---|---|
| Bass | 8 |
| Lead | 8 |
| Pad | 8 |
| Pluck | 9 |
| Keys | 7 |
| Stab | 6 |
| Arp | 6 |
| Drums | 9 |
| Atmos & FX | 7 |

  Los nombres son propios, sin nombres de artistas. Los delays están a ~123 BPM y la mod wheel abre el filtro en la mayoría.
- **Plucks** (prioridad): envolvente RC de filtro rápida y profunda, velocity→brillo, keytrack y un blip de poly-mod de la envolvente al pitch de OSC A en el ataque.
- **Drums** sintetizados con el propio motor: barrido de pitch vía matriz (FILTER ENV → OSC FREQ), ruido y dos pulsos inarmónicos.
- **Auditoría** (`augur_preset_audit`, también en la CI): cada preset pasa por el procesador real con notas de su categoría, con un procesador nuevo por preset para que sea determinista. Falla si hay silencio, valores no finitos o saturación.
- **Nivelación** (`tools/level_presets.py`): volumen de corto plazo (RMS máximo en 50 ms) a −18 dB para lo melódico; drums −16 (bombo/tom), −20 (snare) y −25 (clap, rim, hats, cowbell); pico ≤ −3 dBFS.

## D-019 · Arpegiador (2026-09-26)
- Modos UP, DOWN, UP-DOWN (sin repetir las notas de giro), RANDOM y ORDER (orden tocado); 1–4 octavas.
- Rate 1/4 … 1/32 con puntillos y tresillos; gate de 2–100 % del paso; swing de 0–50 % en los pasos impares; latch (un acorde nuevo reemplaza al retenido).
- **Reloj en beats derivado del contador absoluto de muestras.** Cada evento cae en una muestra exacta y el resultado es bit-idéntico sin importar cómo el host corte los bloques (test).
  - Con transporte en marcha sigue la rejilla de la canción: espera la siguiente línea y se recoloca en loops o saltos.
  - Parado, arranca en la primera tecla con el BPM del host.
- Las notas del arpegio entran por la misma asignación de voces que el teclado (poly, mono, unison y legato siguen valiendo).

## D-020 · Modelos de filtro, pendiente/modo y HPF (2026-09-26)
- El filtro de voz ofrece cinco modelos, todos ZDF semi-implícitos y en SIMD de 4 voces:

| Modelo | Topología | Carácter |
|---|---|---|
| REV 3 (CEM3320) | Escalera | Original |
| REV 1 (SSM2040) | Escalera | Original |
| CASCADE | Escalera OTA 4 polos | Etapas más limpias, entrada más saturada, resonancia suave |
| MULTIMODE | SVF con integradores saturables | 12 dB, o 24 dB con dos secciones Butterworth, resonancia en la segunda |
| BITE | Sallen-Key 2 polos, saturador dentro del lazo de realimentación positiva | Estilo Korg35: agresivo y chillón |

  - BITE solo tiene LP/HP; el HP lleva un polo extra a la salida para mantener 12 dB/oct.
- **Pendiente 24/12 dB y modo LP/BP/HP** en la escalera, mezclando la entrada y las cuatro salidas de etapa (enfoque Xpander). El BP está normalizado a ganancia 1 en el corte.
- **HPF post-filtro** de 2 polos (Q 0.707) de 10 a 2000 Hz, apagado a 10 Hz.
- Tests:
  - estabilidad con cutoff aleatorio a resonancia 1.1 en los 5 × 3 × 2 casos;
  - respuesta LP/HP/BP y pendiente medida (12 dB ≈ 12, 24 dB ≈ 24);
  - auto-oscilación de BITE;
  - HPF.
- CPU (5 voces, 48 kHz): **8.3–9.0 %** en todos los modelos, sin cambio respecto a D-017.
- Constantes provisionales, a calibrar con grabaciones.

## D-021 · Prueba de uso prolongado (2026-09-26)
- `augur_preset_audit --soak "<preset>" <segundos>` toca acordes (1 s sí, 1 s no) con el procesador real y reporta cada 10 s:
  - nivel y agudos (primera diferencia) en los silencios;
  - nivel y brillo de las notas;
  - el bloque más lento.
- Con 69 presets × 60 s y "Warm Horizon" × 180 s no hay nada que se acumule con el uso: los silencios y el brillo quedan iguales del inicio al final.
- Los picos de tiempo por bloque son aleatorios entre corridas (planificador del SO), no del motor.

## D-022 · Modulación y osciladores extra (2026-09-26)
- **MOD ENV**: tercera envolvente RC por voz (ADSR), fuente de la matriz.
- **LFO 2 por voz**: seno, triángulo, sierra ↑, sierra ↓, cuadrada, S&H y aleatorio suave.
  - Con RETRIG, cada nota arranca en fase 0 (poly). Sin él, la voz arranca en la fase del reloj libre del motor, calculada en la posición exacta dentro del chunk, así que es independiente del corte de bloques.
  - Sync a tempo con las divisiones del LFO 1.
  - Su velocidad es destino de la matriz, por voz.
- **Matriz de 8 slots** (mm5–mm8 nuevos). A las listas solo se les añaden entradas al final, para que los presets guardados sigan valiendo.
  - Fuentes nuevas: MOD ENV, LFO 2, KEYTRACK, NOTE RANDOM.
  - Destinos nuevos: FM AMOUNT, RING, SUB, DRIVE, OSC 1/2 LEVEL, NOISE LEVEL, LFO 2 RATE.
- **FM lineal (cross-mod) OSC B → OSC A**: la frecuencia se mueve alrededor de la portadora, hasta ±3× a fondo, así que la afinación se mantiene mientras crecen las bandas laterales.
  - Pasa por el pitch del VCO muestra a muestra, con el mismo solver de eventos: tasa lineal dentro de la muestra, D-012.
  - `fastmath::log2` nuevo, con error < 2e-6.
- **Ring mod**: OSC A × OSC B, las dos señales AC del mezclador.
  - Sin banda limitada propia: el producto de dos señales limitadas a Nyquist puede plegar sumas por encima. Con 2x de sobremuestreo y armónicos en 1/n queda bajo, pero no se garantiza −100 dB. Es un compromiso aceptado, igual que en el hardware digital de referencia.
- **Sub oscilador**: flip-flop disparado por los resets de OSC A (−1 o −2 octavas). Los escalones van con BLEP en la misma línea de tiempo y latencia que las salidas del VCO: sin aliasing de la cuadrada y alineado en fase con la sierra.
- Tests:
  - frecuencia del sub (−1/−2 oct, ±0.3 %);
  - FM y ring finitos y audibles;
  - mod env → pitch y LFO 2 → nivel;
  - **bit-idéntico con todo activado y bloques aleatorios**.
- CPU: 5 voces 8.6 % → **9.3 %**.

## D-023 · Efectos: FUZZ, phaser, modos de chorus, delay sync/ping-pong, plate (2026-09-26)
- **Orden de la cadena**: FUZZ (sobre el bus sobremuestreado, antes del decimador) → decimador → PHASER → CHORUS → DELAY → REVERB (HALL o PLATE). Cada efecto entra y sale con fundido y solo cuesta CPU mientras suena.
- **FUZZ**: topología clásica del pedal "sustainer" de 4 transistores:
  - dos etapas de recorte con diodos en la realimentación, cada una con HP de acople y LP del condensador de realimentación;
  - tone stack pasivo con mezcla LP 410 Hz / HP 1850 Hz, que da el hueco de medios del sonido "melancólico";
  - etapa de recuperación.
  - Recortador algebraico x/√(1+x²), con curva de diodo y ADAA de primer orden: la antiderivada es √(1+x²), así que no necesita exp ni log.
  - **8x de sobremuestreo interno** (768 kHz a 48 kHz), con half-bands de 27/19/19 taps dimensionados por etapa (`Util/HalfbandFir.h`, plantilla float/SIMD).
  - **Aliasing medido a sustain máximo: −120 a −123 dB**. Con 2x y tanh+ADAA daba −45 a −65 dB, y con 4x −72 a −86 dB.
  - L/R en un solo registro SIMD. CPU con el fuzz activo: +3.5 % (antes de optimizar, +6.4 %).
- **PHASER**: 6 all-pass de primer orden barridos exponencialmente (hasta 120 Hz–4 kHz), con realimentación saturada y el canal R desfasado 90° en el LFO.
- **CHORUS**: modos FREE (perillas), I (0.513 Hz), II (0.863 Hz) e I+II (9.75 Hz, 3.3–3.7 ms). Son los valores del ensemble BBD clásico de dos botones: 1.66–5.35 ms en los modos I y II.
- **DELAY**:
  - sync a tempo con 12 divisiones, de 1/32 a 1 compás, con puntillos y tresillos (1/8D por defecto);
  - ping-pong: la entrada va a L y cada repetición cruza de lado (test).
- **PLATE**: tanque de figura 8 de Dattorro (JAES 1997) escalado a cualquier frecuencia de muestreo y por SIZE. El decay se aplica dos veces por mitad del tanque y la ganancia sale del RT60 pedido: **RT60 medido con integración de Schroeder dentro de ±30 %** (test a 1 y 3 s).
- Test de determinismo con todos los efectos activos y bloques aleatorios: bit-idéntico.

## D-024 · Modos de calidad ECO / GREAT / DIVINE y render offline (2026-09-26)
- **ECO** = 1x, **GREAT** = frecuencia interna ≥ 88.2 kHz (2x a 44.1/48 kHz, como hasta ahora) y **DIVINE** = ≥ 176.4 kHz (4x a 44.1/48 kHz). El 4x usa una etapa half-band de 27 taps (4x→2x) antes del decimador de 127 taps.
- **OFFLINE QUALITY** (SAME / DIVINE, por defecto DIVINE): al exportar desde el DAW (`isNonRealtime`) el motor pasa a DIVINE.
- El cambio reconstruye las voces:
  - en tiempo real se hace fuera del hilo de audio, con el procesamiento suspendido (`AsyncUpdater`);
  - en offline se hace directamente, porque no hay deadline.
  - Luego se precargan los condensadores de acople (warm-up) y se reporta la latencia nueva al host: kernels BLEP + decimadores, calculada por modo.
- `getTailLengthSeconds` = 12 s, para que los renders conserven las colas de delay y reverb.
- CPU a 48 kHz, 5 voces:

| Modo | CPU |
|---|---|
| ECO | 4.7 % |
| GREAT | 9.3 % |
| DIVINE | 17.2 % |

- Tests:
  - tabla de factores por frecuencia de muestreo;
  - bit-idéntico con bloques aleatorios en 1x/2x/4x;
  - repliegue del filtro saturado (DRIVE 1, resonancia 0.6, nota de 1568 Hz):

| Modo | Repliegue |
|---|---|
| ECO | −40 dB |
| GREAT | −47 dB |
| DIVINE | −51 dB |

- **Hallazgo:** con DRIVE a fondo, la saturación de la entrada del filtro y de sus etapas es ahora el límite de aliasing del motor (los VCOs están en −138 dB). Pendiente: ADAA en el DRIVE, que requiere compensar la caída de agudos que introduce a 2x.

## D-025 · Corrección: silbido / "crush" agudo con resonancia y keytrack (2026-09-26)
- **Síntoma reportado:** después de un rato de uso se oye un silbido o "crush" muy agudo, "como viejo".
- **Causa** (medida con `[.diag]`):
  - Con KEYTRACK y notas agudas, el corte se clavaba en 0.45 × la frecuencia interna (43 kHz en GREAT).
  - Ahí la resonancia sonaba ultrasónica y se intermodulaba con los armónicos de la nota, generando tonos no armónicos audibles. Con esta nota de prueba: **6.6 kHz a −9 dB** en GREAT. En ECO lo más fuerte de la salida era un silbido de ~19.9 kHz.
- **Corrección:** el corte llega como máximo a **20 kHz**, el rango del instrumento, y nunca pasa de 0.35 fs (16.8 kHz en ECO a 48 kHz).
  - Mismo parche: el peor tono no armónico bajo 15 kHz baja de **−9 a −37 dB** en GREAT y a **−61 dB** en DIVINE.
  - Test de regresión incluido.
- **Otras causas descartadas** con la prueba de uso prolongado (D-021): no hay acumulación de ruido ni deriva de afinación, y los denormales ya se eliminan.

## D-026 · Modos de voz DUO y trims por voz (2026-09-26)
- **VOICE MODE**:
  - POLY: como hasta ahora.
  - DUO: cada nota toca dos voces, desafinadas en sentidos opuestos (hasta ±25 cents con VOICE DETUNE) y abiertas en estéreo. La polifonía se reduce a VOICES/2.
  - Cambiar de modo suelta las notas, igual que al pasar de poly a mono.
- **TRIMS** (como los trimmers por voz de Diva): afinación ±50 cents y corte ±1 octava para las voces 1–8; las voces 9–16 reutilizan los mismos de forma cíclica. Se suman a la huella analógica de cada voz, no la reemplazan.
- Tests:
  - DUO: 2 voces por nota, 4 con dos notas, y todas se liberan al soltar;
  - trim de afinación medido: +50 cents ±2 en su voz y 0 ±1 en las demás.

## D-027 · Panel ampliado (2026-09-26)
- El lienzo pasa de 1536×1024 a **1536×1400**: el diseño original intacto más dos filas nuevas, todo en una sola vista (decisión del usuario: "Ampliar el panel").
  - Fila 4: ARPEGGIATOR · LFO 2 · MOD ENV (con curva) · OSC + (SUB, RING, FM B›A, octava del sub) · HPF / VOICE (HPF, POLY/DUO, QUALITY).
  - Fila 5: FUZZ · PHASER · FX OPTIONS (modo de chorus, sync/división/ping-pong del delay, HALL/PLATE) · VOICE TRIMS (8 × afinación y corte).
- Cambios en paneles existentes:
  - FILTER: MODEL pasa a menú desplegable (5 modelos) y se añaden SLOPE y MODE.
  - MATRIX: páginas 1-4 / 5-8.
- Widget nuevo `ParamChoiceBox`: desplegable ligado a un parámetro choice/int.
- Escala por defecto 65 % (998×910 px); tamaños de 50 % a 125 % en SETTINGS, donde también está "Render offline in DIVINE quality".
- `augur_preset_audit --snapshot panel.png 2` renderiza el editor a PNG, para revisar el diseño sin abrir un DAW.
- pluginval estricto 10: SUCCESS.

## D-028 · Librería ampliada: 162 sonidos (2026-09-26)
- **94 sonidos nuevos** (`plugin/PresetsExpansion.inc`), construidos sobre el motor ampliado. Total: 162 + Init.

| Categoría | Nuevos | Qué usan |
|---|---|---|
| Signature | 12 | El FUZZ como protagonista melancólico, con phaser, plate y DUO |
| Bass | 12 | Sub, CASCADE, BITE ácido, FM knock, reese DUO, LFO 2 sincronizado, 12 dB |
| Lead | 10 | BITE, FM, ring, DUO, flauta con MOD ENV → ruido |
| Pad | 12 | Multimodo BP/HP, plate, DUO, phaser, S&H suave, HPF |
| Pluck | 12 | FM con MOD ENV, NOTE RANDOM, ping-pong, BP |
| Keys | 7 | Tine FM con velocidad → FM, clav BITE, vibráfono con trémolo por LFO 2 |
| Stab | 7 | Delays con sync 1/8D y ping-pong |
| Arp | 10 | Arpegiador interno: modos, octavas, swing, latch, tresillos |
| Drums | 4 | Ring metálico, tom FM, shaker, sub boom |
| Atmos & FX | 8 | Drones con fuzz, S&H de computadora, riser de 8 compases por MOD ENV |

- Delays y LFO 2 sincronizados a tempo, así que los sonidos encajan en cualquier BPM del proyecto.
- **La auditoría ahora falla si un preset nombra un parámetro inexistente** (antes se ignoraba en silencio).
- Nivelación: `level_presets.py` recorre los dos archivos. Tres pasadas dejan todo en su objetivo, salvo dos sonidos percusivos que quedan 2–3 dB por debajo porque los limita el techo de pico.
- Soak de 30 s en los Signature: sin acumulación.
- CPU con 12 voces y todos los efectos: 24 % de un núcleo en promedio.

## D-029 · Panel en formato ancho (2026-09-26)
- A pedido del usuario (el panel de 1536×1400 no cabía en su pantalla), el lienzo pasa a **2608×1024**, en formato ancho para pantallas 16:9.
  - Las tres filas del diseño original quedan intactas a la izquierda.
  - Los módulos nuevos van en un bloque a la derecha, alineados con esas tres filas: ARP / LFO 2 / HPF-VOICE, luego MOD ENV / FX OPTIONS / VOICE TRIMS, luego FUZZ / PHASER / OSC +.
  - Cada panel conserva su distribución interna, centrada en su nuevo tamaño (tabla `expansionPanels`); controles y rótulos se mueven juntos.
- Escala por defecto 65 %: 1695×666 px, que cabe en 1920×1080 con la ventana del DAW.
- Al abrir, la ventana se limita al área útil de la pantalla, por si una sesión se guardó en un monitor más grande.
- Tamaños de 45 % a 100 %; mínimo 40 % (1043 px), para portátiles de 1366 px.

## D-030 · Octava por oscilador y legibilidad (2026-09-26)
- **OCTAVE** (−2 … +2) en OSC 1 y OSC 2 (`osc1_oct`, `osc2_oct`). Se suma en semitonos enteros a la parte digital del CV, junto con FREQUENCY, y pasa por la misma cuantización del DAC. Test: ±1 y ±2 octavas exactas (log2 ±0.003).
- **Legibilidad** (reporte del usuario: "las letras se ven muy chiquitas aunque aumente el tamaño"):
  - Los rótulos de 9–11 px del mockup quedaban en ~6 px en pantalla al 65 %. Ahora el texto pequeño crece ×1.32, el mediano ×1.18 y el grande ×1.06, con menos espaciado entre letras.
  - Rótulos y captions más claros, con más contraste sobre el fondo negro.
  - Todo texto que no cabe en su área se reduce solo (hasta el 60 %) en lugar de cortarse.
  - La etiqueta de cada perilla se limita al ancho de su perilla, así que nunca toca la vecina.

## D-031 · TAPE ECHO (multicabezal de cinta) y reverb de muelles (2026-09-26)
- Pedido del usuario: el reverb no le gustó; quiere algo tipo la unidad clásica de eco de cinta con muelles de los 70. No se usan sus marcas: el módulo se llama "TAPE ECHO" y la reverb "SPRING".
- **`SpringReverb`**: tres muelles. Cada uno es un lazo de realimentación alrededor de una cadena dispersiva de 48 all-pass estirados, H(z) = (a + z^-K)/(1 + a z^-K), con K ≈ fs/8 kHz y a = 0.62 (modelo de muelle de Parker y Välimäki).
  - La cadena retrasa más los graves, así que cada transitorio se convierte en el "chirp" característico del muelle.
  - En el lazo hay un retardo de ida y vuelta (37/43/51 ms), una caída a 4.5 kHz y una deriva aleatoria lenta del 0.3 %.
  - La entrada pasa por un HP de 180 Hz, porque el muelle es delgado en graves.
  - Salida: muelle 1 a L, muelle 2 a R y muelle 3 al centro.
  - RT60 medido dentro de ±40 % (test a 1.5 y 3 s). Es además el tercer tipo de la reverb principal (HALL / PLATE / SPRING): SIZE fija la tensión y DECAY el RT60.
- **`TapeEcho`**:
  - Cinta en bucle con cabeza de grabación y 3 de reproducción (1 : 1.95 : 2.9). REPEAT RATE va de 250 a 55 ms en la cabeza 1, con inercia de transporte: el cambio de velocidad desliza el pitch.
  - Wow de 0.55 Hz, flutter de 7.3 Hz y deriva aleatoria, con profundidad regulable.
  - Grabación: entrada con LP de 7 kHz y saturación de cinta suave y algo asimétrica; INPUT controla el drive.
  - Reproducción: ancho de banda 110 Hz–3.8 kHz y BASS/TREBLE (±) en el preamp. Como el preamp está antes del lazo, los controles también dan forma a las repeticiones.
  - INTENSITY llega a una ganancia de lazo de 1.15, así que se desboca en autooscilación, y la saturación lo contiene (test: pico acotado con todo al máximo).
  - Selector de 12 modos: 1–4 solo eco (H1, H2, H3, H2+3); 5–11 eco + muelles (H1, H2, H3, H1+2, H2+3, H1+3, todas); 12 solo muelles. Las cabezas 1 y 2 van algo abiertas en estéreo.
  - El muelle recibe la entrada más el eco.
- Cadena: … DELAY → TAPE ECHO → REVERB.
- Panel: fila nueva a lo ancho (lienzo 2608×1216; en pantallas de 1920 queda a la misma escala que antes). Incluye selector de modo, indicadores de cabezas y muelle, 8 perillas y un dibujo del bucle de cinta con las cabezas activas encendidas.
- Tests: posición de las cabezas por modo, runaway acotado, RT60 del muelle y determinismo con bloques aleatorios.
- CPU a 48 kHz: +2.9 % el tape echo con muelles, +1.8 % la reverb de muelles.
- 6 presets nuevos (168 sonidos): Dub Tape Chords, Runaway Tape Lead, Spring Tine Keys, Tape Loop Pad, Echo Chamber Pluck y Spring Drip Stab.

## D-032 · MASTER VOLUME y etiquetas completas (2026-09-26)
- **MASTER** (`master_volume`, −60 … +6 dB, por defecto 0 dB), en la cabecera junto a UNDO. Se multiplica por el LEVEL del preset con el mismo suavizado.
  - LEVEL (`amp_level`) sigue siendo parte de cada sonido: es su nivelación de volumen.
  - MASTER es global y no se guarda en presets: `PresetManager::isGlobalSetting` lo excluye (igual que QUALITY y OFFLINE QUALITY) al restablecer, cargar y guardar presets de usuario. Sí se guarda con la sesión del DAW.
- **Corrección** (reporte del usuario: "las letras se cortan de la mitad para abajo"): al agrandar el texto (D-030), la etiqueta de cada perilla quedaba más alta que el área de su componente y se recortaba. La perilla ahora reserva size + 42 px de alto (antes + 34) y un área de etiqueta de 20 px.

## D-033 · Packs de expansión, importar carpetas y corrección de volumen al cargar (2026-09-26)
- **Browser**: los presets de usuario se leen de forma recursiva y se muestran como árbol de carpetas (Pack > Género > preset) bajo "USER / EXPANSIONS".
  - "Install expansion pack (.zip)…": extrae solo `.augur5`, `.txt` y `.md`, y rechaza rutas con `..`, absolutas o fuera de la carpeta de presets.
  - "Add presets folder…": pedido del usuario, que solo podía añadir preset por preset. Copia todos los `.augur5` de una carpeta y sus subcarpetas, conservando la estructura bajo el nombre de la carpeta.
- **Corrección:** al cargar un sonido, el nivel (y todos los suavizadores) se deslizaba 20 ms desde los valores del sonido anterior, así que la primera nota tocada justo después de cargar salía con el volumen del preset previo. `warmUp()` ahora reinicia los suavizadores a los valores nuevos (test). Esto también afectaba a la medición de nivel de los plucks: la librería de fábrica se re-niveló.
- **`augur_preset_audit --level-pack <carpeta>`**: carga cada preset en el procesador real y mide pico y volumen. Ajusta `amp_level` hasta el objetivo guardado en el archivo (hasta 4 pasadas, sin superar −3 dBFS de pico) y falla con parámetros desconocidos, salida no finita o silencio.
- **Savanna Horn Lead** (Lead), pedido por el usuario: lead de bronce "trompeteante" con sierras desafinadas en unísono, un "scoop" de pitch al inicio de cada nota (MOD ENV → OSC 1/2 FREQ, −3 semitonos), filtro que se abre, legato con glide y vibrato retardado. No lleva nombres de artistas ni canciones.

## D-034 · AUGUR-5 Anthology Vol.1: 500 presets de pago (TONAL LAB) (2026-09-26)
- Producto aparte: **500 presets en 20 estilos** (25 por estilo).
  - Estilos: Techno, Melodic Techno, Progressive House, Deep House, House, Tech House, Minimal, Trance, Psytrance, Drum & Bass, Dubstep, UK Garage, Synthwave, Electro, Acid, Ambient, Downtempo & Lo-Fi, IDM, Future Bass y Dub Techno.
  - Prefijos por rol: BA, LD, PD, PL, CH, KY, AR, FX, DR.
- **Generación** (`tools/pack/make_anthology.py`): diseño por reglas y reproducible (semilla por estilo).
  - Arquetipos por rol: 9 de bajo (sub, rolling, reese, acid, FM, wobble, 808, pluck…), 9 de lead (incluido "horn", el lead trompeteante), 9 de pad, 7 de pluck, 6 de stab, 6 de keys, 6 de atmósfera, 5 de percusión y arps con el arpegiador interno.
  - Perfil por estilo: oscuridad, edad analógica, espacio, divisiones de delay, tipo de reverb y probabilidad de fuzz, tape echo, phaser y chorus; swing y modos/velocidades del arp.
  - Mod wheel → cutoff en casi todos.
  - Nombres propios de TONAL LAB, sin artistas ni marcas.
- **Control de calidad** (`augur_preset_audit --level-pack`): cada preset pasa por el procesador real y se nivela a su objetivo (−18 dB melódicos; percusión −16/−20/−25/−29 según tipo; pico ≤ −3 dBFS).
  - Resultado final: **500/500 dentro de rango**.
  - Correcciones hechas por lo que reveló la medición:
    - el arquetipo "pluck band-pass" tenía un transitorio de aguja (ahora es una banda estática);
    - la percusión metálica tiene objetivo −29 dB, por su factor de cresta;
    - el deslizamiento de nivel al cargar (D-033).
- **Distribución**: `tools/pack/build_zip.py` genera `packs/dist/TONAL LAB - AUGUR-5 Anthology Vol.1 (500 presets).zip` (328 KB, rutas portables `/` para macOS).
  - Instalación en el plugin: BROWSER > Install expansion pack (.zip), o descomprimir y usar "Add presets folder".
  - Verificado con `--install-pack` y `--import-folder`: 500 instalados y 500 visibles en el browser.
  - El zip no va al repositorio (`packs/dist/` en `.gitignore`); los presets fuente sí.

## D-035 · Versión 1.0.0 e instaladores (2026-09-26)
- Versión del producto **1.0.0**: la primera completa.
- **macOS** (`installer/mac/build_pkg.sh`, se ejecuta en la CI de macOS después de tests, pluginval y auval): `AUGUR-5 1.0.0 (macOS).pkg`, binarios universales (arm64 + x86_64), macOS 11+. El instalador tiene tres opciones:

| Opción | Destino |
|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3` |
| AU | `/Library/Audio/Plug-Ins/Components` |
| App | `/Applications` |

  - Firma ad-hoc (sin Developer ID todavía). El postinstall quita la cuarentena y reinicia `AudioComponentRegistrar` para que Logic re-escanee.
  - Cómo firmar y notarizar con certificado: `installer/mac/README.md`.
- **Windows**: `AUGUR-5 1.0.0 (Windows).zip`, con el VST3 y el Standalone.
- Cada push sube los instaladores como artefactos. Un tag `v*` publica un **GitHub Release** con el .pkg y el .zip.
