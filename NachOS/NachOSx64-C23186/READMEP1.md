# Proyecto 2: Programas de usuario de NachOS

**Melissa Garita Ch. — C23186**

## Notas sobre lo que se modificó

- **Paginación:** BitMap de marcos libres en la clase `Machine`. `AddrSpace` asigna marcos libres y carga el programa página por página.
- **Memoria:** `NumPhysPages` se subió de 32 a 128 para que quepan `shell` + `todos` + `copy` a la vez y los hilos de `agua`. La paginación se probó primero con 32 marcos.
- **Pila:** `UserStackSize` pasó de 1024 a 1280 bytes porque `addrspacetest` y `copy` usan un buffer local de 1024 bytes.
- **Tabla de archivos abiertos:** clase nueva `NachosOpenFilesTable` (`userprog/nachostabla.h` y `.cc`), compartida por los hilos de un proceso. Archivos y sockets usan la misma tabla.
- **Llamados al sistema** en `userprog/exception.cc`:
  - Obligatorios: Halt, Exit, Exec, Join, Create, Open, Read, Write, Close, Fork, Yield.
  - Opcionales: semáforos, locks y condiciones; Socket, Connect, Bind, Listen, Accept, Shutdown.
  - Buzones: MsgGet, MsgCtl, MsgRcv, MsgSnd (números 50 a 53). Usan colas de mensajes System V de Linux, así NachOS y Linux comparten el buzón con la misma llave (23186).
- **Fork:** el hijo comparte código y datos con el padre y recibe una pila propia.
- **Cleanup** (`threads/system.cc`): libera la memoria del programa antes de borrar `machine`, y solo borra el hilo actual si es `main`.
- **Programas nuevos:** `test/agua.c`, `test/enviar.c`, `test/recibir.c`, `linux/enviarLinux.c`, `linux/recibirLinux.c`.
- **`test/socket.c` corregido:** el pedido se arma en un arreglo propio, se quita el `\n` de la figura y la respuesta se lee completa.

## Compilación

```bash
# kernel
cd code/userprog
make depend
make

# programas de usuario
cd ../test
make halt addrspacetest copy todos shell fork pingPong agua socket enviar recibir

# programas de linux para los buzones
cd ../linux
gcc enviarLinux.c -o enviarLinux
gcc recibirLinux.c -o recibirLinux
```

## Ejecución

Todo se corre desde `code/userprog`.

```bash
./nachos -x ../test/halt
./nachos -x ../test/addrspacetest

cp nachos nachos.1
./nachos -x ../test/copy

./nachos -x ../test/todos
./nachos -x ../test/fork
./nachos -x ../test/pingPong
./nachos -x ../test/agua
./nachos -x ../test/socket          # figura: rabbit.txt
```

**Shell** (se sale con `Ctrl+C`):
```bash
./nachos -x ../test/shell
--../test/todos
```

**Buzones:**
```bash
# nachos -> nachos, dentro del shell
--../test/enviar
--../test/recibir

# linux -> nachos
../linux/enviarLinux "hola desde linux"
./nachos -x ../test/recibir

# nachos -> linux
./nachos -x ../test/enviar
../linux/recibirLinux
```