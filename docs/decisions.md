# Decisiones de arquitectura — AUGUR-5 "3340"

Formato: contexto → decisión → consecuencias. Las más recientes van al final.

## D-001 · Dependencias como git submodules (2026-09-25)
JUCE 8.0.15 y Catch2 v3.16.0 en `external/`, fijados a tag. Alternativa descartada: `FetchContent` (descarga JUCE en cada build limpio y no funciona sin red). Actualizar = `git -C external/JUCE checkout <tag>` + commit.

## D-002 · CMake Presets + Ninja
`win-debug/win-release` (MSVC x64) y `mac-debug/mac-release` (universal arm64+x86_64). Ninja da builds rápidos y `compile_commands.json`. En Windows el preset usa `architecture.strategy = external`: CMake Tools carga el entorno de MSVC por su cuenta.

## D-003 · Runtime de MSVC estático (`/MT`)
El plugin se tiene que cargar en cualquier PC, aunque no tenga el VC++ Redistributable instalado (la computadora con Ableton del usuario, clientes). Costo: binario un poco más grande. Se aplica a todo el proyecto para evitar mezclar runtimes.

## D-004 · Identidad del plugin (inmutable)
`PRODUCT_NAME "AUGUR-5"`, fabricante `Augr`, plugin `Au53`, bundle `com.auguraudio.augur5`. Los DAWs guardan proyectos y presets con estos valores; cambiarlos después del release rompe las sesiones de los usuarios. "3340" es el nombre del modelo y solo aparece en la GUI (las comillas no son válidas en nombres de archivo).

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
