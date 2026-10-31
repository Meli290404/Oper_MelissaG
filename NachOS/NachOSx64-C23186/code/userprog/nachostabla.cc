// nachostabla.cc
//	implementacion de la tabla de archivos abiertos de un proceso nachos
//
// Copyright (c) -2026 Universidad de Costa Rica

#include <stdio.h>
#include <unistd.h>

#include "nachostabla.h"

//----------------------------------------------------------------------
// NachosOpenFilesTable::NachosOpenFilesTable
//	los identificadores 0, 1 y 2 quedan reservados para la consola
//----------------------------------------------------------------------

NachosOpenFilesTable::NachosOpenFilesTable()
{
    openFiles = new int[ MAX_ARCHIVOS_ABIERTOS ];
    openFilesMap = new BitMap( MAX_ARCHIVOS_ABIERTOS );

    for ( int i = 0; i < MAX_ARCHIVOS_ABIERTOS; i++ ) {
        openFiles[ i ] = -1;
    }

    // entrada, salida y error de la consola apuntan a los de unix
    for ( int i = 0; i < 3; i++ ) {
        openFiles[ i ] = i;
        openFilesMap->Mark( i );
    }

    usage = 0;
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::~NachosOpenFilesTable
//----------------------------------------------------------------------

NachosOpenFilesTable::~NachosOpenFilesTable()
{
    delete [] openFiles;
    delete openFilesMap;
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::Open
//	busca un identificador libre, guarda ahi el descriptor unix
//	y devuelve el identificador, o -1 si la tabla esta llena
//----------------------------------------------------------------------

int NachosOpenFilesTable::Open( int UnixHandle )
{
    int NachosHandle = openFilesMap->Find();

    if ( NachosHandle != -1 ) {
        openFiles[ NachosHandle ] = UnixHandle;
    }

    return NachosHandle;
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::Close
//	libera el identificador y devuelve el descriptor unix para cerrarlo,
//	la consola no se puede cerrar
//----------------------------------------------------------------------

int NachosOpenFilesTable::Close( int NachosHandle )
{
    int UnixHandle = -1;

    if ( NachosHandle > 2 && isOpened( NachosHandle ) ) {
        UnixHandle = openFiles[ NachosHandle ];
        openFiles[ NachosHandle ] = -1;
        openFilesMap->Clear( NachosHandle );
    }

    return UnixHandle;
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::isOpened
//----------------------------------------------------------------------

bool NachosOpenFilesTable::isOpened( int NachosHandle )
{
    if ( NachosHandle < 0 || NachosHandle >= MAX_ARCHIVOS_ABIERTOS ) {
        return false;
    }

    return openFilesMap->Test( NachosHandle );
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::getUnixHandle
//----------------------------------------------------------------------

int NachosOpenFilesTable::getUnixHandle( int NachosHandle )
{
    if ( ! isOpened( NachosHandle ) ) {
        return -1;
    }

    return openFiles[ NachosHandle ];
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::addThread
//----------------------------------------------------------------------

void NachosOpenFilesTable::addThread()
{
    usage++;
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::delThread
//	cuando devuelve cero el ultimo hilo debe cerrar los archivos y borrar la tabla
//----------------------------------------------------------------------

int NachosOpenFilesTable::delThread()
{
    usage--;
    return usage;
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::closeAll
//	cierra en unix todo lo que el programa dejo abierto, menos la consola
//----------------------------------------------------------------------

void NachosOpenFilesTable::closeAll()
{
    for ( int i = 3; i < MAX_ARCHIVOS_ABIERTOS; i++ ) {
        if ( isOpened( i ) ) {
            close( Close( i ) );
        }
    }
}

//----------------------------------------------------------------------
// NachosOpenFilesTable::Print
//----------------------------------------------------------------------

void NachosOpenFilesTable::Print()
{
    printf( "tabla de archivos abiertos, %d hilos la usan\n", usage );
    for ( int i = 0; i < MAX_ARCHIVOS_ABIERTOS; i++ ) {
        if ( isOpened( i ) ) {
            printf( "\tnachos %d -> unix %d\n", i, openFiles[ i ] );
        }
    }
}