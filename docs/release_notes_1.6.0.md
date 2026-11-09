# AUGUR-5 "3340" · versión 1.6.0

**TONAL LAB · noviembre de 2026**

AUGURY se vuelve una herramienta de producción: el ave del morph se puede automatizar en el DAW y mover con un control MIDI, y los presets guardan su escena. El grave de los efectos queda en fase en mono, el rack de efectos tiene varias correcciones y el motor gasta un poco menos de CPU con el mismo sonido.

---

## Lo nuevo en 1.6.0

### AUGURY automatizable: MORPH X y MORPH Y
La posición del ave son ahora dos parámetros, **Morph X** y **Morph Y**.
- **Automatízalos** en tu DAW para que el sonido viaje entre las cuatro esquinas a lo largo de la canción. También funciona al exportar.
- **Asígnalos a un control MIDI** desde tu DAW (un knob, un fader o un pad XY) y toca el morph en vivo.
- Mientras se mueven, el panel muestra cómo cambian los controles. Si después giras un control, se oye tu ajuste.
- UNDO, cargar un preset o abrir un proyecto devuelven el sonido y el ave juntos, sin mezclar de más.

### El morph ya no corta las notas
Antes, al volar entre un sonido POLY y uno UNISON, las notas que mantenías se soltaban. Ahora VOICES, UNISON, POLY/DUO y el arpegiador no se mueven con el ave: quedan como los estabas tocando y el acorde sigue sonando. **RECALL** sí aplica los de la esquina, con su sonido exacto.

### OMEN ya no deja el sonido mudo
Al pulsar OMEN muchas veces seguidas, algunas variaciones quedaban casi en silencio. Ahora OMEN cuida lo que hacía sonar a tu sonido:
- no cierra el filtro con la envolvente invertida ni sube el HPF por encima de él;
- no cambia el ruteo de la matriz ni el modo del filtro;
- un sonido percusivo sigue percusivo y uno sostenido sigue sosteniendo.

Las variaciones siguen siendo amplias; ninguna queda muda.

### Los presets guardan su escena AUGURY
Al guardar un preset con esquinas capturadas, el preset se lleva las cuatro esquinas y la posición del ave. Un preset sin escena deja tus esquinas como están, así que puedes seguir capturando sonido tras sonido.

**AUGURY Scenes** (pack aparte, 10 presets): diez sonidos de AUSPICE, uno por categoría, cada uno con su escena lista para volar. La esquina A es el sonido original y B, C y D son variaciones con el mismo volumen.

### El grave en fase en mono
El grave (bajo 150 Hz) de CHORUS, DELAY, TAPE ECHO, REVERB y SPRING, y el de UNISON y DUO, ahora sale al centro. Ya no se cancela al sumar a mono en un club ni en un parlante de celular. Por encima de 150 Hz el estéreo sigue igual, y la señal sin efecto no se toca.

### Correcciones en los efectos
- **DRIVE** ya no deja corriente continua (DC) en la salida.
- **DRIVE CRUSH** con MIX parcial ya no produce un efecto de peine que restaba graves.
- **DELAY** (MIX y FEEDBACK), el **PREDELAY** del reverb y otros controles ya no hacen ruido de escalones ni clics al automatizarlos.
- Si un efecto llegara a fallar numéricamente, el rack se limpia solo en vez de quedarse sonando roto.

### Menos CPU, mismo sonido
El motor se reorganizó para gastar alrededor de un 4 % menos de CPU. El audio es idéntico, muestra por muestra, al de 1.5.0.

### Icono nuevo
AUGUR-5 vuelve a su símbolo propio, blanco: el horizonte del augur con el sol y dos aves. Es el icono de la aplicación, de la carpeta VST3 y del instalador.

---

## Compatibilidad

- **Tus proyectos y presets** cargan con sus mismos ajustes. Un proyecto de 1.5 pone el ave donde la tenía.
- **El sonido solo cambia** en el grave de los efectos y de UNISON/DUO, que ahora queda centrado, y en sonidos con DRIVE (sin DC) o CRUSH con MIX parcial (sin el peine).
- **Presets de los packs**: 181 presets que movía el cambio de los efectos vuelven a su volumen exacto, y unos pocos bajan unas décimas para no pasar de −1 dBFS en acordes.
- **Tus presets de usuario no se tocan.**
- Un preset con escena AUGURY abierto en una versión anterior carga su sonido, sin la escena.

---

## Instalación

**Windows**: ejecuta `AUGUR-5 1.6.0 Setup (Windows x64).exe`. Instala:
- el VST3 en `C:\Program Files\Common Files\VST3`;
- la aplicación independiente;
- SUN ATLAS en `%APPDATA%\TONAL LAB\AUGUR-5\Presets`.

Se instala encima de la versión anterior.

**macOS** (Apple Silicon e Intel, macOS 11 o posterior): abre `AUGUR-5 1.6.0 (macOS).pkg`. Instala el VST3, el Audio Unit (Logic Pro), la aplicación y SUN ATLAS.

La primera vez macOS lo bloquea porque el instalador todavía no está notarizado. Ve a **Ajustes del Sistema › Privacidad y seguridad › Abrir igualmente**.

Después de instalar, vuelve a escanear los plug-ins en tu DAW.

**Packs aparte** (Anthology Vol.1, AUSPICE y AUGURY Scenes): instala cada zip desde ⚙ › INSTALL PACK (.ZIP). Si ya tenías Anthology o AUSPICE, instala los zips nuevos: traen los volúmenes corregidos.
