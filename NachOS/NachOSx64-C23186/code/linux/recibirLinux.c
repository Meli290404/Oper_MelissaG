// recibirLinux.c
//	programa de linux que espera un mensaje en el buzon de nachos,
//	lo escribe y destruye el buzon
//	compilar: gcc recibirLinux.c -o recibirLinux
//	usar: ./recibirLinux

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define LLAVE 23186		// carne c23186, la misma llave que usan enviar.c y recibir.c
#define MAX_MENSAJE 512

struct MensajeBuzon {
   long tipo;
   char texto[ MAX_MENSAJE ];
};

int main() {
   struct MensajeBuzon mensaje;
   int buzon;
   int recibidos;

   buzon = msgget( LLAVE, IPC_CREAT | 0600 );
   if ( buzon == -1 ) {
      perror( "msgget" );
      return 1;
   }

   printf( "recibirLinux: esperando un mensaje en el buzon %d\n", buzon );
   recibidos = msgrcv( buzon, &mensaje, MAX_MENSAJE, 0, MSG_NOERROR );
   if ( recibidos == -1 ) {
      perror( "msgrcv" );
      return 1;
   }

   printf( "recibirLinux: llego el mensaje: %.*s\n", recibidos, mensaje.texto );
   msgctl( buzon, IPC_RMID, NULL );
   return 0;
}