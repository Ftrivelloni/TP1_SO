# TP1_SO - Chomp Champs

Este proyecto implementa un juego llamado Chomp Champs, donde múltiples jugadores compiten en un tablero compartido. El juego utiliza memoria compartida y semáforos para la sincronización entre procesos.

## Estructura del Proyecto

El proyecto está compuesto por los siguientes archivos principales:

- **master.c**: Controla el flujo principal del juego, inicializa los jugadores, la vista y gestiona la lógica del juego.
- **vista.c**: Representa la vista del juego, mostrando el estado del tablero y los jugadores.
- **player_simple.c**: Implementa un jugador con lógica simple para decidir sus movimientos.
- **structs.h**: Contiene las definiciones de estructuras.
- **master_utils.c**: Es la librería de master.c.
- **master_utils.h**: Contiene las definiciones de las funciones de master_utils.c.
- **sharedMem.c**: Es la librería utilizada para el uso de memoria compartida.
- **sharedMem.h**: Contiene las definiciones de las funciones de sharedMem.c.
- **Makefile**: Facilita la compilación del proyecto.

## Compilación

Para compilar el proyecto, utiliza el archivo Makefile incluido. Ejecuta el siguiente comando en la raíz del proyecto:

```bash
make all
```

## Descripción del Juego

Cada jugador es un proceso independiente que interactúa con el máster del juego a través de memoria compartida. El máster coordina la ejecución, administra el tablero y sincroniza los turnos mediante semáforos. El objetivo de cada jugador es realizar movimientos estratégicos para dominar el tablero y vencer a los oponentes.

### Reglas del Juego

- Los jugadores se mueven en una cuadrícula rectangular con recompensas que van del 1 al 9
- Los jugadores pueden moverse una casilla a la vez en 8 direcciones posibles
- Los jugadores no pueden moverse a través de casillas ya capturadas u otros jugadores
- Una vez que una casilla es capturada, permanece capturada hasta el final del juego
- El juego termina cuando ningún jugador puede moverse o se produce un tiempo de espera
- El ganador es el jugador con la puntuación más alta

## Parámetros del Máster

El máster acepta los siguientes parámetros:

| Parámetro | Descripción | Valor por defecto |
|-----------|-------------|-------------------|
| `-w width` | Ancho del tablero | 10 |
| `-h height` | Alto del tablero | 10 |
| `-d delay` | Milisegundos de espera entre impresiones del estado | 200 |
| `-t timeout` | Tiempo máximo (en segundos) para recibir un movimiento válido de un jugador | 10 |
| `-s seed` | Semilla para la generación aleatoria del tablero | time(NULL) |
| `-v view` | Ruta al binario de la vista (opcional) | Sin vista |
| `-p player1 ...` | Rutas a los binarios de los jugadores (mínimo 1, máximo 9) | Obligatorio |

## Ejemplo de Ejecución

```bash
./master -w 15 -h 12 -d 300 -t 8 -s 1234 -v ./vista -p ./player_simple ./player_simple
```

## Detalles de Implementación

La implementación utiliza varios mecanismos de IPC POSIX:

- **Memoria Compartida**: Para el estado del juego y la sincronización
- **Semáforos**: Para sincronizar el acceso a recursos compartidos
- **Tuberías (Pipes)**: Para la comunicación entre el master y los jugadores

El código utiliza el patrón de sincronización lectores-escritores para garantizar un acceso seguro al estado compartido del juego.
