# AUGUR-5 — SUN ATLAS: el banco de presets

Generador: [`tools/pack/make_sun_atlas.py`](../tools/pack/make_sun_atlas.py). Resultado: `packs/SUN ATLAS/<CATEGORÍA>/<nombre>.augur5`; en el navegador aparece como la colección **SUN ATLAS**, con sus categorías. Zip: `python tools/pack/build_zip.py "SUN ATLAS"`.

Semilla 5. 3000 candidatos (3 por plaza); 1000 elegidos.

| Categoría | Presets | Arquetipos |
|---|---|---|
| PAD | 160 | airy 6, breath 9, choir 11, dark 31, fifths 22, glass 8, polymod 24, pwm 18, strings 19, sync_swell 12 |
| KEYS | 95 | bell 11, clav 11, epiano 10, felt 10, harpsi 12, organ 16, poly_keys 16, vibes 9 |
| PLUCK | 100 | classic 30, glass 14, harp 7, kalimba 12, noise 11, polymod 15, sync 8, wood 3 |
| LEAD | 95 | brass_lead 14, fm 4, fuzz 16, horn 10, pwm 17, soft 7, sync 4, unison 19, whistle 4 |
| BASS | 120 | 808 5, acid 9, fm 5, pluck 6, pulse 8, reese 32, rolling 8, saw 13, sub 10, unison 24 |
| BRASS | 60 | braam 8, section 16, soft_horn 7, stab 15, swell 14 |
| ARP | 100 | octaves 9, order 18, random 14, triplet 17, up16 17, updown 25 |
| SEQ | 90 | bassline 18, melodic 8, pulse 16, ratchet 24, swing 11, triplet 13 |
| TEXTURE | 85 | crushed 14, flutter 8, noise_wash 8, phase_wind 9, ring_metal 9, sh_bubbles 10, spring_drips 15, tape_dust 12 |
| DRONE | 55 | beating 11, dark_drone 10, fifth_drone 9, noise_drone 7, shimmer_drone 18 |
| FX | 40 | alarm 3, fall 4, impact 7, laser 6, riser 5, sci 2, sweep 1, zap 12 |

| Familia | Presets |
|---|---|
| IDM | 137 |
| NEO | 91 |
| CINE | 167 |
| TECHNO | 177 |
| HOUSE | 118 |
| COLOR | 139 |
| HYPNO | 171 |

**Nivel:** mediana -16.0 dB (objetivo -16, los 400 ms más fuertes), rango -22.0 … -15.9; pico máximo -0.9 dBFS.

**Resonancia:** mediana 0.15, máximo 0.49; 16 presets por encima de 0.40.

**Diferencias entre sonidos** (huella tímbrica sin efectos, distancia al vecino más cercano de su categoría; 1.0 ≈ audible en monitores): mediana 1.58, el 5 % más cercano 0.74.

**Efectos:** REVERB 795, ECHO 261, DELAY 155, CHORUS 272, PHASER 90, FLANGER 18, DRIVE 93, COMP 188, FUZZ 16.

**Reproducibilidad:** cada preset se tocó dos veces, en orden directo e inverso (vecinos distintos). Diferencia máxima 0.01 dB; mediana 0.00 dB.

**Descartados:** brightest 6 %: 166, spiky: 44, dc: 43, bright: 30, level: 17.

**Acordes (D-047):** después de la selección, `augur_preset_audit --chord-headroom` bajó el LEVEL de 88 de los 600 sonidos de acordes (PAD, KEYS, BRASS, PLUCK, ARP) para que, tocados como acorde, no pasen de -1 dBFS. Los plucks quedan en -17.8 dB de mediana con notas sueltas (pico -3.3 dBFS).
