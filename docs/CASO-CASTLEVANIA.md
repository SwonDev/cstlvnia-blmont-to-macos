# Caso técnico: Castlevania en Apple silicon

Adaptación local de contenido 1.1.0, paquete macOS 1.1.4. Base del runtime: [SuperMarioWonderRecomp, revisión 010ed7a](https://github.com/BelmanteGu/SuperMarioWonderRecomp/tree/010ed7a596a621c15d3a8dfa5795ca8c3668b066). No se atribuye al upstream compatibilidad previa con este título. Este documento conserva los resultados históricos de la adaptación. La implementación se distribuye ahora en este repositorio; los recursos del juego los aporta cada usuario. Consulta la [compilación](../README.md) y la [procedencia](../UPSTREAM.md).

## De carga a imagen

El SDK requirió procesar todos los módulos, DT_INIT, imports débiles y DT_RELR. Importar correctamente no bastaba: faltaban reubicaciones. Los marcos de excepción vacíos no eran límites válidos de funciones. Las instrucciones se contrastaron ejecutando estados contra Unicorn, no contando únicamente opcodes reconocidos.

El antiguo rango de memoria colisionaba con dyld. Se inspeccionaron las regiones y se limitaron sustituciones a reservas vacías, preservando guardas y páginas vecinas. En el sistema de archivos se completaron montajes, consultas de rutas y espacio libre. El reloj compartido moderno necesitaba contexto/UUID coherente para evitar fechas de 1970.

La imagen llegó después de corregir identidad de descriptores copiados por valor, reposición de memoria de listas y copias realmente usadas por GPU. El modo de cuadriláteros se expandió a dos triángulos. Se mantuvo programa por etapa, porque limpiar cómputo no debía borrar VS/FS. Los bancos de constantes y handles de texturas exigían el contrato del SDK de esta revisión.

## Vídeo y audio

La decodificación H.264 original por software producía imágenes antes de que se viesen correctamente. La fuente tenía 1088 líneas codificadas y 1080 visibles: localizar U/V usando las primeras y empaquetar NV12 usando las segundas corrigió color y franja. No se incorporó el códec usado sólo como referencia de diagnóstico al producto.

La salida de audio requirió frecuencia, canales, disposición de búferes y ABI correctos. La señal documentada es PCM16, estéreo, 48 kHz. Una prueba anterior capturó 111,85 segundos sin saturación; eso no certifica voces por altavoces. El juego ofrece textos españoles y voces originales inglesas/japonesas, según [KONAMI](https://www.konami.com/games/castlevania/belmonts_curse/eu/en/).

## Tres causas diferentes en las transiciones

| Causa | Evidencia | Corrección y prueba |
|---|---|---|
| Capa inicial de TextureView ignorada | Las escrituras relativas a la vista pisaban la capa 0 de una máscara de 16 capas. | Incorporar nivel/capa/región de la vista. Probar capas distintas y límites. |
| Primer índice de material obsoleto | Una traza diferida capturó 48 antes de 0, igual en CPU y upload, con la misma textura/generación. | Preparar el material antes de publicarlo y llamar después al cuerpo original; respetar inversión e índice original. |
| Copias solapadas sin dependencia WAW | La transición a un layout idéntico no emitía barrera entre escritura base y parches. | Dependencia TRANSFER_WRITE a TRANSFER_WRITE antes de los parches; interceptar las llamadas reales y validar regiones/volumen/layout. |

La regla de sincronización se contrastó con [Khronos](https://github.khronos.org/Vulkan-Site/spec/latest/chapters/synchronization.html). No se sustituyó la corrección por eliminar dibujos. La instrumentación específica se retiró antes de la entrega.

## Resolución e interfaz original

Se distinguieron puntos, drawable, lienzo, color y profundidad; se reservaron tamaños coherentes. El panel de 3024 × 1964 usa un lienzo 16:9 de **3024 × 1701**. La transferencia sRGB final corrigió tonos medios oscuros; no se añadió una curva de aclarado arbitraria.

Los selectores originales de Unity existían, pero requerían opciones válidas, foco, sobrecarga correcta de FullScreenMode y el backend propio de preferencias. Se preservaron widgets y traducciones. X aplica, Y descarta, B cierra. La prueba cambió a 1280 × 720, aplicó el modo de escritorio y recuperó resolución en frío. El modo headless sincroniza su host diagnóstico; no prueba un Space visible de macOS.

## Qué se probó y qué queda fuera

| Comprobación | Resultado local reportado | Límite |
|---|---|---|
| Contratos | 33 CTest y 19 pruebas Python | Fronteras cubiertas, no todo el programa. |
| Instrucciones | 9.744 estados de 58 formas contra Unicorn | Entradas sintéticas seleccionadas. |
| Shaders | 4.774 programas; variante UI adicional validada | No demuestra que todos los efectos futuros se hayan visto. |
| Paquete final | 165 s, 1.130 fotogramas, sin ABA detectado ni shaders ausentes | Recorrido concreto de ajustes/partida. |
| App instalada | Otros 165 s y 1.130 fotogramas, misma identidad | Lectura GPU y entrada sintética. |
| Persistencia | Resolución recuperada tras 20 s de arranque frío | Space visible no certificado. |
| Datos reales durante actualización | Seis archivos idénticos antes/después | Son archivos de perfil, no seis partidas. |
| Guardado | Identidad estable y tiempo creciente, fecha corregida | Punto inicial del prólogo; falta otro punto posterior. |
| Hardware y ventana | Usuario confirmó buena jugabilidad; automatización nativa no disponible | Altavoces, mando físico y Space no certificados por esas pruebas. |

La automatización de ventana falló con `native pipe startup failed`; no se reinició la sesión del usuario ni se alteraron permisos para sortearla. La página de la [galería](GALERIA.md) presenta las catorce capturas publicadas; sus leyendas distinguen resultados históricos de la candidata final.

## Identidad de referencia

```text
Versión de paquete: 1.1.4
Contenido: 1.1.0
SHA-256 ejecutable firmado:
c74e829b8db84f290cbd8b4914604047fe02825c72775437e8e115b0e118b164
SHA-256 sello CodeResources:
cb40c187dca13230086a192fb60c710ae8bab206259657e37262d344e02d376d
```

Son identificadores del artefacto privado observado, no enlaces de descarga. Los números proceden de informes locales fechados el 18 de octubre de 2026. Estos resultados históricos no sustituyen una ejecución propia. El repositorio permite generar una nueva candidata desde fuentes y entradas propias; su identidad diferirá de este artefacto privado de referencia. Los logs completos, guardados y recursos comerciales permanecen privados.
