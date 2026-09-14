# Ronda de la papa caliente

CI-0122 Sistemas Operativos - Tarea programada I

Simula el juego de la papa caliente con procesos (`fork`) que se pasan la
papa en un anillo, usando un buzón (cola de mensajes de System V) para
comunicarse. Cada participante activo le aplica las reglas de Collatz a la
papa; si el resultado da 1, "explota" y ese participante pasa a ser pasivo.
Gana el último en explotar. Un proceso invasor manda mensajes falsos para
tratar de confundir a los participantes.

## Compilar

```
make
```

Esto genera el ejecutable `ronda`. Para borrar lo compilado:

```
make clean
```

## Ejecutar

```
./ronda [participantes] [papa_inicial] [direccion]
```

Los tres parámetros son opcionales, si no se dan el programa los genera
solos:

- **participantes**: cuántos jugadores tiene la ronda (número entero).
- **papa_inicial**: valor con el que arranca la papa (número entero).
- **direccion**: `clockwise` o `counter-clockwise`. Cualquier otro valor
  se toma como `clockwise`.

Ejemplos:

```
./ronda                          # todo generado al azar
./ronda 8                        # 8 participantes, resto al azar
./ronda 8 500 clockwise          # 8 participantes, papa=500, sentido horario
./ronda 12 989345275647 counter-clockwise
```

## Qué se ve en la pantalla

Cada participante avisa cuando registra a su vecino, cuando cambia el
valor de la papa, cuando explota (pasa a ser pasivo), y cuando descarta un
mensaje del invasor. Al final se muestra cuál participante fue el ganador
(el último en explotar), y cómo todos los procesos (participantes e
invasor) van saliendo al enterarse de que el juego terminó.

## Si algo queda pegado

Si el programa se interrumpe a la fuerza (por ejemplo con Ctrl+C) puede
quedar el buzón sin borrar. Para limpiarlo a mano:

```
ipcs -q            # buscar la cola con la llave 23186
ipcrm -Q 23186     # borrarla
```