# Matriz de aceptación de una candidata

Plantilla sin resultados. Estados permitidos: **pasa, falla, no ejecutado, bloqueado, fuera de alcance**. Cada «pasa» necesita evidencia y el alcance exacto del instrumento.

| Identidad | Valor |
|---|---|
| Fecha y revisión upstream/propia | Sin determinar |
| Entrada, herramientas y configuración | Registro privado sin determinar |
| Bundle ID/build y arquitectura | Sin determinar |
| SHA-256 ejecutable firmado / sello | Sin determinar |
| Copia probada: build, staging o instalada | Sin determinar |
| Perfil QA, fixture y pantalla inicial | Sin determinar |
| Dispositivo e instrumento | Sin determinar |

| Capacidad | Resultado esperado | Estado | Evidencia / alcance |
|---|---|---|---|
| Contratos | Errores y límites, no sólo camino feliz | No ejecutado | |
| CPU | Estados contrastados contra oráculo | No ejecutado | |
| Inicio | Módulos y servicios necesarios reales | No ejecutado | |
| Imagen | Menús, escena, efectos y transiciones | No ejecutado | |
| Captura | Muestras consecutivas de esta ejecución, cantidad > 0 | No ejecutado | |
| Shaders | Ningún ausente durante el recorrido | No ejecutado | |
| Resolución/color | Píxeles finales y aspecto correctos | No ejecutado | |
| Opciones | Foco, selección, aplicar, descartar, recarga | No ejecutado | |
| Ratón | UI, mundo, arrastre visible, cancelación | No ejecutado | |
| Trackpad físico | Precisión, dos dedos, inercia cuando proceda | No ejecutado | |
| Mando sintético | Cliente real y efecto observado | No ejecutado | |
| Mando físico | Acciones concretas, foco y reconexión | No ejecutado | |
| Audio digital | Canales, frecuencia, señal y cola | No ejecutado | |
| Audio audible | Música, efectos y voces por dispositivo | No ejecutado | |
| Película | Imagen, audio, sincronía, subtítulos y salto | No ejecutado | |
| Guardado | Commit, cierre, recarga y punto recuperado | No ejecutado | |
| Instalación | Identidad y dependencias internas, sin fallback | No ejecutado | |
| LaunchServices | Ventana, icono y atribución de audio | No ejecutado | |
| Ventanas | Redimensionar, maximizar, Space y restauración | No ejecutado | |
| Perfil real | Preservación antes del uso, cambios legítimos después | No ejecutado | |
| Reversión | Conjunto coherente recuperable | No ejecutado | |
| Publicación | Allowlist, historial y capturas revisados | No ejecutado | |

## Dictamen

Registrar capacidades aceptadas y pendientes. Un resultado de protocolo no asciende a prueba física; una lectura GPU no acredita un Space visible; una firma válida no acredita una partida. No usar «perfecto» como estado de la matriz.
