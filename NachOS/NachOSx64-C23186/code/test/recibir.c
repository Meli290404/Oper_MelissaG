// recibir.c
//	prueba de buzones: espera un mensaje en el buzon que tiene como llave el carne,
//	lo escribe en la consola y destruye el buzon

#include "syscall.h"

#define LLAVE 23186		// carne c23186 sin la letra, porque la llave tiene que ser un numero


// cantidad de caracteres de un string
int largo( char * texto ) {
   int n = 0;

   while ( texto[ n ] != 0 ) {
      n++;
   }

   return n;
}


// escribe un string 
void escribir( char * texto ) {
   Write( texto, largo( texto ), ConsoleOutput );
}


int main() {
   char mensaje[ 100 ];
   int buzon;
   int n;

   buzon = MsgGet( LLAVE, 0 );
   escribir( "recibir: esperando un mensaje\n" );

   // msgrcv no regresa hasta que haya un mensaje en el buzon
   n = MsgRcv( buzon, 99, mensaje );

   escribir( "recibir: llego el mensaje: " );
   Write( mensaje, n, ConsoleOutput );
   escribir( "\n" );

   MsgCtl( buzon, IPC_RMID_NachOS );
   Exit( 0 );
}
