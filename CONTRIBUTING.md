# Contribuir

Una contribución útil describe un problema reproducible, su causa, revisión de referencia, prueba ejecutada y límites. Las correcciones generales pertenecen preferentemente al upstream que mantiene ese componente. Antes de abrir PR, buscar duplicados y enlazar propuestas existentes.

Para este cuaderno se aceptan casos originales, correcciones verificadas y mejoras de las plantillas. Distinguir medición local y afirmación de un tercero. Fijar versiones y enlazar fuentes primarias. No convertir una prueba parcial en soporte universal.

## Material admisible

Documentación original, ejemplos sintéticos y, tras revisión de derechos y privacidad, capturas ilustrativas contextualizadas. No adjuntar juegos, claves, firmware, saves, ejecutables, shaders extraídos, dumps, datos personales, logs completos o enlaces a descargas no autorizadas. Sanitizar una ruta no vuelve público su contenido.

Conservar atribuciones y licencias de cualquier material incorporado. El contenido del juego y las capturas no quedan bajo MIT por estar en este repositorio. Identificar autoría/procedencia de cada imagen y no usar capturas ajenas sin autorización adecuada.

## Verificar una modificación

Revisar enlaces relativos, UTF-8, afirmaciones, contexto de capturas y consistencia de versiones. Ejecutar el verificador de documentos del repositorio. Si cambia una imagen, volver a inspeccionar píxeles y metadatos. Mantener una lista explícita de archivos públicos y revisar el diff completo antes del push. No copiar el historial del workspace privado.

```bash
python3 scripts/verify_docs.py
python3 -m unittest discover -s tests -v
```

Requiere Python 3.10 o posterior, sin paquetes externos. `PUBLIC_FILES.json` contiene el inventario aprobado; si se incorporasen imágenes tras resolver su publicación, un manifiesto de capturas debe fijar sus hashes. Cambiar el inventario exige revisión, no regeneración automática para esconder un fallo. El verificador comprueba enlaces locales e integridad y rechaza chunks PNG adicionales; no prueba los píxeles del juego, no consulta enlaces externos y no garantiza ausencia de secretos.
