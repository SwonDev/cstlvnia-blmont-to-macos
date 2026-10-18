# Castlevania Belmont’s Curse for macOS

[![Pruebas del port](https://github.com/SwonDev/cstlvnia-blmont-to-macos/actions/workflows/docs.yml/badge.svg?branch=main)](https://github.com/SwonDev/cstlvnia-blmont-to-macos/actions/workflows/docs.yml)

Port no oficial de **Castlevania: Belmont’s Curse** para **macOS en Apple silicon**. Incluye el recompilador AArch64 → C++, el runtime adaptado, la capa gráfica NVN → Vulkan/MoltenVK, las adaptaciones del juego y las herramientas para construir una app local a partir de tu propia copia.

Repositorio: [SwonDev/cstlvnia-blmont-to-macos](https://github.com/SwonDev/cstlvnia-blmont-to-macos). Contenido compatible: **1.1.0**; versión del paquete local: **1.1.4**. Los build ID y hashes admitidos están en [games/castlevania/build.json](games/castlevania/build.json). Un ExeFS de otra revisión se rechaza antes de generar código. RomFS debe proceder de la misma 1.1.0: se exige `Data/` al preparar y se contrasta el conjunto requerido de shaders después; ese inventario no valida todos los recursos del juego.

**El repositorio no incluye el juego ni una app descargable.** Debes aportar ExeFS, RomFS e icono de Control de tu propia copia. El código AOT, los programas SPIR-V derivados del juego, los paquetes, las entradas y los logs permanecen en `private/`. No se publican ROM, claves, firmware ni partidas. Las capturas documentales conservan los derechos de sus titulares y están excluidas de MIT.

![Partida y HUD originales de Castlevania](assets/screenshots/14-partida-compilacion-publica.png)

*Lectura GPU del prólogo en la app generada por el pipeline público. [Galería de catorce capturas: doce históricas y dos de esta construcción](docs/GALERIA.md).*

## Estado de la construcción pública

La adaptación privada anterior mostró menú, cinemáticas, prólogo, HUD, diálogo, equipamiento y opciones originales. Esta publicación incorpora su código y sustituye las rutas del workspace privado por una preparación explícita de entradas propias.

| Frontera | Estado verificado al 18 de octubre de 2026 |
|---|---|
| Runtime y contratos | 33 CTest y 9.744 estados de instrucciones contra Unicorn ejecutados de nuevo en la revisión pública. Son pruebas acotadas. |
| Generación AOT | Regenerada desde las entradas locales para `main`, `sdk` y `subsdk0`. No se publica el resultado generado. |
| Shaders desde cero | 5.000 programas inventariados; 4.999 SPIR-V válidos. Los 4.774 de la caché histórica coinciden byte por byte; hay 225 válidos adicionales. |
| Límite del traductor | Un programa geometry no está soportado. No se declara que carezca de uso en todo el juego. |
| Build completo desde el pipeline público | Terminado desde las entradas propias; 33 CTest y suite de 72 pruebas Python sin fallos (una omitida). |
| App generada por ese pipeline | Empaquetada, firmada e instalada en QA aislada. Recorrido de vídeo de 165 s, 1.130 muestras sin alternancias detectadas, recarga en frío y otros 80 s de partida observada. |

Los informes AOT conservan tres instrucciones estáticas sin implementar (dos `mov` en `main` y una `stg` en `sdk`); su presencia limita la cobertura y no se presenta como soporte completo de todas las rutas.

El empaquetador tiene pruebas con entradas propias pequeñas, herramientas reales de macOS, firma estricta y carga de SDL3/MoltenVK después de mover el bundle. Eso verifica el empaquetado, no sustituye la prueba del juego. La [validación pública](docs/VALIDACION.md) detalla las ejecuciones de esta candidata. Los recorridos históricos, sus límites y el método están en [docs/CASO-CASTLEVANIA.md](docs/CASO-CASTLEVANIA.md) y [docs/METODOLOGIA.md](docs/METODOLOGIA.md).

## Requisitos

- Mac con Apple silicon y herramientas de Xcode, incluido el SDK de macOS.
- Python **3.12 o posterior**, CMake y Ninja.
- SDL3, MoltenVK, cabeceras y loader de Vulkan, SPIRV-Tools (`spirv-val`).
- SDK **.NET 10** para compilar el traductor de shaders.
- Espacio para los datos propios, el C++ generado, los objetos de compilación y una copia de recursos dentro de la app. La generación y compilación son costosas; empieza con cuatro trabajos paralelos.

Con Homebrew ya instalado:

```sh
xcode-select --install
brew install python@3.12 cmake ninja sdl3 molten-vk vulkan-headers vulkan-loader spirv-tools dotnet
```

La versión mínima de macOS de la app depende de los componentes que empaquetes: se toma el mayor `minos` de su ejecutable, SDL3 y MoltenVK. En el build local final, el ejecutable y SDL3 3.4.18 exigen **macOS 27**; MoltenVK 1.4.2 admite 12. No se promete compatibilidad con sistemas anteriores sin construir y probar las dependencias correspondientes.

## Construir con tu propia copia

Clona las fuentes y comprueba las herramientas:

```sh
git clone https://github.com/SwonDev/cstlvnia-blmont-to-macos.git
cd cstlvnia-blmont-to-macos
./build-macos.sh --doctor
```

Prepara estos tres argumentos; el repositorio no obtiene ni descifra el contenido por ti:

| Entrada | Qué debe contener |
|---|---|
| `--exefs` | Directorio ExeFS descifrado con los NSO `main`, `sdk` y `subsdk0` de 1.1.0. |
| `--romfs` | Raíz de RomFS descifrado, que contiene `Data/`; no indicar directamente esa subcarpeta. |
| `--icon` | Imagen del icono de tu Control/NACP. Se convierte localmente a `.icns`; no hay un icono comercial embebido. |

Ejecuta el pipeline completo para compilar e instalar una app nueva en `~/Applications` (omite `--install` si prefieres dejarla en `private/dist/`):

```sh
./build-macos.sh \
  --exefs "/ruta/a/tu/ExeFS" \
  --romfs "/ruta/a/tu/RomFS" \
  --icon "/ruta/a/tu/icono-Control.dat" \
  --language es \
  --jobs 4 \
  --stage all \
  --install
```

`--language` admite `es`, `en` y `ja`; selecciona el idioma del juego sin añadir traducciones externas. El entorno Python se crea en `private/venv/` con dependencias fijadas y hashes. La fuente MIT del traductor se descarga a `private/third_party/` desde una revisión y un archivo con SHA-256 fijados; no se descarga ningún recurso del juego. Consulta [UPSTREAM.md](UPSTREAM.md).

La preparación registra las rutas locales en `private/input.json`, extrae las imágenes de los NSO a `private/castlevania/analysis/` y crea un enlace gestionado hacia tu RomFS. Conserva ese RomFS disponible y no cambies de revisión bajo el mismo checkout. Las siguientes etapas reutilizan la configuración; `--language` tiene por defecto `es`, así que repítelo si elegiste otro idioma.

## Ejecutar por etapas y revisar fallos

```sh
./build-macos.sh --stage prepare --exefs "/ruta/a/tu/ExeFS" --romfs "/ruta/a/tu/RomFS" --icon "/ruta/a/tu/icono-Control.dat"
./build-macos.sh --stage generate --jobs 4
./build-macos.sh --stage shaders --jobs 4
./build-macos.sh --stage build --jobs 4
./build-macos.sh --stage package
```

| Etapa | Resultado |
|---|---|
| `prepare` | Valida revisión e imágenes; prepara datos privados y el idioma. |
| `generate` | Genera C++ AOT para los tres módulos, con excepciones y el perfil de resolución. |
| `shaders` | Descubre, traduce y valida programas desde ExeFS/RomFS propios. |
| `build` | Reutiliza o genera AOT, compila con CMake/Ninja y ejecuta las pruebas. No ejecuta la etapa de shaders. |
| `package` | Empaqueta el ejecutable y la caché ya preparados, con recursos internos y firma ad hoc. |
| `test` | Construye y ejecuta contratos CTest y pruebas Python sin aportar datos comerciales. |
| `all` | Preparación, AOT, shaders, compilación, pruebas y paquete, en ese orden. |

Para ejecutar sólo los contratos:

```sh
./build-macos.sh --stage test --jobs 4
```

Una etapa fallida no equivale a una app terminada. Conserva la salida de la herramienta y los informes privados de shaders; no copies una caché anterior para ocultar un fallo de traducción. `--help` muestra las opciones de la revisión que tengas instalada:

```sh
./build-macos.sh --help
python3.12 scripts/macos_bundle.py --help
```

## Paquete e instalación opcional

La salida predeterminada es `private/dist/Castlevania Belmont’s Curse.app`, con identificador `dev.swondev.castlevania` y versión 1.1.4. Incluye los recursos aportados, el icono, SDL3, MoltenVK y sus avisos de licencia, además de los de SuperMarioWonderRecomp, Ryubing y FidelityFX. El empaquetador ajusta las dependencias a rutas internas y ejecuta `codesign --verify --deep --strict`. La firma es local ad hoc; no constituye notarización de Apple.

La construcción sin `--install` no instala. Si sólo has ejecutado la etapa `build` y todavía no existe una app en `private/dist/`, puedes empaquetar e instalar así:

```sh
./build-macos.sh --stage package --install-dir "$HOME/Applications"
```

`--install` sin directorio utiliza también `~/Applications`. El instalador mueve únicamente la app nueva y rechaza una app ya existente o un destino simbólico; no sustituye una instalación previa ni modifica sus partidas. La salida privada debe ser nueva: si ya existe una candidata, consérvala y usa otro `--output` con `scripts/macos_bundle.py`, o un checkout independiente. Los metadatos y logs del paquete permanecen en `private/`.

Si ya ejecutaste `all` sin instalar, la app terminada está en `private/dist/`: puedes moverla con Finder a una carpeta de aplicaciones libre. No repitas `package` sobre ese mismo destino. Abre la app desde Finder una vez creada. Los recursos se leen dentro del bundle; los datos modificables de la app empaquetada se guardan mediante `SDL_GetPrefPath("SwonDev", "castlevania")`, fuera de éste. No se importan automáticamente partidas del workspace privado. Una app construida con tu copia contiene datos del juego: no la adjuntes a releases, issues o PR.

## Vídeo, Retina y controles

El port mantiene los menús, textos y widgets originales. El selector de resolución ofrece modos 16:9 válidos y una opción nativa ajustada al panel: por ejemplo, un panel de 3024 × 1964 usa un lienzo de **3024 × 1701**. Los puntos de ventana, los píxeles Retina y la resolución interna son magnitudes distintas. El vídeo original conserva su resolución de origen.

Los ajustes originales conservan aplicar, descartar y cerrar. El modo exclusivo reservado a Windows se adapta a pantalla completa de escritorio en macOS; ventana, maximización y pantalla completa nativa se gestionan por SDL/AppKit. El menú **Vídeo** de macOS muestra la salida en píxeles y permite cambiar el modo de ventana. Una lectura GPU o una prueba headless no certifica por sí sola un Space visible.

Este perfil utiliza el consumidor Switch2Bridge y la entrada de teclado del runtime, conservando posiciones y glifos Nintendo. El mando SDL genérico no está habilitado como alternativa en este perfil. Para el Switch 2 Pro se integra directamente [SwonDev/Switch2Bridge](https://github.com/SwonDev/Switch2Bridge); usa las instrucciones de ese proyecto para el puente. Este consumidor no instala un mando virtual global ni requiere desactivar SIP. La vibración del consumidor nativo y la cobertura completa de botones físicos siguen pendientes. [Protocolo y pruebas](docs/SWITCH2BRIDGE.md).

## Código, método y créditos

- [UPSTREAM.md](UPSTREAM.md): base fijada, traductor y fronteras de procedencia.
- [CREDITS.md](CREDITS.md): autores, herramientas, licencias y titulares del juego.
- [docs/GALERIA.md](docs/GALERIA.md): catorce capturas contextualizadas, históricas y del nuevo paquete.
- [docs/METODOLOGIA.md](docs/METODOLOGIA.md): método de adaptación y validación.
- [CONTRIBUTING.md](CONTRIBUTING.md): cómo aportar causas, pruebas y límites sin material privado.

La base es **SuperMarioWonderRecomp**, de BelmanteGu y contribuidores, bajo MIT. **D2R Mobile**, de jhaizhou-ops y contribuidores, fue el antecedente del método; no es la base del runtime de este port. Su [PR #87](https://github.com/jhaizhou-ops/d2r-mobile/pull/87) se cita como trabajo relacionado y no implica aprobación upstream.

El código del proyecto y las modificaciones originales se publican bajo [MIT](LICENSE), respetando las licencias de terceros. **Castlevania: Belmont’s Curse**, su contenido, imágenes y marcas pertenecen a KONAMI y a los demás titulares indicados por el juego. Las capturas se incluyen para documentar la adaptación y quedan excluidas de MIT. Proyecto independiente, sin afiliación, patrocinio ni aprobación de KONAMI, Nintendo o Apple. [Sitio oficial del juego](https://www.konami.com/games/castlevania/belmonts_curse/eu/en/).
