# ChompChamps

ChompChamps es un juego multijugador similar a Snake implementado utilizando IPCs POSIX (Comunicación Entre Procesos). El juego consta de tres componentes principales:

1. `master` - Controla la lógica del juego, los movimientos de los jugadores y la sincronización
2. `vista` - Muestra el estado del juego
3. `player` - Implementa la IA para el movimiento de los jugadores

## Reglas del Juego

- Los jugadores se mueven en una cuadrícula rectangular con recompensas que van del 1 al 9
- Los jugadores pueden moverse una casilla a la vez en 8 direcciones posibles
- Los jugadores no pueden moverse a través de casillas ya capturadas u otros jugadores
- Una vez que una casilla es capturada, permanece capturada hasta el final del juego
- El juego termina cuando ningún jugador puede moverse o se produce un tiempo de espera
- El ganador es el jugador con la puntuación más alta
- En caso de empate, gana el jugador con menos movimientos válidos
- Si aún hay empate, gana el jugador con menos movimientos inválidos

## Compilación

Para compilar el juego, ejecute:

```bash
make
```

Esto creará tres ejecutables: `master`, `vista` y `player`.

## Ejecución del Juego

Para ejecutar el juego con la configuración predeterminada:

```bash
./master -w 15 -h 15 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple
```

Opciones de línea de comandos:

- `-w ancho`: Ancho del tablero (predeterminado: 10, mínimo: 10)
- `-h alto`: Altura del tablero (predeterminado: 10, mínimo: 10)
- `-d retraso`: Milisegundos de espera después de cada actualización de estado (predeterminado: 200)
- `-t tiempo`: Tiempo de espera en segundos para recibir solicitudes de movimiento válidas (predeterminado: 10)
- `-s semilla`: Semilla aleatoria para la generación del tablero (predeterminado: hora actual)
- `-v vista`: Ruta al binario de visualización (opcional)
- `-p jugador1 jugador2 ...`: Rutas a los binarios de los jugadores (requerido, 1-9 jugadores)

## Detalles de Implementación

La implementación utiliza varios mecanismos de IPC POSIX:

- Memoria Compartida: Para el estado del juego y la sincronización
- Semáforos: Para sincronizar el acceso a recursos compartidos
- Tuberías (Pipes): Para la comunicación entre el master y los jugadores

El código utiliza el patrón de sincronización lectores-escritores para garantizar un acceso seguro al estado compartido del juego.
