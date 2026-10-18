# Galería: Castlevania Belmont’s Curse for macOS

Catorce capturas: las doce primeras proceden de pruebas históricas de la adaptación local; las dos últimas muestran la app generada desde el pipeline público. Cada grupo conserva su contexto y no certifica todas las escenas del juego. Son **lecturas GPU del juego**, copiadas sin retoque; no capturas de un escritorio con ventana visible. Las dimensiones de los PNG reflejan el instrumento de QA, no necesariamente el lienzo renderizado. [Manifiesto de dimensiones y SHA-256](../assets/screenshots/manifest.json).

El arte, personajes, interfaz y marcas son de sus titulares: KONAMI y los demás titulares identificados por el juego. Las imágenes ilustran el caso técnico; **no están bajo la licencia MIT** del código del proyecto. Su publicación no implica afiliación ni aprobación de KONAMI, Nintendo o Apple. No se distribuyen texturas, modelos, audio, películas ni otros paquetes de recursos.

## Prólogo y partida: capturas históricas

Estas imágenes documentan etapas anteriores del trabajo. No se usan para certificar todos los gráficos ni la versión final 1.1.4. El [caso técnico](CASO-CASTLEVANIA.md) distingue los recorridos.

### 1. Menú principal

![Menú principal original de Castlevania en español](../assets/screenshots/01-menu.png)

El menú demuestra presentación de imagen e interfaz; por sí solo no prueba una partida o recuperación de progreso.

### 2. Cinemática y subtítulos

![Ciudad en llamas durante la cinemática, con subtítulos en español](../assets/screenshots/02-cinematica-ciudad.png)

Imagen del recorrido de decodificación/presentación original. No acredita escucha de voces.

### 3. Trevor en la cinemática

![Trevor durante la película inicial](../assets/screenshots/03-cinematica-trevor.png)

La fuente de vídeo conserva su detalle original de 1080p; una salida Retina no crea detalle adicional.

### 4. Diálogo del prólogo

![Trevor y otros personajes, escenario del río y cuadro de diálogo](../assets/screenshots/04-dialogo.png)

Prueba histórica con el cliente real del puente y entrada sintética; no una acreditación de pulsaciones físicas.

### 5. Escenario y composición

![Panorámica del río con personajes y ciudad de fondo](../assets/screenshots/05-escenario.png)

Lectura GPU del prólogo. Las bandas conservan la composición de esa escena.

### 6. Partida y HUD

![Personaje en el muelle y HUD original](../assets/screenshots/06-partida-hud.png)

Fotograma del recorrido de partida. Una imagen fija no mide por sí sola fluidez ni ausencia de parpadeos.

### 7. Tutorial

![Interior con personaje saltando e instrucción original de mantener B](../assets/screenshots/07-tutorial.png)

Captura histórica del tutorial. Se conserva el glifo Nintendo y la distinción entre pulsación breve y mantenida.

### 8. Arcanos

![Menú de Arcanos con Llama maldita y descripción en español](../assets/screenshots/08-arcanos.png)

Interfaz original durante el recorrido sintético.

### 9. Equipamiento

![Pantalla de equipamiento, arsenal y reliquias](../assets/screenshots/09-equipamiento.png)

Otra pantalla del mismo recorrido; no contiene un perfil personal publicado ni un archivo de guardado.

## Opciones y cinemática de la serie 1.1.4

### 10. Resolución nativa ajustada a 16:9

![Opciones originales con resolución 3024 por 1701](../assets/screenshots/10-video-retina.png)

Candidata final 1.1.4. En el panel 3024 × 1964 se usa un lienzo de 3024 × 1701; esta captura de documentación mide 1024 × 576.

### 11. Selector de pantalla completa

![App instalada con modo Pantalla completa con bordes seleccionado](../assets/screenshots/11-video-pantalla-completa.png)

Lectura GPU de la app instalada 1.1.4. Muestra el selector original y la resolución 1280 × 720. No sustituye una prueba del Space visible de macOS.

### 12. Vista aérea de la cinemática

![Vista aérea de la ciudad durante la cinemática con subtítulos](../assets/screenshots/12-cinematica-aerea.png)

Candidata intermedia de la serie 1.1.4, anterior a la última validación de límites. Se identifica como tal para no confundir una imagen representativa con la QA final.


## Construcción desde el pipeline público

### 13. Opciones del paquete generado

![Vídeo original del nuevo paquete, con resolución 1280 × 720 y pantalla completa de escritorio aplicadas](../assets/screenshots/13-video-compilacion-publica.png)

Lectura GPU de la app generada desde la implementación pública `5731727`, ejecutable `835675db…`. El recorrido aplicó 1280 × 720 y modo 1; la imagen conserva el lienzo de presentación 3024 × 1701. No acredita un Space visible. [Pruebas de esta candidata](VALIDACION.md).


### 14. Partida del paquete generado

![Trevor y HUD en el escenario del prólogo desde el nuevo paquete](../assets/screenshots/14-partida-compilacion-publica.png)

Lectura GPU a 3024 × 1701 de la candidata pública, en un recorrido adicional de 80 segundos con entrada sintética por el cliente real Switch2Bridge. Se observaron escenario, personaje y HUD; sin errores fatales ni shaders ausentes en ese tramo. El dispositivo físico y todos los niveles siguen fuera de esa prueba.
