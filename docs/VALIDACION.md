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

## Identidad del paquete privado probado

```text
Paquete: 1.1.4, dev.swondev.castlevania, arm64
Contenido: 1.1.0
Ejecutable SHA-256:
835675dbc711f223dfc1a1a816be5ddc9947bd52165323a38766831ca65a9601
CodeResources SHA-256:
c222c3aa8ae3c1107c06ef639180c32d8a5101572826a165a85eebe2f0b37153
```

Herramientas observadas: Xcode 27.1, SDK macOS 27.0, Python 3.12.14, .NET 10.0.401, CMake 4.4.2, Ninja 1.13.2, SDL 3.4.18 y MoltenVK 1.4.2. El mínimo calculado de esta app es macOS 27.0. En otra máquina debe calcularse a partir de sus propios componentes.

## Límites conservados

- Un programa geometry inventariado no está soportado por el traductor. No se afirma que nunca se use.
- El informe AOT declara dos `mov` y una `stg` estáticos sin implementar. Las pruebas no recorren todas las rutas ni niveles.
- Las lecturas GPU y la entrada sintética no prueban un Space visible, botones físicos, vibración ni calidad de escucha. La automatización de ventana nativa no estuvo disponible.
- El inventario de shaders requerido contrasta una frontera de RomFS, no todos sus recursos. Debe aportarse el RomFS coherente con la misma revisión.
- No hay notarización ni release binaria. La app y los logs completos contienen recursos o información privada y no se distribuyen.

La [metodología](METODOLOGIA.md) explica las puertas; [UPSTREAM](../UPSTREAM.md) fija procedencia. Este registro describe estas ejecuciones finitas y no promete ausencia universal de fallos.
