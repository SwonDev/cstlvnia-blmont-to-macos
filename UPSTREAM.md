# Fuentes y revisiones de referencia

Este repositorio contiene una adaptación de código MIT para **Castlevania Belmont’s Curse for macOS**. La base técnica es SuperMarioWonderRecomp; los resultados de la adaptación a Castlevania no se atribuyen a sus autores originales.

## Base del recompilador y runtime

- Proyecto: [BelmanteGu/SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp).
- Revisión importada: [`010ed7a596a621c15d3a8dfa5795ca8c3668b066`](https://github.com/BelmanteGu/SuperMarioWonderRecomp/tree/010ed7a596a621c15d3a8dfa5795ca8c3668b066).
- Autores: BelmanteGu y contribuidores. [Licencia MIT original](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/010ed7a596a621c15d3a8dfa5795ca8c3668b066/LICENSE), conservada en [licenses/SuperMarioWonderRecomp-MIT.txt](licenses/SuperMarioWonderRecomp-MIT.txt).
- Documentación de referencia: [README](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/010ed7a596a621c15d3a8dfa5795ca8c3668b066/README.md), [framework](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/010ed7a596a621c15d3a8dfa5795ca8c3668b066/docs/FRAMEWORK.md) y [traductor](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/010ed7a596a621c15d3a8dfa5795ca8c3668b066/docs/nvn/TRANSLATOR.md).

La adaptación incorpora el perfil `games/castlevania/`, contratos de módulos, carga, vídeo, resolución, gráficos, audio y entrada, además de preparación y empaquetado locales. La generación AOT transforma las entradas privadas aportadas por el usuario; su resultado no se incluye en Git.

## Traductor offline de shaders

[scripts/bootstrap_sources.py](scripts/bootstrap_sources.py) obtiene únicamente la fuente del traductor y verifica su identidad:

| Campo | Valor fijado |
|---|---|
| Fuente | [Software_arch/Ryubing](https://codeberg.org/Software_arch/Ryubing) |
| Revisión | `a82350bb774f70fcbd41c9987bf67a3775409963` |
| Archivo | [ryubing-a82350bb77.tar.gz](https://codeberg.org/Software_arch/Ryubing/archive/a82350bb77.tar.gz) |
| SHA-256 del archivo | `aba9789bdcf3fda9e90f965636e9e85e9dcc299333fbf5451bcf22ecfd608506` |
| Componentes consumidos | `Ryujinx.Common`, `Ryujinx.Graphics.Shader`, `Spv.Generator` |
| Licencia | [MIT de Ryujinx Team and Contributors](licenses/Ryubing-MIT.txt) |
| Ubicación local | `private/third_party/`; no contiene recursos descargados del juego. |

El harness se compila con .NET 10 para esta cadena de construcción. Una adaptación del proyecto de compilación no cambia la revisión de las fuentes fijadas. El bootstrap contrasta los archivos consumidos contra el archivo verificado, además del marcador de revisión. Los programas Maxwell extraídos y los SPIR-V resultantes se generan desde la copia propia, permanecen privados y se validan por separado.

## Otros componentes

- [FidelityFX FSR, AMD](https://github.com/GPUOpen-Effects/FidelityFX-FSR): [MIT conservada](licenses/FidelityFX-FSR-MIT.txt).
- [SDL](https://github.com/libsdl-org/SDL) y [MoltenVK](https://github.com/KhronosGroup/MoltenVK): se localizan en la instalación del usuario y se incluyen en la app local con sus respectivas licencias. El paquete verifica arquitectura arm64, dependencias internas, versión mínima de macOS y firma.
- [SwonDev/Switch2Bridge](https://github.com/SwonDev/Switch2Bridge): el consumidor nativo del runtime lee el protocolo del puente. No se redistribuye su aplicación; [contrato de integración](docs/SWITCH2BRIDGE.md).

Las versiones efectivas de herramientas y bibliotecas deben registrarse al construir. El empaquetador calcula el mínimo de macOS a partir de todos los componentes; la revisión upstream no fija por sí sola la compatibilidad de las bibliotecas instaladas.

## Antecedente del método

[jhaizhou-ops/d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile) es el antecedente del trabajo de adaptación y QA. No es la base del runtime de este port. El [PR #87](https://github.com/jhaizhou-ops/d2r-mobile/pull/87) es una contribución relacionada, no una declaración de merge, aprobación o compatibilidad de Castlevania por ese upstream.

Los documentos de [metodología](docs/METODOLOGIA.md) distinguen fuentes, experimentos históricos y validación de cada candidata. [CREDITS.md](CREDITS.md) recoge autores y derechos; [LICENSE](LICENSE) cubre el código y documentación propios, con los avisos MIT de terceros conservados. Las capturas y el contenido comercial están excluidos de esa licencia.
