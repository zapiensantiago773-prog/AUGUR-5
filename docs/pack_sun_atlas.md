# AUGUR-5 — SUN ATLAS: el banco de presets

Generador: [`tools/pack/make_sun_atlas.py`](../tools/pack/make_sun_atlas.py). Resultado: `packs/SUN ATLAS/<CATEGORÍA>/<nombre>.augur5`; en el navegador aparece como la colección **SUN ATLAS**, con sus categorías. Zip: `python tools/pack/build_zip.py "SUN ATLAS"`.

Semilla 5. 3000 candidatos (3 por plaza); 1000 elegidos.

| Categoría | Presets | Arquetipos |
|---|---|---|
| PAD | 160 | airy 7, breath 9, choir 10, dark 32, fifths 21, glass 9, polymod 25, pwm 17, strings 17, sync_swell 13 |
| KEYS | 95 | bell 11, clav 3, epiano 9, felt 12, harpsi 16, organ 15, poly_keys 19, vibes 10 |
| PLUCK | 100 | classic 29, glass 15, harp 9, kalimba 22, noise 1, polymod 17, sync 7 |
| LEAD | 95 | brass_lead 9, fm 5, fuzz 12, horn 10, pwm 12, soft 16, sync 5, unison 22, whistle 4 |
| BASS | 120 | 808 7, acid 8, fm 5, pluck 5, pulse 8, reese 34, rolling 8, saw 11, sub 7, unison 27 |
| BRASS | 60 | braam 8, section 18, soft_horn 8, stab 16, swell 10 |
| ARP | 100 | octaves 12, order 18, random 14, triplet 19, up16 15, updown 22 |
| SEQ | 90 | bassline 17, melodic 11, pulse 5, ratchet 20, swing 19, triplet 18 |
| TEXTURE | 85 | crushed 14, flutter 12, noise_wash 8, phase_wind 12, ring_metal 12, sh_bubbles 11, spring_drips 4, tape_dust 12 |
| DRONE | 55 | beating 14, dark_drone 9, fifth_drone 7, noise_drone 7, shimmer_drone 18 |
| FX | 40 | alarm 5, fall 6, impact 10, laser 3, riser 5, sci 4, sweep 3, zap 4 |

| Familia | Presets |
|---|---|
| IDM | 137 |
| NEO | 91 |
| CINE | 167 |
| TECHNO | 177 |
| HOUSE | 118 |
| COLOR | 139 |
| HYPNO | 171 |

**Nivel:** mediana -16.0 dB (objetivo -16, los 400 ms más fuertes), rango -21.9 … -15.9; pico máximo -0.9 dBFS.

**Resonancia:** mediana 0.15, máximo 0.54; 13 presets por encima de 0.40.

**Diferencias entre sonidos** (huella tímbrica sin efectos, distancia al vecino más cercano de su categoría; 1.0 ≈ audible en monitores): mediana 1.63, el 5 % más cercano 0.85.

**Efectos:** REVERB 800, ECHO 250, DELAY 149, CHORUS 270, PHASER 89, FLANGER 18, DRIVE 101, COMP 191, FUZZ 12.

**Reproducibilidad:** cada preset se tocó dos veces, en orden directo e inverso (vecinos distintos). Diferencia máxima 0.02 dB; mediana 0.00 dB.

**Descartados:** level: 175, brightest 6 %: 151, spiky: 143, dc: 33, bright: 33.
