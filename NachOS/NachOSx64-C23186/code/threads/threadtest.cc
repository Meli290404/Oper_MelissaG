// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create several threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustrate the inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
//
// Copyright (c) -2026 Universidad de Costa Rica

#include <unistd.h>
#include "copyright.h"
#include "system.h"
#include "diningph.h"

DiningPh * dp;
Semaphore * terminados; // el main espera a que acaben los hilos

void Philo( void * p ) {

    int eats, thinks;
    long who = (long) p;

    currentThread->Yield();

    for ( int i = 0; i < 10; i++ ) {

        printf(" Philosopher %ld will try to pickup sticks\n", who + 1);

        dp->pickup( who );
        dp->print();
        eats = Random() % 6;

        currentThread->Yield();
        sleep( eats );

        dp->putdown( who );

        thinks = Random() % 6;
        currentThread->Yield();
        sleep( thinks );
    }

    terminados->V(); // avisa a el main que el filósofo terminó

}

int cO = 0;
int cH = 0;
Semaphore * sH; //aquí esperan los H sin pareja
Semaphore * sO; //aquí esperan los O sin pareja
Semaphore * mutexAgua; //protege c0, cH y moleculas
int moleculas = 0; //cuenta cuantas moléculas se han formado

void H( void * p ) {
    long id = (long) p;
    currentThread->Yield();

    mutexAgua->P();
    printf("Llega H%ld [O=%d, H=%d]\n", id, cO, cH);
    if (cH > 0 && cO > 0) {
        cH--;
        cO--;
        moleculas++;
        printf("Se forma agua con H%ld [O=%d, H=%d]\n", id, cO, cH);
        sO->V(); // se despierta al H que estaba esperando
        sH->V(); // se despierta al O que estaba esperando
        mutexAgua->V();
    } else {
        cH++;
        mutexAgua->V();
        sH->P();
    }

    terminados->V(); // avisa a el main que el H terminó
}

void O( void * p ) {
    long id = (long) p;
    currentThread->Yield();

    mutexAgua->P();
    printf("Llega O%ld [O=%d, H=%d]\n", id, cO, cH);
    if (cH > 1) {
        cH -= 2;
        moleculas++;
        printf("Se forma agua con O%ld [O=%d, H=%d]\n", id, cO, cH);
        sH->V(); // se despierta a un H que estaba esperando
        sH->V(); // se despierta a otro H que estaba esperando
        mutexAgua->V();
    } else {
        cO++;
        mutexAgua->V();
        sO->P();
    }
    terminados->V(); // avisa a el main que el O terminó
}

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 10 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"name" points to a string with a thread name, just for
//      debugging purposes.
//----------------------------------------------------------------------

void
SimpleThread(void* name)
{
    // Reinterpret arg "name" as a string
    char* threadName = (char*)name;
    
    // If the lines dealing with interrupts are commented,
    // the code will behave incorrectly, because
    // printf execution may cause race conditions.
    for (int num = 0; num < 3; num++) {
        //IntStatus oldLevel = interrupt->SetLevel(IntOff);
	printf( "*** thread %s looped %d times\n", currentThread->getName(), num );
	//interrupt->SetLevel(oldLevel);
        //currentThread->Yield();
    }
    //IntStatus oldLevel = interrupt->SetLevel(IntOff);
    printf( ">>> Thread %s has finished\n", threadName );
    //interrupt->SetLevel(oldLevel);

    currentThread->Yield();		// Yielding CPU will delete threads asynchronously

    currentThread->Finish();

}

void PruebaFilosofos() {
    Thread * Ph;
    dp = new DiningPh();

    for ( long k = 0; k < 5; k++ ) {
        char * name = new char[ 20 ];
        sprintf( name, "Filosofo %ld", k + 1 );
        Ph = new Thread( name );
        Ph->Fork( Philo, (void *) k );
    }

    for ( int k = 0; k < 5; k++ ) {
        terminados->P();
    }
}

void PruebaAgua() {
    const int MOLECULAS = 10;
    const int ATOMOS = 3 * MOLECULAS;
    int esOxigeno[ ATOMOS ];
    Thread * t;

    sH = new Semaphore( "sH", 0 );
    sO = new Semaphore( "sO", 0 );
    mutexAgua = new Semaphore( "mutexAgua", 1 );

    // N oxigenos y 2N hidrogenos, mezclados al azar 
    for ( int i = 0; i < ATOMOS; i++ ) {
        esOxigeno[ i ] = ( i < MOLECULAS );
    }
    for ( int i = ATOMOS - 1; i > 0; i-- ) {
        int j = Random() % ( i + 1 );
        int tmp = esOxigeno[ i ];
        esOxigeno[ i ] = esOxigeno[ j ];
        esOxigeno[ j ] = tmp;
    }

    long nH = 0, nO = 0;
    for ( int i = 0; i < ATOMOS; i++ ) {
        char * name = new char[ 20 ];
        if ( esOxigeno[ i ] ) {
            sprintf( name, "O%ld", nO );
            t = new Thread( name );
            t->Fork( O, (void *) nO++ );
        } else {
            sprintf( name, "H%ld", nH );
            t = new Thread( name );
            t->Fork( H, (void *) nH++ );
        }
    }

    for ( int i = 0; i < ATOMOS; i++ ) {   // esperar a todos los atomos
        terminados->P();
    }

    printf( "\nSe formaron %d moleculas de agua. Quedan O=%d, H=%d\n", moleculas, cO, cH );
}


//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest()
{
    DEBUG('t', "Entering SimpleTest");

    terminados = new Semaphore( "terminados", 0 );

    PruebaFilosofos();
    PruebaAgua();

/*  Código original de prueba (SimpleThread)
    char * threadName;
    Thread * newThread;
    for ( int k=1; k<5; k++ ) {
      threadName = new char[100];
      sprintf( threadName, "Hilo %d", k );
      newThread = new Thread( threadName );
      newThread->Fork ( SimpleThread, (void*) threadName );
    }

    currentThread->Yield();

    SimpleThread( (void*) "Hilo 0" );

    currentThread->Finish();
*/

}

