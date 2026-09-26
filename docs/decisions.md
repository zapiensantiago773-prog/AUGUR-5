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
`plugin/Parameters.h` es la fuente única de IDs (vienen del diseño de GUI). Cada parámetro se registra en el APVTS en la fase donde su DSP existe, para que ningún control esté "muerto". Todos usan `ParameterID { id, 1 }`; la versión solo sube si cambia el significado. En la Fase 0 solo está registrado `amp_level`.

## D-009 · Estado versionado
`getStateInformation` escribe `stateVersion` (hoy 1) en el ValueTree. `setStateInformation` ignora XML de otro tipo; las migraciones se agregan cuando cambie el formato.
