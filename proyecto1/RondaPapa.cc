/**
  *  Ejemplo base para el problema de la ronda o papa caliente
  *
  *  CI-0122 Sistemas Operativos
  *  Fecha: 2026/Ago/12
  *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include "Buzon.h"	

#define MaxParticipantes 10

// Variable global para el número de participantes
int participantes = MaxParticipantes;

/**
  *  --- Protocolo de mensajes de la ronda ---
  *
  *  Se usa un solo buzón (una sola cola, con la llave = mi carnet, ver
  *  Buzon.h) para todo el mundo: participantes, main e invasor. Lo que
  *  distingue "para quién es" cada mensaje es el parámetro "tipo" que ya
  *  recibe Buzon::Enviar/Recibir,el participante con identificador id (0..n-1)
  *  solo recibe mensajes con tipo == DESTINO(id).
  *
  *
  *  El invasor tiene su propio lugar (participantes+1), y hay un
  *  lugar más (participantes+2) que se usa como CONTADOR de cuántos
  *  participantes siguen activos.
  *
 **/
#define DESTINO( id )		( (id) + 1 )		// mtype nunca puede ser 0, por eso el +1
#define BUZON_INVASOR		DESTINO( participantes )	// apartamento reservado del invasor
#define BUZON_CONTADOR		( BUZON_INVASOR + 1 )		// aqui vive el contador de activos

// Direcciones de giro posibles
#define CLOCKWISE		0
#define COUNTER_CLOCKWISE	1

int direccion = CLOCKWISE;	// la fija main() antes de los fork()
Buzon * buzonGlobal = NULL;	// main() lo crea antes de los fork()

/**
  *  Estructura para el paso de mensajes entre procesos
 **/
struct RondaPapa {
   long papa;	// valor actual de la papa; si es NEGATIVO, el juego terminó
   int  origen;	// quién manda: el id (0..n-1) de quien reenvió este mensaje
};


/**
  *  A quién le paso la papa si soy "id", según la dirección de giro
  *
 **/
int vecinoSiguiente( int id ) {

   if ( CLOCKWISE == direccion ) {
      return ( id + 1 ) % participantes;
   }
   return ( id - 1 + participantes ) % participantes;

}


/**
  *  Quién me la debería pasar, según la dirección de giro.
  *  Solo la usa main() para "disfrazarse" de mi vecino real cuando manda
  *  el mensaje con el que arranca la ronda, así el participante
  *  que arranca registra a su vecino verdadero, y no a "main", como su
  *  emisor válido.
  *
 **/
int vecinoAnterior( int id ) {

   if ( CLOCKWISE == direccion ) {
      return ( id - 1 + participantes ) % participantes;
   }
   return ( id + 1 ) % participantes;

}


/**
  *  Aplica las reglas de Collatz al valor de la papa
  *
 **/
long cambiarPapa( long papa ) {

   if ( 1 == (papa & 0x1) ) {		// papa es impar
            papa = (papa << 1) + papa + 1;	// papa = papa * 2 + papa + 1
         } else {
            papa >>= 1;			// n = n / 2, utiliza corrimiento a la derecha, una posicion
         }

   return papa;

}


/**
  *   Código para cada participante
  *   Debe cambiar el valor de la papa y determinar si explotó
 **/
int participante( int id ) {

   RondaPapa mensaje;
   int siguiente    = vecinoSiguiente( id );
   int vecinoValido = 0;		// se llena con el primer mensaje que reciba
   bool registrado  = false;	// ¿ya aprendí quién es mi vecino válido?
   bool activo      = true;	// empiezo activo; cuando "exploto" paso a pasivo
   int contador;

   while ( true ) {

      // Bloqueante: solo me entregan lo dirigido a mi id
      buzonGlobal->Recibir( &mensaje, sizeof( mensaje ), DESTINO( id ) );

      if ( ! registrado ) {
         // El enunciado garantiza que el primer mensaje de la ronda es
         // válido, así que lo aprovecho para "aprender" quién es mi vecino real, sin tener que
         // calcularlo yo mismo.
         vecinoValido = mensaje.origen;
         registrado = true;
         printf( "Participante %d: registro a %d como mi vecino valido\n", id, vecinoValido );
      } else if ( mensaje.origen != vecinoValido ) {
         // No es mi vecino registrado -> viene del invasor, lo descarto
         // y me quedo esperando el mensaje real
         printf( "Participante %d: descarto mensaje falso de %d (esperaba %d)\n",
                 id, mensaje.origen, vecinoValido );
         continue;
      }

      if ( mensaje.papa < 0 ) {
         // Fin del juego: lo reenvío tal cual para que le llegue a los
         // que faltan en el anillo, y termino mi ejecución
         printf( "Participante %d: me avisaron que el juego termino, salgo\n", id );
         mensaje.origen = id;
         buzonGlobal->Enviar( &mensaje, sizeof( mensaje ), DESTINO( siguiente ) );
         break;
      }

      // Pausa chiquita para que la ronda tenga un ritmo observable
      usleep( 50000 + random() % 100000 );

      if ( activo ) {

         mensaje.papa = cambiarPapa( mensaje.papa );
         printf( "Participante %d (activo): nuevo valor de la papa = %ld\n", id, mensaje.papa );

         if ( 1 == mensaje.papa ) {
            // Exploté: paso a pasivo, pero sigo vivo reenviando
            // (si hago _exit aquí, rompo el anillo para mi vecino)
            activo = false;
            printf( "Participante %d: la papa exploto, ahora soy pasivo\n", id );

            // Uso el buzón como si fuera un contador compartido: le resto
            // uno al número de activos que quedan. Como la papa solo la
            // tiene un proceso a la vez, no hay condición de carrera aquí.
            buzonGlobal->Recibir( &contador, sizeof( contador ), BUZON_CONTADOR );
            contador--;

            if ( 0 == contador ) {
               // Fui el último activo en salir -> gano yo
               printf( "*** GANADOR: participante %d, el ultimo en salir ***\n", id );
               mensaje.papa = -1;		// aviso de fin de juego
               mensaje.origen = id;
               buzonGlobal->Enviar( &mensaje, sizeof( mensaje ), BUZON_INVASOR ); // que el invasor tambien pare
            } else {
               // Todavía quedan activos: le dejo el contador al que siga
               buzonGlobal->Enviar( &contador, sizeof( contador ), BUZON_CONTADOR );
               mensaje.papa = 2 + random() % 500;	// nuevo valor al azar para que el juego siga
            }
         }

      }
      // Si soy pasivo, no toco mensaje.papa: solo lo reenvío tal cual

      mensaje.origen = id;
      buzonGlobal->Enviar( &mensaje, sizeof( mensaje ), DESTINO( siguiente ) );

   }

   _exit( 0 );	// Everything OK

}


/**
  *   Código para el invasor
  *   Manda mensajes al azar a los participantes de la ronda
  *
 **/
int invasor( int id ) {

   RondaPapa mensaje;
   int destino, origenFalso;

   srandom( getpid() );

   // El invasor tarda un poco en arrancar. Mientras
   // tanto, los mensajes de la primera vuelta sí vienen de emisores
   // válidos, tal como lo garantiza el enunciado.
   sleep( 2 + random() % 3 );

   while ( true ) {

      // Reviso sin bloquear si ya me avisaron que el juego termino
      if ( -1 != buzonGlobal->Recibir( &mensaje, sizeof( mensaje ), BUZON_INVASOR, false ) ) {
         printf( "Invasor: me avisaron que el juego termino, salgo\n" );
         break;
      }

      // Invento un mensaje: a quién se lo mando y de parte de quién
      // dice venir (el participante lo va a comparar contra el vecino
      // que registró de verdad, y si no coincide lo descarta)
      destino     = random() % participantes;
      origenFalso = random() % participantes;

      mensaje.papa   = 1 + random() % 9999;
      mensaje.origen = origenFalso;

      printf( "Invasor: mando mensaje falso a %d, dice venir de %d\n", destino, origenFalso );
      buzonGlobal->Enviar( &mensaje, sizeof( mensaje ), DESTINO( destino ) );

      sleep( 1 + random() % 2 );

   }

   _exit( 0 );	// Everything OK

}


int main( int argc, char ** argv ) {
   int i, resultado;
   int arranque;
   long valorInicial;
   RondaPapa semilla;

   // Sin esto, cada fork() se lleva una copia del buffer de salida
   // pendiente, y cuando cada hijo lo vacia por su cuenta, se repiten
   // lineas que en realidad solo se imprimieron "a medias" antes del fork
   setvbuf( stdout, NULL, _IONBF, 0 );

   if ( argc > 1 ) {
      participantes = atoi( argv[ 1 ] );
   }
   if ( participantes <= 0 ) {
      participantes = MaxParticipantes;
   }

   srandom( getpid() );

   // Direccion de giro: la leo del tercer parametro si vino, si no la genero al azar
   if ( argc > 3 && 0 == strcmp( argv[ 3 ], "counter-clockwise" ) ) {
      direccion = COUNTER_CLOCKWISE;
   } else if ( argc > 3 ) {
      direccion = CLOCKWISE;	// vino algo, pero no "counter-clockwise" -> asumo clockwise
   } else {
      direccion = ( random() % 2 ) ? COUNTER_CLOCKWISE : CLOCKWISE;
   }

   // Valor inicial de la papa: viene del segundo parametro, o lo invento
   if ( argc > 2 && atol( argv[ 2 ] ) > 0 ) {
      valorInicial = atol( argv[ 2 ] );
   } else {
      valorInicial = 2 + random() % 9999;
   }

   printf( "Creando una ronda de %d participantes, papa inicial = %ld, direccion = %s\n",
           participantes, valorInicial, ( CLOCKWISE == direccion ) ? "clockwise" : "counter-clockwise" );

   // Genero el buzon (la cola de mensajes) antes de los fork, asi todos
   // los hijos heredan una copia de este objeto, apuntando a la misma
   // cola del kernel
   buzonGlobal = new Buzon();

   // Creo los participantes, con id 0..participantes-1 (asi lo pide el enunciado)
   for ( i = 0; i < participantes; i++ ) {
      if ( ! fork() ) {
         participante( i );
      }
   }

   // Creacion del proceso invasor, le doy id = participantes (asi
   // BUZON_INVASOR = DESTINO(participantes) le llega solo a el)
   if ( ! fork() ) {
      invasor( participantes );
   }

   // Inicializo el contador de activos, lo van a usar los participantes
   // para saber quien es el ultimo en salir
   buzonGlobal->Enviar( &participantes, sizeof( participantes ), BUZON_CONTADOR );

   // Elijo al azar quien arranca la ronda
   arranque = random() % participantes;

   // Mando el mensaje semilla, "disfrazado" del vecino real de quien
   // arranca, para que lo registre como su vecino valido sin necesitar
   // casos especiales dentro de participante()
   semilla.papa   = valorInicial;
   semilla.origen = vecinoAnterior( arranque );
   buzonGlobal->Enviar( &semilla, sizeof( semilla ), DESTINO( arranque ) );

   printf( "Arranca el participante %d con papa = %ld\n", arranque, valorInicial );

   // main no participa en la ronda ni en las decisiones de sincronizacion,
   // solo espera a que todos los hijos (participantes + invasor) terminen
   for ( i = 0; i < participantes + 1; i++ ) {
      wait( &resultado );
   }

   // Ya terminaron todos, elimino el buzon
   delete buzonGlobal;

   return 0;

}