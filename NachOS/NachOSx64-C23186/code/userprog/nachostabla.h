// nachostabla.h
//	tabla de archivos abiertos de un proceso nachos
//	relaciona el identificador que ve el programa de usuario
//	con el descriptor real de unix
//
// Copyright (c) -2026 Universidad de Costa Rica

#ifndef NACHOSTABLA_H
#define NACHOSTABLA_H

#include "bitmap.h"

#define MAX_ARCHIVOS_ABIERTOS 32

class NachosOpenFilesTable {
  public:
    NachosOpenFilesTable();		// crea la tabla con la consola ya abierta
    ~NachosOpenFilesTable();		// libera las estructuras de la tabla

    int Open( int UnixHandle );		// registra un descriptor unix y devuelve el identificador nachos
    int Close( int NachosHandle );	// libera el identificador y devuelve el descriptor unix o -1
    bool isOpened( int NachosHandle );	// indica si el identificador esta en uso
    int getUnixHandle( int NachosHandle );	// devuelve el descriptor unix o -1

    void addThread();			// otro hilo empieza a usar esta tabla
    int delThread();			// un hilo deja de usar la tabla, devuelve cuantos quedan
    void closeAll();			// cierra todos los descriptores unix que siguen abiertos

    void Print();			// imprime la tabla para depurar

  private:
    int * openFiles;			// descriptor unix de cada identificador nachos
    BitMap * openFilesMap;		// identificadores ocupados
    int usage;				// cantidad de hilos que comparten la tabla
};

#endif // NACHOSTABLA_H