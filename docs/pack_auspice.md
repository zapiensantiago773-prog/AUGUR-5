# AUGUR-5 — AUSPICE: melodic & progressive house

Generador: [`tools/pack/make_auspice.py`](../tools/pack/make_auspice.py). Resultado: `packs/AUSPICE/<CATEGORÍA>/<nombre>.augur5`; zip: `python tools/pack/build_zip.py AUSPICE`. Cada sonido está escrito a mano (sin recetas al azar).

| Categoría | Sonidos |
|---|---|
| BASS | 20 |
| ARP | 15 |
| PLUCK | 15 |
| LEAD | 15 |
| PAD | 15 |
| KEYS | 15 |
| BRASS | 15 |
| TEXTURE | 15 |
| DRONE | 15 |
| FX | 15 |

**Nivel:** mediana -16.0 dB (objetivo -16, los 400 ms más fuertes), rango -17.0 … -15.9; pico máximo -0.9 dBFS. Los acordes (pads, keys, brass, plucks, arps) tampoco pasan de −1 dBFS.

**Reproducibilidad:** toda la librería otra vez en orden inverso: diferencia máxima 0.00 dB.

**Diferencias entre sonidos** (huella tímbrica sin efectos, distancia al vecino más cercano de su categoría; 1.0 ≈ audible en monitores, menos de 1.5 = el mismo sonido): mediana 2.80; el par más cercano, Keel of Dusk ~ Comet Knock, 1.14. Mínimo exigido: 1.5; en BASS, 1.1: los veinte tocan el mismo patrón mono y la huella empieza en 60 Hz, por encima del fundamental de A1, así que solo los separa el espectro de 60 Hz para arriba.

## Bajos

[`tools/pack/bass_metrics.py`](../tools/pack/bass_metrics.py), patrón rodante de semicorcheas a 122 BPM en A1. SUB 30–90 Hz, BODY 90–300, BITE 300–2000, SMALL > 150 Hz (lo que suena en un parlante chico); LOWW = estéreo bajo 150 Hz; PUNCH = pico de los primeros 12 ms sobre el cuerpo; EVEN = variación entre notas; RANGO = diferencia de nivel entre E1, A1, D2 y G2.

| Bajo | SUB | BODY | BITE | SMALL | LOWW | PUNCH dB | EVEN dB | CREST dB | RANGO dB |
|---|---|---|---|---|---|---|---|---|---|
| Keel of Dusk | 48 % | 39 % | 12 % | 23 % | 0.002 | 8.0 | 1.2 | 12.6 | 0.1 |
| Templum Sub | 59 % | 32 % | 9 % | 18 % | 0.002 | 7.7 | 0.9 | 12.3 | 0.1 |
| Night Current | 50 % | 35 % | 14 % | 29 % | 0.002 | 8.7 | 1.1 | 13.0 | 0.2 |
| Iron Wingbeat | 42 % | 44 % | 13 % | 25 % | 0.002 | 9.4 | 1.3 | 11.8 | 0.2 |
| Augury Rubber | 47 % | 39 % | 13 % | 21 % | 0.002 | 7.1 | 1.0 | 12.2 | 0.2 |
| Sunset Engine | 59 % | 29 % | 11 % | 26 % | 0.002 | 7.1 | 1.7 | 9.2 | 0.1 |
| Pressure Lattice | 60 % | 30 % | 9 % | 20 % | 0.002 | 9.3 | 1.0 | 12.6 | 0.1 |
| Shoreline Roller | 42 % | 45 % | 11 % | 35 % | 0.002 | 6.6 | 1.3 | 9.8 | 0.2 |
| Black Feather | 49 % | 37 % | 14 % | 29 % | 0.002 | 8.5 | 1.4 | 12.9 | 0.3 |
| Velvet Undertow | 43 % | 49 % | 7 % | 19 % | 0.002 | 9.5 | 1.8 | 12.6 | 0.3 |
| Comet Knock | 45 % | 43 % | 11 % | 19 % | 0.002 | 7.5 | 1.0 | 13.4 | 0.4 |
| Low Sun Arc | 55 % | 29 % | 15 % | 32 % | 0.002 | 11.0 | 1.3 | 13.8 | 0.4 |
| Driver of Omens | 57 % | 34 % | 8 % | 19 % | 0.002 | 7.1 | 1.3 | 11.7 | 0.5 |
| Marrow | 50 % | 40 % | 9 % | 19 % | 0.002 | 7.3 | 1.2 | 12.2 | 0.1 |
| Gilded Groove | 57 % | 31 % | 12 % | 29 % | 0.002 | 6.8 | 1.4 | 13.5 | 0.2 |
| Abyssal Pulse | 41 % | 48 % | 10 % | 18 % | 0.002 | 8.2 | 1.9 | 12.9 | 0.2 |
| Granite Wing | 59 % | 29 % | 10 % | 24 % | 0.002 | 10.5 | 1.7 | 13.9 | 1.4 |
| Ember Keel | 64 % | 21 % | 12 % | 25 % | 0.002 | 11.4 | 1.7 | 13.1 | 0.2 |
| Tidal Root | 51 % | 37 % | 11 % | 22 % | 0.002 | 6.5 | 1.2 | 11.6 | 0.2 |
| Monolith Floor | 48 % | 39 % | 13 % | 23 % | 0.002 | 7.0 | 1.3 | 12.3 | 0.5 |
| **mediana** | 50 % | 37 % | 11 % | 23 % | 0.002 | 8.0 | 1.3 | 12.6 | |

Metas: SUB 0.4…0.65, BODY 0.2…1.0, BITE 0.06…0.25, SMALL 0.18…1.0, LOWW 0.0…0.01, PUNCH 6.0…12.0, EVEN 0.0…3.0, CREST 0.0…14.0; RANGO ≤ 3 dB.
