# Tonal Lab Instruments — logo

**Concepto:** un sol a medio ponerse sobre el horizonte. La mitad izquierda es atardecer (naranja) y la derecha, noche (azul luna); abajo van las ondas del horizonte de los plugins. Las letras están dibujadas como trazos geométricos (no dependen de ninguna fuente).

| Archivo | Uso |
|---|---|
| `tonal-lab-logo-horizontal-dark-bg` | Encabezado de la web, banners y la ventana "About" de los plugins (fondo oscuro) |
| `tonal-lab-logo-horizontal-light-bg` | Documentos, facturas y fondos claros |
| `tonal-lab-logo-stacked-*` | Pantalla de inicio, portadas y redes |
| `tonal-lab-mark-brand` | Ícono solo (esquinas del plugin, favicon, avatar) |
| `tonal-lab-mark-brand-light-bg` | Ícono sobre fondos claros |
| `tonal-lab-mark-dusk` / `-night` | Variante cálida para la familia atardecer (MANTIS) y fría para la familia noche (PYTHIA) |
| `tonal-lab-mark-mono-white` / `-black` | Una sola tinta: grabados, sellos, impresión |
| `tonal-lab-app-icon` | Ícono de app o redes sociales (cuadro redondeado) |

Cada logo viene en SVG (vector, escala sin perder calidad) y en PNG con fondo transparente a 4x.

## En los plugins (JUCE)

Agrega el SVG a `juce_add_binary_data` y dibújalo:

```cpp
auto logo = juce::Drawable::createFromImageData (BinaryData::tonallabmarkbrand_svg,
                                                 BinaryData::tonallabmarkbrand_svgSize);
logo->drawWithin (g, area.toFloat(), juce::RectanglePlacement::centred, 1.0f);
```

## Colores

Naranja `#F2913A` · Ámbar `#FFC07A` · Azul hielo `#BCD6FF` · Azul `#5B9BFF` · Tinta clara `#E8E6E1` · Tinta oscura `#14161B`
