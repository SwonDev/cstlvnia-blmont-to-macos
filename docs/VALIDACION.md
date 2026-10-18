# Validación de la construcción pública

Ejecución local del 18 de octubre de 2026 sobre Apple silicon, desde las fuentes exportadas de este repositorio. Revisión de implementación: `5731727`. Las entradas 1.1.0 se aportaron localmente; no se copió el AOT ni la caché SPIR-V de la instalación anterior.

| Frontera | Resultado observado |
|---|---|
| AOT | Generados `main`, `sdk` y `subsdk0`, con excepciones y perfil. 401.987, 40.112 y 14.173 funciones respectivamente. |
| Shaders | 5.000 inventariados, 4.999 SPIR-V válidos para Vulkan 1.2; los 4.774 requeridos completos. Dos preparaciones desde entradas propias producen los mismos binarios. |
| Compilación final | CMake/Ninja Release completo y repetición con SDL 3.4.18. 33 CTest sin fallos. |
| Python | Suite de 72 pruebas sin fallos; una prueba nativa opcional omitida en esa ejecución. Oráculo: 58 formas, 9.744 estados contra Unicorn. |
| Empaquetado nativo | Ejecución separada de las 22 pruebas, incluida la fixture con herramientas reales, firma y carga relocalizada de SDL/MoltenVK: todas pasan. |
| Checkout limpio | Contratos desde una copia separada de `5731727`, sin datos comerciales: 33 CTest y 71 pruebas Python sin fallos, una omitida. La prueba documental adicional pertenece a la actualización posterior. |
| App generada e instalada en QA | Recursos internos, icono de Control aportado, 4.999 shaders, dependencias internas y firma ad hoc estricta. Se instaló en un destino privado nuevo; no se reemplazó la app del usuario. |
| Recorrido de vídeo | 165 segundos solicitados, 166,8 observados; menú original, selección y aplicación de 1280 × 720 y modo de escritorio. Cliente Switch2Bridge real con servidor sintético aislado. Sin errores fatales ni shaders ausentes. |
| Transiciones medidas | 1.130 lecturas de fotogramas de esa ejecución; ninguna alternancia ABA/ABCA fuerte detectada por el observador. No es una certificación de todos los efectos. |
| Audio medido | 120 segundos de PCM del juego, estéreo a 48 kHz, señal no nula y ninguna muestra saturada. No se grabó el micrófono ni se certificó escucha por altavoces. |
| Partida adicional | Otros 80,9 s, escena del prólogo, Trevor y HUD observados en lectura GPU 3024 × 1701. Entrada sintética de movimiento y botones. Sin errores fatales ni shaders ausentes; cuatro PNG retenidos. |
| Recarga | Otros 20,4 segundos en frío, mismos ejecutable y sello, sin errores ni shaders ausentes. Recuperada la resolución 1280 × 720 desde las preferencias QA. |

Las capturas históricas se identifican en la [galería](GALERIA.md). La imagen del nuevo paquete documenta su propio recorrido; no se usa la imagen histórica para atribuirle un resultado nuevo.

## Identidad del primer paquete privado probado

```text
Paquete: 1.1.4, dev.swondev.castlevania, arm64
Contenido: 1.1.0
Ejecutable SHA-256:
835675dbc711f223dfc1a1a816be5ddc9947bd52165323a38766831ca65a9601
CodeResources SHA-256:
c222c3aa8ae3c1107c06ef639180c32d8a5101572826a165a85eebe2f0b37153
```

Herramientas observadas: Xcode 27.1, SDK macOS 27.0, Python 3.12.14, .NET 10.0.401, CMake 4.4.2, Ninja 1.13.2, SDL 3.4.18 y MoltenVK 1.4.2. El mínimo calculado de esta app es macOS 27.0. En otra máquina debe calcularse a partir de sus propios componentes.

## Revalidación del bootstrap de memoria

La revisión `7bb7f5d` evita asignaciones dinámicas del registro de reservas. La regresión que prohíbe `operator new` durante cada reserva falla con el registro anterior y pasa con el array fijo. El build completo vuelve a pasar 33 CTest y 72 pruebas Python, una omitida. Se creó e instaló otra app privada nueva, sin reemplazar la del usuario, y se probó después de firmarla:

```text
Ejecutable SHA-256:
bfdf04e832acc62378b73ca98802d236be634c3227cc81605765e2e05251ee3a
CodeResources SHA-256:
c222c3aa8ae3c1107c06ef639180c32d8a5101572826a165a85eebe2f0b37153
```

El recorrido adicional duró 166,6 s y conservó ocho PNG. Se observaron partida con Trevor, fondos y HUD, y el menú original con 1280 × 720; 1.116 fotogramas medidos no mostraron alternancias ABA/ABCA fuertes. No hubo errores fatales ni shaders ausentes. PCM: 120 s, 48 kHz estéreo, pico 9.338 y cero muestras saturadas. Recursos internos y directorio de ejecución ajeno al checkout. Otra recarga de 20,3 s recuperó el perfil QA, sin errores ni shaders ausentes. Las capturas públicas 13–14 siguen perteneciendo al primer paquete identificado arriba.

La [CI de esa revisión anterior](https://github.com/SwonDev/cstlvnia-blmont-to-macos/actions/runs/39156879599) pasó en Linux, pero falló en macOS 26 M1: 32/33 CTest pasaron; `mac_memory` rechazó un rango con dos regiones de 4 MiB `VM_MEMORY_MALLOC_SMALL`. La separación del test en un proceso nuevo y el registro sin allocator no bastaron. El guard de `operator new` no intercepta el allocator interno de macOS. Este resultado motivó la corrección siguiente.

## Corrección verificada en el runner M1

La revisión `c2d6e35` incorpora un segmento Mach-O ordinario `__GUEST`, zerofill, de 45 GiB desde 15 GiB: protección inicial y máxima cero, sin contenido en disco. El cargador lo reserva antes de las asignaciones de libSystem. Se aplica al juego y al test. Las direcciones invitadas y el AOT conservan su disposición.

Antes de sustituir un subrango, el runtime exige pertenencia a ese segmento del ejecutable actual tras ASLR y comprueba que la región siga sin acceso, protección máxima ni tag. El test verifica los seis rangos, mantiene vivos 16 MiB de asignaciones del host y contrasta después sus bytes. La regresión falla con código 22 sin el segmento y pasa con él. Diez arranques locales consecutivos pasan. El workflow actualizado añade esa repetición para la siguiente ejecución; el run identificado a continuación probó el test una vez.

La [ejecución completa 39158560322](https://github.com/SwonDev/cstlvnia-blmont-to-macos/actions/runs/39158560322) pasa en **ambas plataformas**: macOS 26 M1, 33/33 CTest y 72 pruebas Python sin omisiones, incluida la fixture nativa de empaquetado; Linux, 72 pruebas Python sin fallos, con doce omisiones correspondientes a pruebas específicas de macOS. No se excluyó el test que fallaba ni se permitió sobrescribir memoria ajena. Estos contratos no equivalen a ejecutar el juego comercial en el runner.

La app reconstruida y firmada se instaló en otro destino QA privado:

```text
Ejecutable SHA-256:
d07775117c492da8907f324bcd14b2d0b1e6b0a504fdb3d84b6e08ef35ada40e
CodeResources SHA-256:
c222c3aa8ae3c1107c06ef639180c32d8a5101572826a165a85eebe2f0b37153
```

El recorrido de 166,9 s mostró partida, HUD y opciones originales con 1280 × 720 y pantalla completa de escritorio. Se midieron 1.117 fotogramas sin alternancias ABA/ABCA fuertes; no hubo errores fatales ni shaders ausentes. PCM: 120 s, 48 kHz estéreo, pico 9.293 y cero muestras saturadas. Recarga posterior de 20,254 s, sin errores ni shaders ausentes. Recursos internos, ejecución fuera del checkout, perfil aislado y app canónica del usuario intacta. Las imágenes de juego de esta prueba se revisaron; las catorce capturas públicas conservan su procedencia anterior.

## Límites conservados

- Un programa geometry inventariado no está soportado por el traductor. No se afirma que nunca se use.
- El informe AOT declara dos `mov` y una `stg` estáticos sin implementar. Las pruebas no recorren todas las rutas ni niveles.
- Las lecturas GPU y la entrada sintética no prueban un Space visible, botones físicos, vibración ni calidad de escucha. La automatización de ventana nativa no estuvo disponible.
- El inventario de shaders requerido contrasta una frontera de RomFS, no todos sus recursos. Debe aportarse el RomFS coherente con la misma revisión.
- No hay notarización ni release binaria. La app y los logs completos contienen recursos o información privada y no se distribuyen.

La [metodología](METODOLOGIA.md) explica las puertas; [UPSTREAM](../UPSTREAM.md) fija procedencia. Este registro describe estas ejecuciones finitas y no promete ausencia universal de fallos.
