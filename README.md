# Native Porting Fieldnotes

Metodología de adaptación y verificación de juegos en **macOS y Apple silicon**, basada en problemas encontrados durante dos adaptaciones locales. Desde la identidad de la entrada hasta la app instalada: gráficos, audio, controles, persistencia y pruebas que acreditan lo que realmente se ha observado.

**Este repositorio publica documentación, plantillas y un verificador de documentos.** No incluye un juego descargable, un runtime listo para compilar ni un conversor universal. Las implementaciones de referencia están enlazadas en [Créditos](CREDITS.md). El caso local puede funcionar sin que este repositorio permita reproducirlo por sí solo.

Se preparó una galería local de doce capturas de Castlevania. **Los PNG no se publican aquí**: las directrices del editor no incluyen GitHub entre las plataformas autorizadas y hace falta resolver la base aplicable a estas imágenes. [Estado de la galería y fuente oficial](docs/GALERIA.md).

## Empezar con otro proyecto

1. Consulta [proyectos relacionados](docs/PROYECTOS-RELACIONADOS.md). Si ya existe un port mantenido que cubra tu entrada, evalúalo antes de crear otro.
2. Copia la [ficha de evaluación](templates/PROYECTO.md) a tu espacio privado. Fija versión, hashes, plataforma y capacidades necesarias.
3. Sigue la [metodología por fases](docs/METODOLOGIA.md). Cada fase tiene artefactos y condición de paso.
4. Usa la [matriz de aceptación](templates/ACEPTACION.md) y el [registro de experimentos](templates/EXPERIMENTO.md). Un build, un evento de entrada o un contador de presentaciones no prueban una partida correcta.
5. Antes de instalar o publicar, pasa las puertas de [entrega y privacidad](docs/ENTREGA-Y-PRIVACIDAD.md).

No necesitas instalar dependencias para leer o reutilizar las plantillas. Las adaptaciones de código requieren la cadena de herramientas y los recursos propios de cada proyecto.

## Qué se ha aprendido

| Área | Lección práctica |
|---|---|
| CPU y carga | Resolver imports no basta: comprobar reubicaciones, inicializadores, excepciones, memoria y semántica de instrucciones. |
| Gráficos | Un shader válido puede recibir datos incorrectos. Probar capas, vistas, copias, bindings y presentación. |
| Pantallas Retina | Separar puntos de macOS, drawable, lienzo y resolución interna. Color y profundidad deben admitir el mismo tamaño. |
| Interfaz original | Probar foco, geometría, selección, aplicación y recarga. Mostrar un selector no demuestra que funcione. |
| Audio y vídeo | Medir señal, canales, frecuencia, sincronía y cola; escuchar por separado. Decodificar no demuestra presentar. |
| Entrada | Probar el recorrido del gesto y su efecto. Conexión del mando y entrada sintética no certifican botones físicos. |
| Instalación | Identificar ejecutable y recursos después de firmar; repetir las pruebas sobre la app instalada con perfil aislado. |
| Datos y disco | Conservar partidas y fuentes, acotar capturas y paquetes, medir recuperación física. |

Detalles: [caso Castlevania](docs/CASO-CASTLEVANIA.md), [aprendizajes de D2R](docs/ANTECEDENTE-D2R.md) y [mando con Switch2Bridge](docs/SWITCH2BRIDGE.md).

## Resultado local documentado

La adaptación local de **Castlevania: Belmont’s Curse**, contenido 1.1.0 y paquete macOS 1.1.4, produjo imagen, cinemática, partida e interfaz originales. Las pruebas locales registraron 33 CTest, 19 pruebas Python y 9.744 estados de instrucciones contrastados. Dos recorridos de 165 segundos midieron 2.260 fotogramas sin alternancias bruscas detectadas ni shaders ausentes. Se verificó cambio y recuperación de resolución desde el menú original.

Son **resultados reportados de recorridos concretos**, no una certificación de todos los niveles. Altavoces, acciones del mando físico, Space visible y recuperación desde un punto posterior del prólogo siguen fuera de esa cobertura. La evidencia bruta conserva datos privados y no se distribuye. [Alcance y trazabilidad](docs/CASO-CASTLEVANIA.md).

El lienzo nativo ajustado documentado es 3024 × 1701. Las lecturas GPU de documentación se guardaron a menor tamaño; no se confunde esa resolución del PNG con la resolución renderizada.

## Switch2Bridge

El mando usa nuestro proyecto [SwonDev/Switch2Bridge](https://github.com/SwonDev/Switch2Bridge). El consumidor nativo de esta adaptación lee su socket directamente; el soporte publicado del puente sigue centrado en Wine/SDL. Esta prueba no convierte todos los juegos nativos en compatibles. [Integración, protocolo y límites](docs/SWITCH2BRIDGE.md).

## Por qué otro repositorio de documentación

Ya existen metodologías, starters y runtimes relacionados: Apple Game Porting Toolkit, Retro Porting Toolkit y SwitchRecomp. **No reivindicamos haber inventado estas fases.** Este cuaderno complementa esas referencias con dos casos, causas verificadas, falsas pistas y puertas de aceptación macOS. No duplica sus motores ni copia sus plantillas. La [investigación fechada](docs/PROYECTOS-RELACIONADOS.md) explica el solapamiento.

## Créditos y contribuciones

Gracias a **jhaizhou-ops y contribuidores de [d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile)** por el proyecto que inició este aprendizaje; a **BelmanteGu y contribuidores de [SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp)** por la base del runtime adaptado al segundo caso; y a los autores de las herramientas y bibliotecas citadas en [CREDITS.md](CREDITS.md). D2R Mobile es el antecedente del método, no la base del runtime de Castlevania.

Las contribuciones deben aportar una causa comprobada, revisión de referencia, prueba y límites. Sigue [CONTRIBUTING.md](CONTRIBUTING.md). No adjuntes material privado en issues o PR.

Documentos, plantillas y verificador originales: [MIT](LICENSE). El contenido de los juegos no queda cubierto por esa licencia. Proyecto independiente, sin afiliación ni aprobación de los editores. El nombre genérico identifica el cuaderno y no cambia los derechos sobre los juegos.
