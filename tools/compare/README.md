# A/B entre instrumentos (`tools/compare`)

Mide librerías de presets de AUGUR-5 y de otros instrumentos tocadas **igual**: las mismas frases que `augur_preset_audit --pack` (122 BPM, velocity 100, 48 kHz), y analizadas con las mismas métricas. Así se comparó AUGUR-5 con MANTIS-37 y PYTHIA 32 en D-045.

## 1. Renderizar

**AUGUR-5**: un pack o la librería de fábrica.

```
build\win-release\plugin\augur_preset_audit_artefacts\Release\augur_preset_audit.exe --pack "packs/SUN ATLAS/PLUCK" --wav build\cmp\augur
build\win-release\plugin\augur_preset_audit_artefacts\Release\augur_preset_audit.exe --factory-wav build\cmp\factory Pluck
```

**MANTIS-37** (su propia auditoría):

```
mantis_preset_audit.exe --pack "packs/HORIZON ATLAS/PLUCK" --wav <carpeta>
```

**Cualquier VST3** cuyos presets sean su estado (PYTHIA 32: `.pythia32`), con `compare_host`:

```
cmake -S tools/compare/host -B build/compare_host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/compare_host
build\compare_host\compare_host_artefacts\Release\compare_host.exe --role pluck "C:\Program Files\Common Files\VST3\PYTHIA 32.vst3" build\cmp\pythia <presets...>
```

Roles: `pluck` (la frase de 8 notas), `keys`, `pad`, `lead`, `bass`.

## 2. Comparar

```
tools\.venv\Scripts\python.exe tools\compare\compare.py AUGUR=build\cmp\augur MANTIS=<carpeta> PYTHIA=build\cmp\pythia
```

Imprime mediana y cuartiles de: volumen (400 ms más fuertes, la escala de los packs), pico, factor de cresta, low (< 150 Hz), air (> 8 kHz), ancho estéreo y, por nota en la frase de pluck, ataque, "punch", caída a −20 dB, barrido del filtro y brillo inicial.

Las métricas por nota suponen la frase de pluck; con otros roles solo valen las globales.
