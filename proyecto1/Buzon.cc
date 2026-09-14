/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-ii
  *
  *   Class implementation
  *
 **/

#include <stdexcept>
#include <cstring>	// memcpy, strlen
#include <cerrno>	// errno, ENOMSG

#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>	// getpid()

#include "Buzon.h"


/**
  *  Class constructor
  *
  *  OJO: este constructor lo debe correr el proceso "main", ANTES de hacer los
  *  fork(). Así, cuando main haga fork(), cada hijo hereda una copia de este
  *  objeto con el mismo "id" de cola (el id de una cola de mensajes es un
  *  recurso del kernel, no un file descriptor de un solo proceso, así que
  *  sigue siendo válido en todos los hijos).
  *
 **/
Buzon::Buzon() {
   int st = -1;

   // IPC_CREAT: si la cola no existe la crea; si ya existe (por ejemplo si el
   // programa se cerró mal la vez anterior y quedó pegada) la reutiliza.
   // 0666: permisos de lectura/escritura para todos (dueño, grupo, otros).
   st = msgget( KEY, IPC_CREAT | 0666 );

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::Buzon( int )" );
   }

   id = st;
   owner = getpid();	// Guardo quién me creó, para saber quién puede borrarme

}


/**
  * Class destructor
  *
  *  Solo el proceso que creó la cola (owner) debe destruirla. Si un hijo
  *  termina y su copia del objeto Buzon se destruye (sale de scope o hace
  *  exit), NO queremos que borre la cola que los demás siguen usando.
  *
 **/
Buzon::~Buzon() {
   int st = 0;

   if ( owner == getpid() ) {
      // Solo main (el dueño original) llega a este if
      st = msgctl( id, IPC_RMID, NULL );
   }

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::~Buzon( int )" );
   }

}


/**
  *  Send method
  *
  *  @param     const char * mensaje: arreglo de caracteres a enviar
  *
  *  Este es solo un atajo para mandar texto plano: calculo el tamaño con
  *  strlen (+1 para que se lleve el '\0') y lo delego al Enviar genérico.
  *
 **/
int Buzon::Enviar( const char * mensaje, long tipo ) {
   int st = -1;

   st = Enviar( (const void *) mensaje, strlen( mensaje ) + 1, tipo );

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::Enviar( const char * )" );
   }

   return st;

}


/**
  *  Send method
  *
  *  @param     const void * mensaje: estructura con el mensaje a enviar
  *  @param	int cantidad: cantidad de bytes a enviar
  *
  *  msgsnd necesita un bloque de memoria que empiece con un "long mtype"
  *  seguido de los datos, pegados uno después del otro. Como a este método
  *  le llega el mensaje aparte (mensaje/cantidad) y el tipo aparte (tipo),
  *  tengo que armar ese bloque "a mano" con memcpy antes de mandarlo.
  *
 **/
int Buzon::Enviar( const void * mensaje, int cantidad, long tipo ) {
   int st = -1;

   // Reservo espacio para el mtype (long) + los datos (cantidad bytes)
   char * buffer = new char[ sizeof( long ) + cantidad ];

   memcpy( buffer, &tipo, sizeof( long ) );			// primero el tipo
   memcpy( buffer + sizeof( long ), mensaje, cantidad );	// luego los datos

   // msgsz es SOLO el tamaño de los datos, sin contar el mtype
   st = msgsnd( id, buffer, cantidad, 0 );

   delete [] buffer;

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::Enviar( const void *, int, long )" );
   }

   return st;

}


/**
  *  Receive method
  *
  *  @param     const void * mensaje: estructura con el mensaje a enviar
  *  @param	int cantidad: cantidad de bytes a enviar
  *  @param	bool esperar: true = se bloquea hasta que llegue un mensaje
  *  			(comportamiento de siempre); false = revisa una sola vez
  *  			y devuelve -1 de inmediato si no hay nada (usa IPC_NOWAIT)
  *
  *  El caso "false" lo necesita el invasor: tiene que revisar de rato en
  *  rato si ya le avisaron que el juego terminó, sin quedarse pegado
  *  esperando ese mensaje en vez de seguir generando interferencia.
  *
 **/
int Buzon::Recibir( void * mensaje, int cantidad, long tipo, bool esperar ) {
   int st = -1;
   int flags = esperar ? 0 : IPC_NOWAIT;

   char * buffer = new char[ sizeof( long ) + cantidad ];

   st = msgrcv( id, buffer, cantidad, tipo, flags );

   if ( -1 == st ) {
      delete [] buffer;

      // Con IPC_NOWAIT, "no hay mensaje todavia" no es un error real,
      // es una respuesta válida a "¿ya llegó algo?" -> no lanzo excepción
      if ( ! esperar && ENOMSG == errno ) {
         return -1;
      }

      throw std::runtime_error( "Buzon::Recibir( void *, int, long )" );
   }

   // Le devuelvo al que llamó solo la parte de datos, sin el mtype
   memcpy( mensaje, buffer + sizeof( long ), cantidad );

   delete [] buffer;

   return st;

}