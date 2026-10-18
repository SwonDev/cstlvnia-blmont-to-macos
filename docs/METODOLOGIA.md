# Método para el siguiente juego

El resultado buscado es una app identificable y verificable, con sus capacidades y límites documentados. Empezar por una entrada concreta y un port existente reduce trabajo; cambiar el nombre de un paquete o sustituir recursos no crea compatibilidad.

## 0. Conservar el estado y estudiar lo que existe

Leer reglas del proyecto, README, manifiestos y decisiones; comprobar Git, cambios sin guardar y procesos activos. No reutilizar un directorio de build ocupado. Buscar primero port específico, runtime, adaptador y función ya existentes. Contrastar versiones instaladas con fuentes oficiales fijadas a revisión. Registrar la matriz del upstream antes de modificarlo.

**Artefactos:** ficha, revisiones, licencias, inventario de capacidades y registro de estado previo. **Puerta:** saber qué se adapta, qué se reutiliza y qué comportamiento debe conservarse. Un anuncio de otro port es evidencia de su autor, no una prueba local.

## 1. Identificar la entrada sin alterar originales

Registrar edición, arquitectura, motor, base, actualización, módulos y hashes en un inventario privado. Verificar que programa y recursos pertenecen a la misma revisión. Separar originales, herramientas, extracciones y resultados. Usar exclusivamente material cuyo uso esté autorizado y no incorporarlo al repositorio público.

Un extractor que termina con archivos vacíos no ha extraído correctamente. Un mensaje de corrupción también puede indicar una versión de formato no soportada. Comprobar capacidades y versiones antes de concluir que la entrada está dañada. No rebajar integridad para obtener una salida aparentemente válida. El estado de DLC se comprueba de forma independiente.

**Artefactos:** manifiesto de entrada, herramientas/versiones, resultado de integridad y composición. **Puerta:** entrada coherente verificada; detener la adaptación si faltan datos necesarios. Este cuaderno no facilita claves, firmware ni fuentes de descargas de juegos.

## 2. Elegir la frontera de adaptación

Comparar contratos de CPU, loader, servicios, gráficos, audio, vídeo, entrada, archivos y producto. Distinguir código reutilizable del host y políticas específicas del título. Mantener intacto el comportamiento original cuando pueda ejecutarse; adaptar su frontera con el anfitrión.

Fijar SHA upstream, herramientas, configuración y hashes de entrada. Toda dirección o wrapper específico exige revisión/hash y bytes o instrucciones esperados; fallar explícitamente ante otra entrada. Un hook de dispatch indirecto puede no cubrir llamadas C++ directas del emisor.

**Artefactos:** matriz de compatibilidad, perfil por revisión y plan de contratos. **Puerta:** no hay direcciones o reglas heredadas de otro juego sin verificar. Un runtime compatible con un juego no lo es automáticamente con otro.

## 3. CPU, memoria y servicios

Probar todos los módulos e inicializadores, reubicaciones compactadas, imports débiles y obligatorios, BSS, límites y rangos truncados. No usar marcos vacíos como funciones que se llamen recursivamente. Verificar saltos entre funciones y destino de tablas.

Compilar y ejecutar las formas de instrucciones usadas; contrastar registros, memoria y flags contra un oráculo independiente. Las excepciones necesitan sus destinos y personalidad originales; devolver éxito a un stub no conserva el contrato. Suspender hilos implica observar pausa, contexto y reanudación reales.

Inspeccionar regiones antes de reservar memoria. No pisar dyld, datos ni guardas. Distinguir páginas del invitado y del host, y garantizar ceros al reutilizar. Los servicios deben devolver estados y errores razonables, no éxitos universales. En archivos, montajes, límites de rutas, espacio libre y persistencia forman parte de la misma frontera.

**Artefactos:** pruebas de contratos, diferencias frente al oráculo y trazas acotadas. **Puerta:** entrada al programa con contratos necesarios ejecutados, sin confundirlo con una partida aceptada.

## 4. Gráficos de extremo a extremo

Inventariar shaders de recursos y del ejecutable; validar traducción, bindings, etapas y variantes. Añadir detección de ausentes durante recorridos: el catálogo estático puede estar incompleto. Mantener código traducido y metadatos coherentes. Un SPIR-V que pasa el validador puede leer el banco equivocado.

Probar copias CPU/GPU, strides, rangos, formatos, niveles, capas, vistas, volúmenes y dependencias. Los datos escritos por GPU no se sustituyen por una lectura antigua de CPU. Mismo layout no equivale a dependencia de memoria. Conservar identidad/generación de descriptores que el programa copia por valor. Capturar estado diferido al enviar, incluyendo longitudes y listas anidadas.

Comprobar primitivas, profundidad, stencil y programa por etapa. Si una cinemática muestra medio rectángulo, contrastar el tipo de primitiva antes de cambiar el decodificador. Revisar los píxeles del pase presentado, no sólo el backing buffer reservado.

**Artefactos:** inventario, validación de shaders, pruebas de comandos y lecturas GPU. **Puerta:** menús, escena, efectos y transiciones observados; «millones de draws» sin imagen no es éxito visual.

## 5. Resolución, color e interfaz

Medir por separado puntos de macOS, drawable físico, lienzo y efectos internos. Aumentar color y profundidad antes del cálculo de almacenamiento; actualizar ancho/alto de forma coherente. Conservar aspecto y recorte originales. No presentar una película de 1080p como detalle nuevo a resolución Retina.

Contrastar transferencia sRGB en el pase final antes de añadir curvas de brillo. Recuperar widgets originales si existen. Probar visibilidad, foco, lista de opciones, selección, setter, aplicación, backend de preferencias y recarga fría. No confundir una sobrecarga booleana con un enum de pantalla completa, ni un LocalizedString con una cadena normal.

En interfaces de escritorio comprobar coordenadas de cursor y hotspot, DPI, paneles abiertos, clic, arrastre con objeto visible, rueda y trackpad. Quitar controles táctiles incluye marcadores, estados de selección y rutas de eventos, no sólo botones visibles. Probar textos largos, español, escalas y ventanas estrechas.

**Artefactos:** tabla de tamaños/modos, capturas, recorridos de UI y cambios persistidos. **Puerta:** el usuario puede realizar y recuperar el cambio. Una fila visible o una llamada directa no lo demuestra.

## 6. Audio, película e idiomas

Verificar ABI de frecuencia/canales, búferes, formato de muestra, cola, finalización y consumo. Medir señal y saturación; contrastar voces concretas, música y efectos por separado. Reproducir por altavoces es otra prueba. Abrir mediante LaunchServices y comprobar la atribución al proceso de la app.

Para vídeo comprobar selección del códec, producción, consumo y presentación. Distinguir dimensiones codificadas de visibles, planos I420/NV12, strides, recorte, subtítulos, reloj y salto. No añadir dependencias al producto cuando sólo se necesitan para diagnóstico. Separar textos originales, textos del port y doblajes que realmente existen.

**Artefactos:** informe de señal acotado, capturas y escucha documentada. **Puerta:** imagen/audio y sincronía del recorrido; no declarar escucha a partir de PCM.

## 7. Entrada y persistencia

Probar el productor real y el efecto del evento: pulsación breve/mantenida, movimiento, liberación, cambio de foco y cancelación. En ratón comprobar quién es dueño del gesto hasta UP; una respuesta de red o cambio de estado no debe convertir un gesto en otro. Los defaults se comprueban en el perfil efectivo y tras reiniciar, no sólo en constantes del código.

Para el mando distinguir muestra conectada en reposo, protocolo sintético, consumidor real con servidor sintético y pulsación física. [Switch2Bridge](SWITCH2BRIDGE.md) documenta esa separación. No modificar el demonio o configuración global para ocultar un fallo del consumidor.

Un fixture debe contener una partida válida, no sólo preferencias. Registrar pantalla inicial y alcanzada. Guardar, cerrar y recargar; contrastar identidad de partida, progreso, reloj, inventario y ajustes. Un botón «Reanudar» no acredita recuperación del punto de control. Nunca promover guardados de QA al usuario.

**Artefactos:** fixture privado identificado, recorrido, cambios esperados y recarga. **Puerta:** acción y resultado observados; pendientes físicos declarados.

## 8. Diagnosticar sin tapar el fallo

Formular hipótesis falsables. Cambiar una causa cada vez; guardar baseline, candidata, hashes y resultado. Detener el experimento si falla el build. Limitar duración, series, imágenes y logs. Una sonda puede cambiar el timing: almacenar unas pocas muestras y volcarlas después puede ser más fiable.

Para parpadeo usar fotogramas consecutivos de la misma ejecución y revisar varios lags de recuperación. Cero muestras invalida la prueba. Filtrar por inicio de ejecución y no mezclar imágenes antiguas. El detector automático selecciona anomalías; el cambio de escena legítimo requiere revisión humana.

Localizar el primer pase o dato divergente. No arreglarlo omitiendo shaders, congelando salida o eliminando una transición. Cuando se restaura una sonda, asegurar que el build invalida los objetos anteriores, especialmente si se conservaron timestamps antiguos.

**Artefactos:** [experimento](../templates/EXPERIMENTO.md), causa, prueba que falla antes y pasa después, instrumentación retirada. **Puerta:** relación causal delimitada y regresión sobre la frontera real.

## 9. Empaquetar, instalar y aceptar

Terminar preparación, firmar y después probar, en ese orden. Incluir dependencias necesarias con rutas internas; recursos de sólo lectura y perfil escribible separados. Probar desde un directorio ajeno al checkout, sin variables de desarrollo ni fallback silencioso. Firma local, notarización y distribución pública son estados distintos.

Hash del ejecutable **y** sello de recursos después de firmar; los shaders pueden cambiar sin recompilar el código. Instalar con una única reversión coherente y preservar datos reales. Repetir las pruebas en la copia de Aplicaciones, comprobar identidad y arranque frío. No comparar el perfil tras jugar como si debiera seguir idéntico al previo: documentar los cambios esperados.

**Artefactos:** [aceptación](../templates/ACEPTACION.md), identidad instalada, preservación de datos, límites y reversión. **Puerta:** app instalada observada en sus capacidades concretas. El informe debe distinguir build, QA oculta, ventana visible y hardware físico.

## 10. Cerrar el ciclo de aprendizaje y espacio

Conservar una staging y una reversión, no paquetes completos por cada experimento. Retener por defecto pocas capturas PNG y métricas; eliminar sólo residuos identificados regenerables después de verificar la instalación. Medir espacio físico disponible antes/después: clones APFS impiden inferirlo del tamaño lógico.

Actualizar runbook, caso técnico y memoria con causa, evidencia, revisión y límites. Publicar desde una lista explícita en historial nuevo; [la puerta de privacidad](ENTREGA-Y-PRIVACIDAD.md) se aplica también a imágenes y metadatos Git. Reutilizar lecciones, no copiar direcciones, políticas o acreditaciones de otro título.

## De un resultado local a un port que otra persona puede compilar

Una app que funciona en el workspace no demuestra que las fuentes públicas reproduzcan ese resultado. La exportación de este port se construyó desde imágenes NSO propias nuevas, sin reutilizar el AOT ni importar la caché comercial anterior. Los artefactos generados permanecen en `private/`; sólo fuentes, perfiles, pruebas, avisos y capturas documentales seleccionadas entran en Git.

- Fijar identidad por módulo y receta de generación, incluidas direcciones base, excepciones, perfil, recompilador y dependencias. Un caché basado sólo en la existencia de `table.cpp` puede conservar código de una entrada distinta.
- Verificar el entorno virtual por `sys.prefix`: resolver el enlace del ejecutable puede devolver el mismo Python base y omitir accidentalmente la reejecución en el venv.
- Descubrir contenedores por formato además de extensión. Unity guarda recursos internos sin sufijo; un glob de `.assets` y `.bundle` omitía programas UI y vídeo.
- Fijar el conjunto requerido de shaders por una huella de hashes ordenados. La cardinalidad por sí sola no detecta que se reemplazó o perdió un origen. Conservar metadatos de recursos al consolidar variantes y declarar etapas no soportadas.
- Verificar el archivo fuente descargado y contrastar los archivos consumidos del checkout con él. Un marcador editable no acredita integridad. Serializar compilación y consumo de un harness que comparte `obj` y `bin`.
- Separar un feed que no responde de un compilador bloqueado. La restauración de este harness usa NuGet oficial explícitamente y conserva la fuente fijada y las auditorías.
- Probar los contratos sin datos comerciales también fuera de Release. La optimización ocultaba dos referencias de audio en el test standalone; se completaron sus stubs de prueba sin modificar la ruta real.
- Calcular el mínimo macOS del conjunto ejecutable/SDL/MoltenVK. Actualizar una biblioteca puede elevarlo; no copiar un mínimo del README anterior.
- Instalar una candidata nueva de forma exclusiva y comprobarla desde fuera del checkout, sin variables de desarrollo. Mantener perfil QA separado, un solo paquete y evidencia acotada.

El [README](../README.md) describe la cadena vigente; [UPSTREAM](../UPSTREAM.md) fija procedencia y licencias. El resultado de cada construcción debe indicar sus propias pruebas y límites.
