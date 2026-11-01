// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Copyright (c) -2026 Universidad de Costa Rica


#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SC_NachOS	015177

/*
 *  Avanza los registros del PC para que, al volver al programa de usuario,
 *  se ejecute la instruccion siguiente al "syscall" 
 */
void returnFromSystemCall() {

   machine->WriteRegister( PrevPCReg, machine->ReadRegister( PCReg ) );
   machine->WriteRegister( PCReg, machine->ReadRegister( NextPCReg ) );
   machine->WriteRegister( NextPCReg, machine->ReadRegister( NextPCReg ) + 4 );

}

/*
 *  copiar un string terminado en cero de la memoria de nachos a un buffer de linux
 */
void leerCadena( int direccion, char * destino, int maximo ) {
   int caracter;
   int i = 0;

   do {
      machine->ReadMem( direccion + i, 1, &caracter );
      destino[ i ] = (char) caracter;
      i++;
   } while ( caracter != 0 && i < maximo );

   destino[ maximo - 1 ] = 0;		// por si el string no cabia completo
}

/*
 *  tabla de procesos para exec y join
 *  cada proceso creado con exec recibe una entrada con un semaforo
 *  donde join espera hasta que el proceso haga exit
 */
#define MAX_PROCESOS 32

BitMap * mapaProcesos = NULL;			// entradas ocupadas de la tabla
Semaphore * procesoTermino[ MAX_PROCESOS ];	// exit hace v, join hace p
int estadoSalida[ MAX_PROCESOS ];		// valor que el proceso le paso a exit

int nuevoProceso() {
   if ( mapaProcesos == NULL ) {
      mapaProcesos = new BitMap( MAX_PROCESOS );
   }

   int id = mapaProcesos->Find();
   if ( id != -1 ) {
      procesoTermino[ id ] = new Semaphore( "proceso termino", 0 );
      estadoSalida[ id ] = 0;
   }

   return id;
}

/*
 *  tablas del kernel para los semaforos, locks y variables de condicion
 *  que piden los programas de usuario, el programa solo conoce el indice
 */
#define MAX_SINCRONIZACION 64

BitMap * mapaSemaforos = new BitMap( MAX_SINCRONIZACION );
Semaphore * semaforos[ MAX_SINCRONIZACION ];

BitMap * mapaLocks = new BitMap( MAX_SINCRONIZACION );
Lock * locks[ MAX_SINCRONIZACION ];

BitMap * mapaCondiciones = new BitMap( MAX_SINCRONIZACION );
Condition * condiciones[ MAX_SINCRONIZACION ];

// revisa que el indice este dentro de la tabla y que este ocupado
bool idValido( BitMap * mapa, int id ) {
   return id >= 0 && id < MAX_SINCRONIZACION && mapa->Test( id );
}

/*
 *  System call interface: Halt()
 */
void NachOS_Halt() {		// System call 0

	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();

}


/*
 *  System call interface: void Exit( int )
 */
void NachOS_Exit() {		// System call 1
   int status = machine->ReadRegister( 4 );

   DEBUG( 'a', "Exit con estado %d\n", status );

   // si es el ultimo hilo del proceso cierra los archivos y borra la tabla
   if ( currentThread->tablaArchivos->delThread() == 0 ) {
      currentThread->tablaArchivos->closeAll();
      delete currentThread->tablaArchivos;
   }
   currentThread->tablaArchivos = NULL;

   // si el hilo lo creo exec, avisar a quien haga join y dejarle el estado
   if ( currentThread->idProceso != -1 ) {
      estadoSalida[ currentThread->idProceso ] = status;
      procesoTermino[ currentThread->idProceso ]->V();
   }

   // la memoria se devuelve cuando el destructor del hilo borra su addrspace
   currentThread->Finish();

}


/*
 *  rutina que ejecuta el hilo nuevo de exec dentro del kernel
 *  empieza el programa desde la direccion 0 como lo hace startprocess
 */
void NachosExecThread( void * p ) {

   currentThread->space->InitRegisters();
   currentThread->space->RestoreState();

   machine->Run();			// nunca regresa
   ASSERT( false );
}


/*
 *  System call interface: SpaceId Exec( char * )
 */
void NachOS_Exec() {		// System call 2
   char nombre[ 256 ];
   OpenFile * ejecutable;
   int id;

   leerCadena( machine->ReadRegister( 4 ), nombre, 256 );

   ejecutable = fileSystem->Open( nombre );
   if ( ejecutable == NULL ) {		// el archivo no existe
      DEBUG( 'u', "exec: no se pudo abrir %s\n", nombre );
      machine->WriteRegister( 2, -1 );
      return;
   }

   // revisar que el programa quepa en los marcos libres
   if ( AddrSpace::paginasNecesarias( ejecutable ) > (unsigned int) machine->mapaMemoria->NumClear() ) {
      DEBUG( 'u', "exec: no hay memoria para %s\n", nombre );
      delete ejecutable;
      machine->WriteRegister( 2, -1 );
      return;
   }

   id = nuevoProceso();
   if ( id == -1 ) {			// tabla de procesos llena
      delete ejecutable;
      machine->WriteRegister( 2, -1 );
      return;
   }

   // el destructor de thread libera el nombre, por eso se pide con new
   char * nombreHilo = new char[ 256 ];
   strcpy( nombreHilo, nombre );
   Thread * hilo = new Thread( nombreHilo );

   // proceso nuevo: memoria propia y tabla de archivos propia
   hilo->space = new AddrSpace( ejecutable );
   delete ejecutable;
   hilo->tablaArchivos = new NachosOpenFilesTable();
   hilo->tablaArchivos->addThread();
   hilo->idProceso = id;

   DEBUG( 'u', "exec: %s crea el proceso %d con %s\n", currentThread->getName(), id, nombre );
   hilo->Fork( NachosExecThread, NULL );

   machine->WriteRegister( 2, id );

}


/*
 *  System call interface: int Join( SpaceId )
 */
void NachOS_Join() {		// System call 3
   SpaceId id = machine->ReadRegister( 4 );

   if ( mapaProcesos == NULL || id < 0 || id >= MAX_PROCESOS || ! mapaProcesos->Test( id ) ) {
      machine->WriteRegister( 2, -1 );	// no existe un proceso con ese id
      return;
   }

   DEBUG( 'u', "join: %s espera al proceso %d\n", currentThread->getName(), id );
   procesoTermino[ id ]->P();		// si el proceso ya termino no espera

   // devolver el estado y liberar la entrada de la tabla
   machine->WriteRegister( 2, estadoSalida[ id ] );
   delete procesoTermino[ id ];
   mapaProcesos->Clear( id );

}


/*
 *  System call interface: void Create( char * )
 */
void NachOS_Create() {		// System call 4
   char nombre[ 256 ];
   int unixHandle;

   leerCadena( machine->ReadRegister( 4 ), nombre, 256 );

   // crea el archivo vacio en linux, o lo trunca si ya existia
   unixHandle = creat( nombre, 0666 );
   if ( unixHandle != -1 ) {
      close( unixHandle );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }

}


/*
 *  System call interface: OpenFileId Open( char * )
 */
void NachOS_Open() {		// System call 5
   char nombre[ 256 ];
   int unixHandle;
   int nachosHandle = -1;

   leerCadena( machine->ReadRegister( 4 ), nombre, 256 );

   unixHandle = open( nombre, O_RDWR );
   if ( unixHandle != -1 ) {
      nachosHandle = currentThread->tablaArchivos->Open( unixHandle );
      if ( nachosHandle == -1 ) {	// tabla llena
         close( unixHandle );
      }
   }

   DEBUG( 'f', "open %s -> nachos %d, unix %d\n", nombre, nachosHandle, unixHandle );
   machine->WriteRegister( 2, nachosHandle );

}


/*
 *  System call interface: OpenFileId Write( char *, int, OpenFileId )
 */
void NachOS_Write() {		// System call 7
   int direccion = machine->ReadRegister( 4 );	// char * buffer, en memoria MIPS
   int tamano = machine->ReadRegister( 5 );	// int size
   OpenFileId id = machine->ReadRegister( 6 );	// OpenFileId id
   char * buffer = new char[ tamano + 1 ];
   int caracter;

   // Copiar el buffer de la memoria de NachOS a la memoria de Linux, byte por byte
   for ( int i = 0; i < tamano; i++ ) {
      machine->ReadMem( direccion + i, 1, &caracter );
      buffer[ i ] = (char) caracter;
   }
   buffer[ tamano ] = 0;

   switch ( id ) {
      case ConsoleInput:		// no se puede escribir en la entrada
         machine->WriteRegister( 2, -1 );
         break;
      case ConsoleOutput:
         fwrite( buffer, 1, tamano, stdout );
         fflush( stdout );
         stats->numConsoleCharsWritten += tamano;
         machine->WriteRegister( 2, tamano );
         break;
      case ConsoleError:
         fwrite( buffer, 1, tamano, stderr );
         fflush( stderr );
         machine->WriteRegister( 2, tamano );
         break;
      default:			// archivos abiertos con open
         if ( currentThread->tablaArchivos->isOpened( id ) ) {
            int escritos = write( currentThread->tablaArchivos->getUnixHandle( id ), buffer, tamano );
            machine->WriteRegister( 2, escritos );
         } else {
            machine->WriteRegister( 2, -1 );
         }
         break;
   }

   delete [] buffer;

}


/*
 *  System call interface: OpenFileId Read( char *, int, OpenFileId )
 */
void NachOS_Read() {		// System call 6
   int direccion = machine->ReadRegister( 4 );	// buffer del programa de usuario
   int tamano = machine->ReadRegister( 5 );
   OpenFileId id = machine->ReadRegister( 6 );
   char * buffer = new char[ tamano ];
   int leidos = -1;

   switch ( id ) {
      case ConsoleInput:
         leidos = read( 0, buffer, tamano );
         if ( leidos > 0 ) {
            stats->numConsoleCharsRead += leidos;
         }
         break;
      case ConsoleOutput:		// no se puede leer de la salida
      case ConsoleError:
         leidos = -1;
         break;
      default:
         if ( currentThread->tablaArchivos->isOpened( id ) ) {
            leidos = read( currentThread->tablaArchivos->getUnixHandle( id ), buffer, tamano );
         }
         break;
   }

   // copiar lo leido de la memoria de linux a la memoria de nachos, byte por byte
   for ( int i = 0; i < leidos; i++ ) {
      machine->WriteMem( direccion + i, 1, buffer[ i ] );
   }

   machine->WriteRegister( 2, leidos );
   delete [] buffer;

}


/*
 *  System call interface: void Close( OpenFileId )
 */
void NachOS_Close() {		// System call 8
   OpenFileId id = machine->ReadRegister( 4 );
   int unixHandle = currentThread->tablaArchivos->Close( id );

   if ( unixHandle != -1 ) {
      close( unixHandle );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }

}


/*
 *  rutina que ejecuta el hilo hijo de fork dentro del kernel
 *  prepara los registros y salta a la funcion del usuario
 */
void NachosForkThread( void * p ) {
   long funcion = (long) p;
   AddrSpace * espacio = currentThread->space;

   espacio->InitRegisters();		// registros en cero y puntero de pila al final de su pila
   espacio->RestoreState();		// cargar la tabla de paginas del hijo en la maquina

   // al terminar la funcion se regresa a la direccion 4 de start.s, que llama a exit
   machine->WriteRegister( RetAddrReg, 4 );

   // empezar a ejecutar en la funcion que se le paso a fork
   machine->WriteRegister( PCReg, funcion );
   machine->WriteRegister( NextPCReg, funcion + 4 );

   machine->Run();			// nunca regresa
   ASSERT( false );
}


/*
 *  System call interface: void Fork( void (*func)() )
 */
void NachOS_Fork() {		// System call 9
   long funcion = machine->ReadRegister( 4 );	// direccion de la funcion en la memoria de nachos
   // revisar que haya marcos libres para la pila del hijo
   if ( currentThread->space->paginasDePila() > (unsigned int) machine->mapaMemoria->NumClear() ) {
      DEBUG( 'u', "fork: no hay memoria para la pila de otro hilo\n" );
      return;
   }
   char * nombre = new char[ 32 ];

   // el destructor de thread libera el nombre, por eso se pide con new
   sprintf( nombre, "hilo de %s", currentThread->getName() );
   Thread * hijo = new Thread( nombre );

   // el hijo comparte codigo, datos y archivos abiertos con el padre, pero tiene su propia pila
   hijo->space = new AddrSpace( currentThread->space );
   hijo->tablaArchivos = currentThread->tablaArchivos;
   hijo->tablaArchivos->addThread();

   DEBUG( 'u', "fork: %s crea a %s en la funcion 0x%x\n", currentThread->getName(), nombre, (int) funcion );
   hijo->Fork( NachosForkThread, (void *) funcion );

}


/*
 *  System call interface: void Yield()
 */
void NachOS_Yield() {		// System call 10
   currentThread->Yield();
}


/*
 *  System call interface: Sem_t SemCreate( int )
 */
void NachOS_SemCreate() {		// System call 11
   int valorInicial = machine->ReadRegister( 4 );
   int id = mapaSemaforos->Find();

   if ( id != -1 ) {
      semaforos[ id ] = new Semaphore( "semaforo de usuario", valorInicial );
   }

   machine->WriteRegister( 2, id );	// -1 si la tabla esta llena
}


/*
 *  System call interface: int SemDestroy( Sem_t )
 */
void NachOS_SemDestroy() {		// System call 12
   int id = machine->ReadRegister( 4 );

   if ( idValido( mapaSemaforos, id ) ) {
      semaforos[ id ]->Destroy();	// despierta a los que esperaban en el semaforo
      delete semaforos[ id ];
      mapaSemaforos->Clear( id );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int SemSignal( Sem_t )
 */
void NachOS_SemSignal() {		// System call 13
   int id = machine->ReadRegister( 4 );

   if ( idValido( mapaSemaforos, id ) ) {
      semaforos[ id ]->V();
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int SemWait( Sem_t )
 */
void NachOS_SemWait() {		// System call 14
   int id = machine->ReadRegister( 4 );

   if ( idValido( mapaSemaforos, id ) ) {
      semaforos[ id ]->P();		// si el valor es cero el hilo se duerme aqui
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: Lock_t LockCreate( int )
 */
void NachOS_LockCreate() {		// System call 15
   int id = mapaLocks->Find();

   if ( id != -1 ) {
      locks[ id ] = new Lock( "lock de usuario" );
   }

   machine->WriteRegister( 2, id );
}


/*
 *  System call interface: int LockDestroy( Lock_t )
 */
void NachOS_LockDestroy() {		// System call 16
   int id = machine->ReadRegister( 4 );

   if ( idValido( mapaLocks, id ) ) {
      delete locks[ id ];
      mapaLocks->Clear( id );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int LockAcquire( Lock_t )
 */
void NachOS_LockAcquire() {		// System call 17
   int id = machine->ReadRegister( 4 );

   if ( idValido( mapaLocks, id ) ) {
      locks[ id ]->Acquire();
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int LockRelease( Lock_t )
 */
void NachOS_LockRelease() {		// System call 18
   int id = machine->ReadRegister( 4 );

   // solo el hilo que tiene el lock lo puede liberar
   if ( idValido( mapaLocks, id ) && locks[ id ]->isHeldByCurrentThread() ) {
      locks[ id ]->Release();
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: Cond_t LockCreate( int )
 */
void NachOS_CondCreate() {		// System call 19
   int id = mapaCondiciones->Find();

   if ( id != -1 ) {
      condiciones[ id ] = new Condition( "condicion de usuario" );
   }

   machine->WriteRegister( 2, id );
}


/*
 *  System call interface: int CondDestroy( Cond_t )
 */
void NachOS_CondDestroy() {		// System call 20
   int id = machine->ReadRegister( 4 );

   if ( idValido( mapaCondiciones, id ) ) {
      delete condiciones[ id ];
      mapaCondiciones->Clear( id );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int CondSignal( Cond_t )
 */
void NachOS_CondSignal() {		// System call 21
   int idCondicion = machine->ReadRegister( 4 );
   int idLock = machine->ReadRegister( 5 );

   // la condicion se usa siempre con un lock que el hilo ya tiene
   if ( idValido( mapaCondiciones, idCondicion ) && idValido( mapaLocks, idLock )
        && locks[ idLock ]->isHeldByCurrentThread() ) {
      condiciones[ idCondicion ]->Signal( locks[ idLock ] );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int CondWait( Cond_t )
 */
void NachOS_CondWait() {		// System call 22
   int idCondicion = machine->ReadRegister( 4 );
   int idLock = machine->ReadRegister( 5 );

   // la condicion se usa siempre con un lock que el hilo ya tiene
   if ( idValido( mapaCondiciones, idCondicion ) && idValido( mapaLocks, idLock )
        && locks[ idLock ]->isHeldByCurrentThread() ) {
      condiciones[ idCondicion ]->Wait( locks[ idLock ] );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: int CondBroadcast( Cond_t )
 */
void NachOS_CondBroadcast() {		// System call 23
   int idCondicion = machine->ReadRegister( 4 );
   int idLock = machine->ReadRegister( 5 );

   // la condicion se usa siempre con un lock que el hilo ya tiene
   if ( idValido( mapaCondiciones, idCondicion ) && idValido( mapaLocks, idLock )
        && locks[ idLock ]->isHeldByCurrentThread() ) {
      condiciones[ idCondicion ]->Broadcast( locks[ idLock ] );
      machine->WriteRegister( 2, 0 );
   } else {
      machine->WriteRegister( 2, -1 );
   }
}


/*
 *  System call interface: Socket_t Socket( int, int )
 */
void NachOS_Socket() {			// System call 30
   int familia = machine->ReadRegister( 4 );	// 0 ipv4, 1 ipv6
   int tipo = machine->ReadRegister( 5 );	// 0 stream, 1 datagramas
   int unixHandle;
   int nachosHandle = -1;

   unixHandle = socket( familia == AF_INET6_NachOS ? AF_INET6 : AF_INET,
                        tipo == SOCK_DGRAM_NachOS ? SOCK_DGRAM : SOCK_STREAM, 0 );

   // el socket se guarda en la misma tabla que los archivos,
   // asi read, write y close funcionan igual con sockets
   if ( unixHandle != -1 ) {
      nachosHandle = currentThread->tablaArchivos->Open( unixHandle );
      if ( nachosHandle == -1 ) {
         close( unixHandle );
      }
   }

   DEBUG( 'n', "socket -> nachos %d, unix %d\n", nachosHandle, unixHandle );
   machine->WriteRegister( 2, nachosHandle );
}


/*
 *  System call interface: int Connect( Socket_t, char *, int )
 */
void NachOS_Connect() {		// System call 31
   int id = machine->ReadRegister( 4 );
   char ip[ 64 ];
   int puerto = machine->ReadRegister( 6 );
   int unixHandle = currentThread->tablaArchivos->getUnixHandle( id );
   int resultado = -1;

   leerCadena( machine->ReadRegister( 5 ), ip, 64 );

   if ( unixHandle != -1 ) {
      if ( strchr( ip, ':' ) != NULL ) {	// una direccion con dos puntos es ipv6
         struct sockaddr_in6 direccion;
         memset( &direccion, 0, sizeof( direccion ) );
         direccion.sin6_family = AF_INET6;
         direccion.sin6_port = htons( puerto );
         if ( inet_pton( AF_INET6, ip, &direccion.sin6_addr ) == 1 ) {
            resultado = connect( unixHandle, (struct sockaddr *) &direccion, sizeof( direccion ) );
         }
      } else {
         struct sockaddr_in direccion;
         memset( &direccion, 0, sizeof( direccion ) );
         direccion.sin_family = AF_INET;
         direccion.sin_port = htons( puerto );
         if ( inet_pton( AF_INET, ip, &direccion.sin_addr ) == 1 ) {
            resultado = connect( unixHandle, (struct sockaddr *) &direccion, sizeof( direccion ) );
         }
      }
   }

   DEBUG( 'n', "connect %s:%d -> %d\n", ip, puerto, resultado );
   machine->WriteRegister( 2, resultado );
}


/*
 *  System call interface: int Bind( Socket_t, int )
 */
void NachOS_Bind() {		// System call 32
   int id = machine->ReadRegister( 4 );
   int puerto = machine->ReadRegister( 5 );
   int unixHandle = currentThread->tablaArchivos->getUnixHandle( id );
   int resultado = -1;

   if ( unixHandle != -1 ) {
      struct sockaddr_in direccion;
      int reusar = 1;

      // permite volver a usar el puerto apenas se cierra el servidor
      setsockopt( unixHandle, SOL_SOCKET, SO_REUSEADDR, &reusar, sizeof( reusar ) );

      memset( &direccion, 0, sizeof( direccion ) );
      direccion.sin_family = AF_INET;
      direccion.sin_addr.s_addr = htonl( INADDR_ANY );	// escucha en todas las interfaces
      direccion.sin_port = htons( puerto );
      resultado = bind( unixHandle, (struct sockaddr *) &direccion, sizeof( direccion ) );
   }

   DEBUG( 'n', "bind puerto %d -> %d\n", puerto, resultado );
   machine->WriteRegister( 2, resultado );
}


/*
 *  System call interface: int Listen( Socket_t, int )
 */
void NachOS_Listen() {		// System call 33
   int id = machine->ReadRegister( 4 );
   int cola = machine->ReadRegister( 5 );
   int unixHandle = currentThread->tablaArchivos->getUnixHandle( id );
   int resultado = -1;

   if ( unixHandle != -1 ) {
      resultado = listen( unixHandle, cola );
   }

   machine->WriteRegister( 2, resultado );
}


/*
 *  System call interface: int Accept( Socket_t )
 */
void NachOS_Accept() {		// System call 34
   int id = machine->ReadRegister( 4 );
   int unixHandle = currentThread->tablaArchivos->getUnixHandle( id );
   int nachosHandle = -1;

   if ( unixHandle != -1 ) {
      // espera un cliente, el socket nuevo tambien entra en la tabla
      int cliente = accept( unixHandle, NULL, NULL );
      if ( cliente != -1 ) {
         nachosHandle = currentThread->tablaArchivos->Open( cliente );
         if ( nachosHandle == -1 ) {
            close( cliente );
         }
      }
   }

   DEBUG( 'n', "accept -> nachos %d\n", nachosHandle );
   machine->WriteRegister( 2, nachosHandle );
}


/*
 *  System call interface: int Shutdown( Socket_t, int )
 */
void NachOS_Shutdown() {	// System call 35
   int id = machine->ReadRegister( 4 );
   int modo = machine->ReadRegister( 5 );	// 0 lectura, 1 escritura, 2 ambos
   int unixHandle = currentThread->tablaArchivos->getUnixHandle( id );
   int resultado = -1;

   if ( unixHandle != -1 ) {
      resultado = shutdown( unixHandle, modo );
   }

   machine->WriteRegister( 2, resultado );
}


//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2) - SC_User;

    switch ( which ) {

       case SyscallException:
         DEBUG( 'u', "%s hace el llamado al sistema %d\n", currentThread->getName(), type );
          switch ( type ) {
             case SC_Halt:		// System call # 0
                NachOS_Halt();
                break;
             case SC_Exit:		// System call # 1
                NachOS_Exit();
                break;
             case SC_Exec:		// System call # 2
                NachOS_Exec();
                break;
             case SC_Join:		// System call # 3
                NachOS_Join();
                break;

             case SC_Create:		// System call # 4
                NachOS_Create();
                break;
             case SC_Open:		// System call # 5
                NachOS_Open();
                break;
             case SC_Read:		// System call # 6
                NachOS_Read();
                break;
             case SC_Write:		// System call # 7
                NachOS_Write();
                break;
             case SC_Close:		// System call # 8
                NachOS_Close();
                break;

             case SC_Fork:		// System call # 9
                NachOS_Fork();
                break;
             case SC_Yield:		// System call # 10
                NachOS_Yield();
                break;

             case SC_SemCreate:         // System call # 11
                NachOS_SemCreate();
                break;
             case SC_SemDestroy:        // System call # 12
                NachOS_SemDestroy();
                break;
             case SC_SemSignal:         // System call # 13
                NachOS_SemSignal();
                break;
             case SC_SemWait:           // System call # 14
                NachOS_SemWait();
                break;

             case SC_LckCreate:         // System call # 15
                NachOS_LockCreate();
                break;
             case SC_LckDestroy:        // System call # 16
                NachOS_LockDestroy();
                break;
             case SC_LckAcquire:         // System call # 17
                NachOS_LockAcquire();
                break;
             case SC_LckRelease:           // System call # 18
                NachOS_LockRelease();
                break;

             case SC_CondCreate:         // System call # 19
                NachOS_CondCreate();
                break;
             case SC_CondDestroy:        // System call # 20
                NachOS_CondDestroy();
                break;
             case SC_CondSignal:         // System call # 21
                NachOS_CondSignal();
                break;
             case SC_CondWait:           // System call # 22
                NachOS_CondWait();
                break;
             case SC_CondBroadcast:           // System call # 23
                NachOS_CondBroadcast();
                break;

             case SC_Socket:	// System call # 30
		NachOS_Socket();
               break;
             case SC_Connect:	// System call # 31
		NachOS_Connect();
               break;
             case SC_Bind:	// System call # 32
		NachOS_Bind();
               break;
             case SC_Listen:	// System call # 33
		NachOS_Listen();
               break;
             case SC_Accept:	// System call # 32
		NachOS_Accept();
               break;
             case SC_Shutdown:	// System call # 33
		NachOS_Shutdown();
               break;

             default:
                printf("NachOS version: %d-%d\n", (SC_User + SC_NachOS)/10, (SC_Base + SC_NachOS)%10 );
                printf("Unexpected syscall exception %d\n", type );
                ASSERT( false );
                break;
          }
          returnFromSystemCall();	// avanzar el PC despues de cualquier llamado
          break;

       case PageFaultException: {
          break;
       }

       case ReadOnlyException:
          printf( "Read Only exception (%d)\n", which );
          ASSERT( false );
          break;

       case BusErrorException:
          printf( "Bus error exception (%d)\n", which );
          ASSERT( false );
          break;

       case AddressErrorException:
          printf( "Address error exception (%d)\n", which );
          ASSERT( false );
          break;

       case OverflowException:
          printf( "Overflow exception (%d)\n", which );
          ASSERT( false );
          break;

       case IllegalInstrException:
          printf( "Ilegal instruction exception (%d)\n", which );
          ASSERT( false );
          break;

       default:
          printf( "Unexpected exception %d\n", which );
          ASSERT( false );
          break;
    }

}
