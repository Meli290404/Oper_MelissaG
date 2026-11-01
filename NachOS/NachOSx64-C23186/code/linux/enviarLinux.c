// enviarLinux.c
//	programa de linux que envia un mensaje al buzon de nachos,
//	usa la misma llave y el mismo formato de mensaje que el kernel de nachos
//	compilar: gcc enviarLinux.c -o enviarLinux
//	usar: ./enviarLinux "texto del mensaje"

#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define LLAVE 23186		// carne c23186, la misma llave que usan enviar.c y recibir.c
#define MAX_MENSAJE 512

struct MensajeBuzon {
   long tipo;
   char texto[ MAX_MENSAJE ];
};

int main( int argc, char ** argv ) {
   struct MensajeBuzon mensaje;
   const char * texto = "hola desde linux";
   int buzon;
   int tamano;

   if ( argc > 1 ) {
      texto = argv[ 1 ];
   }

   buzon = msgget( LLAVE, IPC_CREAT | 0600 );
   if ( buzon == -1 ) {
      perror( "msgget" );
      return 1;
   }

   tamano = strlen( texto );
   if ( tamano > MAX_MENSAJE ) {
      tamano = MAX_MENSAJE;
   }
   mensaje.tipo = 1;
   memcpy( mensaje.texto, texto, tamano );

   if ( msgsnd( buzon, &mensaje, tamano, 0 ) == -1 ) {
      perror( "msgsnd" );
      return 1;
   }

   printf( "enviarLinux: mensaje enviado al buzon %d\n", buzon );
   return 0;
}