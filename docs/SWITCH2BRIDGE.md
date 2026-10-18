# Mando: integrar Switch2Bridge sin duplicarlo

Repositorio oficial del proyecto del usuario: **[SwonDev/Switch2Bridge](https://github.com/SwonDev/Switch2Bridge)**. Sus [instrucciones de instalación](https://github.com/SwonDev/Switch2Bridge#instalación), [funcionamiento](https://github.com/SwonDev/Switch2Bridge/blob/main/docs/COMO-FUNCIONA.md) e [investigación](https://github.com/SwonDev/Switch2Bridge/blob/main/docs/INVESTIGACION.md) son la fuente de mantenimiento. No redistribuimos el demonio, la shim ni otra copia de su código.

Referencia pública contrastada: [`d7dc2f2`](https://github.com/SwonDev/Switch2Bridge/tree/d7dc2f240ea4a267a84bc44f89668048380db1ba). La app instalada durante el caso era 1.0.0, empaquetada desde `ff2e1a1d99c8529a8f693fa4ab7cf82583cd3d7d`; una revisión pública posterior no identifica automáticamente ese ejecutable.

## Dos consumidores distintos

```text
Switch 2 Pro Controller
  Bluetooth / GATT
    Switch2Bridge.app
      socket Unix: tramas de 20 bytes
        consumidor Wine/SDL del repo original
        consumidor nativo específico de la adaptación local
```

El README publicado del puente declara soporte probado en Wine y limita los juegos nativos. La adaptación local **no crea un mando virtual global**: su runtime consume directamente el socket. Esto añade compatibilidad a ese consumidor concreto; no demuestra que otros juegos nativos puedan usarlo ni modifica el alcance público del puente. No se desactivó SIP ni se añadieron entitlements de dispositivo virtual.

## Contrato del consumidor usado en el caso

| Campo | Formato |
|---|---|
| Orden y tamaño | Little endian, `<IIhhhhBBBB`, 20 bytes |
| Magia | `0x53325031` |
| Botones | Máscara UInt32, posiciones Nintendo |
| Ejes | Cuatro Int16; Y del puente crece hacia abajo |
| Gatillos | Dos UInt8 |
| Estado | Conexión UInt8 y relleno UInt8 |

Contrastar siempre la revisión del productor antes de implementar un cliente. El adaptador del caso conserva A/B/X/Y y glifos Nintendo, convierte el signo de Y para Horizon y neutraliza al perder foco, caducar tramas o desconectar. Reensambla lecturas fragmentadas, valida cabecera/estado y reconecta. La ruta o configuración del socket debe ser local; no publicar rutas personales.

## Matriz de pruebas

| Prueba | Qué demuestra | Qué no demuestra |
|---|---|---|
| Tramas reales conectadas en reposo | Transporte y estado conectado | Botones físicos o jugabilidad |
| Fixtures de fragmentación y límites | Parser, ejes y neutralización | Bluetooth real |
| Cliente real + servidor sintético aislado | Entrada hasta el runtime y efectos observados | Mando físico |
| Pérdida de foco y desconexión | No quedan botones/ejes atascados | Toda condición del sistema |
| Acciones físicas dentro del juego | Cadena completa de esas acciones | Todas las combinaciones o vibración |

El caso ejecutó las tres primeras categorías y contratos de cancelación/reconexión. La comprobación de acciones físicas sigue fuera de la certificación automatizada. La **vibración no está implementada en el consumidor nativo** y se anuncia desactivada. No confundir esa limitación con la del proyecto completo del puente.

Las pruebas sintéticas usan un socket propio y foco sintético sólo en modo diagnóstico; no toman el socket del demonio ni modifican botellas ajenas. Anclar eventos a un estado observado del juego: segundos desde conexión no equivalen a segundos desde el primer sondeo del invitado.
