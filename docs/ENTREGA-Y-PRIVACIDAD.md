# Entrega, datos y publicación

Una entrega debe relacionar entrada, fuente, candidata, recursos y copia instalada. Publicar el aprendizaje es una operación diferente de distribuir un juego. Esta guía usa recibos y comprobaciones acotadas; ningún escáner sustituye la revisión de procedencia y contenido.

## Recibo privado por candidata

Registrar fecha, SHA upstream, cambios propios, herramientas/versiones, configuración y hash de la entrada. Después de firmar: bundle ID, versión, arquitectura, ejecutable, sello de recursos y dependencias. Registrar por separado notarización, si existe; una firma ad hoc válida no la implica.

Cada prueba declara perfil, estado inicial, hora de inicio, duración, imágenes realmente medidas, instrumento y resultado. Relacionar el recibo con la [matriz](../templates/ACEPTACION.md), sin adjuntar datos personales. Los hashes son identificadores, no prueba funcional.

## QA aislada antes de la copia real

1. Inventariar y proteger los datos existentes. Comprobar rutas efectivas y procesos que los usan.
2. Crear un perfil de QA propio. Confirmar que contiene el estado necesario; preferencias solas no son un guardado.
3. Ejecutar la candidata con recursos internos desde un directorio independiente, sin variables de desarrollo.
4. Revisar píxeles, acciones y cambios persistidos. Separar pruebas ocultas y visibles.
5. Instalar únicamente después de terminar la preparación y firma. Conservar una reversión coherente de la versión previa.
6. Contrastar identidad instalada y repetir un recorrido frío con perfil QA. Abrir por LaunchServices para la comprobación visible y atribución de audio.
7. Preservar los datos reales. Aplicar ajustes con la interfaz original cuando corresponda; no copiar progreso de QA. Comparar antes/después y explicar los cambios legítimos tras jugar.

La reversión incluye todos los archivos que deben mantenerse juntos: ejecutable, plist, sello y metadatos de recursos. Si los recursos cambiaron, también deben poder recuperarse. No restaurar sólo un ejecutable antiguo dentro de un bundle nuevo.

## Presupuesto de evidencia y disco

Fijar un máximo de segundos, imágenes retenidas, audio y tamaño de log antes de iniciar. En el caso local se adoptaron 24 PNG por defecto, rango de 4–64 y audio limitado a 120 segundos. Los análisis de fotogramas pueden consumir más muestras en memoria sin retenerlas todas en disco.

Conservar una staging y una reversión. Revisar rutas exactas antes de retirar productos regenerables. Proteger originales, fuentes, Git, recursos propios, guardados, secretos y evidencia necesaria. Medir disponibilidad física con `statvfs` o `df`; en APFS, `du` no prueba los bytes realmente recuperados. No vaciar cachés o carpetas desconocidas de forma indiscriminada.

## Exportación pública por lista explícita

Crear un directorio nuevo con historial nuevo; no hacer público el workspace privado ni filtrarlo sólo con `.gitignore`. Copiar únicamente archivos aprobados individualmente. Las extensiones de texto no garantizan que un archivo sea publicable.

| Revisión | Comprobación |
|---|---|
| Procedencia | Material original o licencia/permiso y avisos comprobados. No inferir derechos porque algo esté en GitHub. |
| Inventario | Lista exacta de archivos, tamaños, tipos y hashes. Ningún symlink o submódulo que abra una ruta privada. |
| Texto | Sin nombres/rutas personales, correos privados, tokens, credenciales, identificadores de dispositivo ni enlaces a recursos privados. |
| Contenido | Sin ROM, firmware, claves, tickets, guardados, programas extraídos, código AOT generado, shaders de juego ni dumps. |
| Imágenes | Sólo juego, sin escritorio/overlay personal; revisión visual y de chunks PNG/EXIF/XMP/texto. Mantener su contexto y derechos. |
| Git | Revisar staged y todos los objetos del historial nuevo; autor público y correo noreply. No incluir historial privado. |
| Remoto | Verificar árbol, README, enlaces e imágenes después del push. No publicar releases binarias por defecto. |

Las capturas seleccionadas se conservan localmente y no se publican en este cuaderno. La autorización del usuario para preparar la galería no equivale a permiso del editor. Las [directrices de KONAMI](https://www.konami.com/games/castlevania/belmonts_curse/eu/en/guideline/) no cubren expresamente GitHub; resolver la base aplicable antes de incorporar esos PNG. Un nombre genérico no elimina los derechos de terceros ni garantiza ausencia de reclamaciones. No presentar el proyecto como oficial.

## Cuando una comprobación no puede hacerse

Registrar bloqueo e instrumento, preservar el entorno del usuario y completar las pruebas independientes. Un fallo de automatización no demuestra un fallo del juego, pero tampoco permite declarar la ventana visible aceptada. El estado correcto es «no ejecutado» o «bloqueado», con su alcance concreto.
