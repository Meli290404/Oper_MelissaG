/*
 *  agua.c
 *	problema del agua con hilos de usuario y semaforos de nachos
 *	cada atomo es un hilo creado con fork,
 *	dos atomos de hidrogeno y uno de oxigeno forman una molecula
 */

#include "syscall.h"

#define MOLECULAS 3
#define ATOMOS 9

int cH;		// hidrogenos esperando pareja 
int cO;		// oxigenos esperando pareja
int moleculas;

Sem_t mutex;		// protege cH, cO y moleculas
Sem_t sH;		// aqui esperan los hidrogenos
Sem_t sO;		// aqui esperan los oxigenos
Sem_t terminados;	// cada atomo avisa a main cuando termina


// cantidad de caracteres de un string 
int largo( char * texto ) {
   int n = 0;

   while ( texto[ n ] != 0 ) {
      n++;
   }

   return n;
}


// escribe un string en la consola 
void escribir( char * texto ) {
   Write( texto, largo( texto ), ConsoleOutput );
}


// escribe un numero no negativo en la consola
void escribirNumero( int numero ) {
   char digitos[ 12 ];
   int i = 11;

   digitos[ i ] = 0;
   if ( numero == 0 ) {
      i--;
      digitos[ i ] = '0';
   }
   while ( numero > 0 ) {
      i--;
      digitos[ i ] = '0' + numero % 10;
      numero = numero / 10;
   }

   escribir( &digitos[ i ] );
}


// escribe cuantos atomos estan esperando
void escribirEstado() {
   escribir( "[O=" );
   escribirNumero( cO );
   escribir( ", H=" );
   escribirNumero( cH );
   escribir( "]\n" );
}


// codigo de cada hilo de hidrogeno
void H() {
   SemWait( mutex );
   escribir( "llega H " );
   escribirEstado();

   if ( cH > 0 && cO > 0 ) {		// hay un h y un o esperando, se forma agua 
      cH--;
      cO--;
      moleculas++;
      escribir( "\t*** un H formo la molecula " );
      escribirNumero( moleculas );
      escribir( " " );
      escribirEstado();
      SemSignal( sH );
      SemSignal( sO );
      SemSignal( mutex );
   } else {				// falta pareja, espera
      cH++;
      SemSignal( mutex );
      SemWait( sH );
   }

   SemSignal( terminados );
   Exit( 0 );
}


// codigo de cada hilo de oxigeno
void O() {
   SemWait( mutex );
   escribir( "llega O " );
   escribirEstado();

   if ( cH > 1 ) {			// hay dos h esperando, se forma agua
      cH = cH - 2;
      moleculas++;
      escribir( "\t*** un O formo la molecula " );
      escribirNumero( moleculas );
      escribir( " " );
      escribirEstado();
      SemSignal( sH );
      SemSignal( sH );
      SemSignal( mutex );
   } else {				// falta pareja, espera
      cO++;
      SemSignal( mutex );
      SemWait( sO );
   }

   SemSignal( terminados );
   Exit( 0 );
}


int main() {
   char * orden = "HOOHHHOHH";		// orden en que llegan los atomos, 6 h y 3 o
   int i;

   cH = 0;
   cO = 0;
   moleculas = 0;

   mutex = SemCreate( 1 );
   sH = SemCreate( 0 );
   sO = SemCreate( 0 );
   terminados = SemCreate( 0 );

   for ( i = 0; i < ATOMOS; i++ ) {
      if ( orden[ i ] == 'H' ) {
         Fork( H );
      } else {
         Fork( O );
      }
   }

   // esperar a que todos los atomos terminen
   for ( i = 0; i < ATOMOS; i++ ) {
      SemWait( terminados );
   }

   escribir( "\nse formaron " );
   escribirNumero( moleculas );
   escribir( " moleculas de agua, quedan " );
   escribirEstado();

   SemDestroy( mutex );
   SemDestroy( sH );
   SemDestroy( sO );
   SemDestroy( terminados );

   Exit( 0 );
}
