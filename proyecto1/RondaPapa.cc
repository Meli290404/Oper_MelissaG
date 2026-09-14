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

#include "Buzon.h"	// Lo empezamos a usar de verdad a partir del próximo paso

#define MaxParticipantes 10

// Variable global para el número de participantes
int participantes = MaxParticipantes;

/**
  *  Se usa solo un buzón (una sola cola, con la llave = mi carnet, ver
  *  Buzon.h) para todo el mundo: participantes, main e invasor. Lo que
  *  distingue "para quién es" cada mensaje es el campo mtype, el participante con identificador
  *  id (0..n-1) solo recibe mensajes con mtype == DESTINO(id).
  *
  *  El invasor tiene su propio lugar en participantes+1, así main
  *  también le puede avisar cuando el juego terminó.
  *
 **/
#define DESTINO( id )	( (id) + 1 )		// mtype nunca puede ser 0, por eso el +1
#define BUZON_INVASOR	DESTINO( participantes )	// apartamento reservado del invasor

#define MAIN_ID		( -1 )		// origen que usa "main" cuando arranca la ronda

/**
  *  Estructura para el paso de mensajes entre procesos
 **/
struct RondaPapa {
   long mtype;	// = DESTINO(id) de quien debe recibir este mensaje
   int  papa;	// valor actual de la papa; si es NEGATIVO, el juego terminó
   int  origen;	// quién manda: mi id (0..n-1), o MAIN_ID si lo manda main

   // No se incluye el "tipo de mensaje" (normal vs invasor) aquí
   // a propósito. Un participante no debe poder confiar en un campo que
   // el invasor también puede llenar; la única defensa real es comparar
   // "origen" contra el emisor que registré como válido en el primer
   // mensaje recibido.
};


/**
  *  Aplica las reglas de Collatz al valor de la papa
  *
 **/
int cambiarPapa( int papa ) {

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

   _exit( 0 );	// Everything OK

}


/**
  *   Código para el invasor
  *   Manda mensajes al azar a los participantes de la ronda
  *
 **/
int invasor( int id ) {

   _exit( 0 );	// Everything OK

}


int main( int argc, char ** argv ) {
   int buzon, id, i, j, resultado;

   if ( argc > 1 ) {
      participantes = atoi( argv[ 1 ] );
   }
   if ( participantes <= 0 ) {
      participantes = MaxParticipantes;
   }

   srandom( getpid() );

   printf( "Creando una ronda de %d participantes\n", participantes );
   for ( i = 1; i <= participantes; i++ ) {
      if ( ! fork() ) {
         participante( i );
      }
   }

// El programa principal decidirá cual es el primer participante en arrancar y el valor inicial de la papa

// Creación del proceso invasor
   if ( ! fork() ) {
      invasor( i );
   }

// Espera que los participantes finalicen
   for ( i = 1; i <= participantes; i++ ) {
      j = wait( &resultado );
   }
   
   j = wait( &resultado );  // Espera por el invador

}