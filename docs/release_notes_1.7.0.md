# AUGUR-5 "3340" · versión 1.7.0

**TONAL LAB · noviembre de 2026**

El estéreo queda en su sitio: las notas y los acordes ya no saltan de un lado al otro, el eco de cinta suena centrado y puedes cambiar de modo en cualquier efecto mientras tocas sin oír un clic.

---

## Lo nuevo en 1.7.0

### El estéreo ya no salta de lado
Con SPREAD abierto, cada voz tenía un lugar fijo y las notas rotaban entre voces: una nota sonaba a la izquierda y la siguiente a la derecha, y un acorde se cargaba a un lado y el siguiente al otro.

Ahora la imagen queda siempre centrada:
- una nota sola suena al medio;
- un acorde se abre a los dos lados por igual, con el mismo ancho de antes;
- PAN sigue moviendo el instrumento entero a donde quieras.

**BY VOICE**, el botón nuevo debajo de PAN, vuelve al paneo clásico por voz si lo prefieres.

### El eco de cinta, centrado
Con una sola cabeza (o con dos vecinas) el eco entero se iba hacia un lado. Ahora las cabezas que suenan se reparten alrededor del centro, con la misma separación entre ellas.

### Cambiar de modo sin clics
Antes se oía un clic al cambiar, con una nota sonando:
- el modelo de DRIVE, el modo de CHORUS, las etapas del PHASER, THROUGH-ZERO del FLANGER, PING-PONG, las cabezas del TAPE ECHO o el tipo de REVERB;
- el modelo del filtro o su pendiente (24 / 12 dB).

Ahora el efecto baja un instante, cambia y vuelve. También se nota en AUGURY: un vuelo que pasa de una esquina a otra con efectos distintos ya no hace clic.

### Controles que ya no hacen escalones
HPF, PAN y SPREAD se deslizan al moverlos o automatizarlos. El DECAY de la reverb HALL también.

### Más correcciones en los efectos
- **DRIVE FOLD** ya no sube el volumen de golpe (antes, unos 15 dB al elegirlo).
- **REVERB** ROOM, HALL y SHIMMER quedan equilibradas entre izquierda y derecha.
- Al cargar un preset, los efectos están activos desde la primera nota. Antes, el ataque de esa primera nota sonaba sin ellos.

### El mismo sonido con cualquier buffer
En algunos casos el resultado exacto dependía del tamaño de buffer del DAW. Ahora un proyecto suena igual, muestra por muestra, con cualquier buffer y al exportar.

---

## Compatibilidad

- **Tus proyectos** abren con BY VOICE encendido: las voces suenan repartidas como antes. Apágalo cuando quieras el estéreo centrado.
- **Los presets** (de fábrica, de los packs y los tuyos) cargan con el estéreo centrado.
- **Cambia el sonido**, en proyectos y presets, de los que usan TAPE ECHO con una cabeza o con dos vecinas (el eco pasa al centro) y, muy poco, el reparto de las reverbs ROOM, HALL y SHIMMER. El volumen no cambia.
- Los packs no cambian: no hace falta reinstalarlos.

---

## Instalación

**Windows**: ejecuta `AUGUR-5 1.7.0 Setup (Windows x64).exe`. Instala:
- el VST3 en `C:\Program Files\Common Files\VST3`;
- la aplicación independiente;
- SUN ATLAS en `%APPDATA%\TONAL LAB\AUGUR-5\Presets`.

Se instala encima de la versión anterior.

**macOS** (Apple Silicon e Intel, macOS 11 o posterior): abre `AUGUR-5 1.7.0 (macOS).pkg`. Instala el VST3, el Audio Unit (Logic Pro), la aplicación y SUN ATLAS.

La primera vez macOS lo bloquea porque el instalador todavía no está notarizado. Ve a **Ajustes del Sistema › Privacidad y seguridad › Abrir igualmente**.

Después de instalar, vuelve a escanear los plug-ins en tu DAW.
