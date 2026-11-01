// enviar.c
//	prueba de buzones: lee un mensaje de la consola y lo envia al buzon
//	que tiene como llave el carne, el mensaje lo puede recibir nachos o linux

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

   // leer el mensaje y quitarle el cambio de linea del enter
   escribir( "mensaje a enviar: " );
   n = Read( mensaje, 99, ConsoleInput );
   if ( n < 0 ) {
      n = 0;
   }
   if ( n > 0 && mensaje[ n - 1 ] == '\n' ) {
      n--;
   }

   if ( MsgSnd( buzon, n, mensaje ) == 0 ) {
      escribir( "enviar: mensaje enviado\n" );
   } else {
      escribir( "enviar: no se pudo enviar\n" );
   }

   Exit( 0 );
}
