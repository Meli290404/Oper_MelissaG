// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Copyright (c) -2026 Universidad de Costa Rica

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    paginasPila = divRoundUp( UserStackSize, PageSize );
    usuarios = new int;
    *usuarios = 1;			// este hilo usa el codigo y los datos

    // revisar que haya suficientes marcos libres para todo el programa
    ASSERT( numPages <= (unsigned int) machine->mapaMemoria->NumClear() );

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);

    // tabla de paginas: cada pagina virtual recibe un marco libre del bitmap
    this->pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;
	pageTable[i].physicalPage = machine->mapaMemoria->Find();	// busca un marco libre y lo marca ocupado
	pageTable[i].valid = true;
	pageTable[i].use = false;
	pageTable[i].dirty = false;
	pageTable[i].readOnly = false;

	// limpiar el marco para que los datos no inicializados y la pila empiecen en cero
	bzero( &machine->mainMemory[ pageTable[i].physicalPage * PageSize ], PageSize );

	DEBUG( 'v', "pagina virtual %d -> marco %d\n", i, pageTable[i].physicalPage );
    }

    // copiar el codigo y los datos inicializados pagina por pagina
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        cargarSegmento( executable, noffH.code.virtualAddr, noffH.code.size, noffH.code.inFileAddr );
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        cargarSegmento( executable, noffH.initData.virtualAddr, noffH.initData.size, noffH.initData.inFileAddr );
    }

}

//----------------------------------------------------------------------
// AddrSpace::cargarSegmento
// 	copia un segmento del ejecutable a memoria, un pedazo de pagina a la vez,
//	porque las paginas virtuales seguidas pueden estar en marcos separados
//----------------------------------------------------------------------

void AddrSpace::cargarSegmento( OpenFile * ejecutable, int direccionVirtual, int tamano, int posicionArchivo )
{
    while ( tamano > 0 ) {
        int pagina = direccionVirtual / PageSize;
        int desplazamiento = direccionVirtual % PageSize;
        int cantidad = PageSize - desplazamiento;	// lo que falta para terminar esta pagina

        if ( cantidad > tamano ) {
            cantidad = tamano;
        }

        int direccionFisica = pageTable[ pagina ].physicalPage * PageSize + desplazamiento;
        ejecutable->ReadAt( &machine->mainMemory[ direccionFisica ], cantidad, posicionArchivo );

        direccionVirtual += cantidad;
        posicionArchivo += cantidad;
        tamano -= cantidad;
    }
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	espacio de direcciones para un hilo creado con fork:
//	las paginas de codigo y datos apuntan a los mismos marcos del padre
//	y solo las paginas de la pila reciben marcos nuevos
//----------------------------------------------------------------------

AddrSpace::AddrSpace( AddrSpace * padre )
{
    numPages = padre->numPages;
    paginasPila = padre->paginasPila;
    unsigned int paginasCompartidas = numPages - paginasPila;

    // revisar que haya marcos libres para la pila del hijo
    ASSERT( paginasPila <= (unsigned int) machine->mapaMemoria->NumClear() );

    // el hijo se suma a los usuarios del codigo y los datos
    usuarios = padre->usuarios;
    (*usuarios)++;

    pageTable = new TranslationEntry[ numPages ];

    // codigo y datos: mismos marcos que el padre
    for ( unsigned int i = 0; i < paginasCompartidas; i++ ) {
        pageTable[i] = padre->pageTable[i];
        DEBUG( 'v', "hijo: pagina virtual %d -> marco %d compartido\n", i, pageTable[i].physicalPage );
    }

    // pila: marcos nuevos y limpios
    for ( unsigned int i = paginasCompartidas; i < numPages; i++ ) {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = machine->mapaMemoria->Find();
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
        bzero( &machine->mainMemory[ pageTable[i].physicalPage * PageSize ], PageSize );
        DEBUG( 'v', "hijo: pagina virtual %d -> marco %d de pila\n", i, pageTable[i].physicalPage );
    }
}

//----------------------------------------------------------------------
// AddrSpace::paginasNecesarias
// 	lee el encabezado del ejecutable y calcula cuantas paginas ocuparia,
//	sirve para revisar si hay memoria antes de crear el espacio
//----------------------------------------------------------------------

unsigned int AddrSpace::paginasNecesarias( OpenFile * ejecutable )
{
    NoffHeader noffH;
    unsigned int size;

    ejecutable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);

    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;
    return divRoundUp( size, PageSize );
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   unsigned int paginasCompartidas = numPages - paginasPila;

   // la pila es solo de este hilo, sus marcos siempre se devuelven
   for ( unsigned int i = paginasCompartidas; i < numPages; i++ ) {
      machine->mapaMemoria->Clear( pageTable[i].physicalPage );
   }

   // el codigo y los datos se devuelven solo cuando ya ningun hilo los usa
   (*usuarios)--;
   if ( *usuarios == 0 ) {
      for ( unsigned int i = 0; i < paginasCompartidas; i++ ) {
         machine->mapaMemoria->Clear( pageTable[i].physicalPage );
      }
      delete usuarios;
   }

   delete [] this->pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
