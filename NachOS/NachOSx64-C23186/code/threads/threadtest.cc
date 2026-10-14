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
Semaphore * terminados; // el main espera a que acaben los filósofos

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



//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest()
{
    Thread * Ph;

    DEBUG('t', "Entering SimpleTest");

    dp = new DiningPh();
    terminados = new Semaphore("terminados", 0);

    for ( long k = 0; k < 5; k++ ) {
        char * name = new char[20];
        sprintf( name, "Philosopher %ld", k + 1 );
        Ph = new Thread( name );
        Ph->Fork( Philo, (void *) k );
    }

    for (int k = 0; k < 5; k++) {
        terminados->P(); // espera a que terminen los filósofos
    }

    return;

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

}

