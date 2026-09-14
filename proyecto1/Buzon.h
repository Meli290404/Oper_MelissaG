/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-ii
  *
  *   Class interface
  *
 **/

#include <sys/types.h>	// pid_t definition

// Uso mi carnet C23186 como llave, le quito la "C" porque IPC_PRIVATE/msgget
// necesitan un entero (key_t), no puede llevar letras.
#define KEY 23186	// Valor de la llave del recurso (carnet C23186)

class Buzon {
   public:
      Buzon();
      ~Buzon();
      int Enviar( const char *mensaje, long = 1 );
      int Enviar( const void *mensaje, int, long = 1 );
      int Recibir( void *mensaje, int, long = 1, bool = true );	// len: space in mensaje; el ultimo bool: true = bloqueante, false = no bloqueante

   private:
      int id;		// Identificador del buzon
      pid_t owner;	// Mailbox owner

};