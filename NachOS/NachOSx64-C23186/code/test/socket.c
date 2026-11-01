#include "syscall.h"


char * strcat(char *s, const char *append) {
   char *save = s;
   for (; *s; ++s);
   while ((*s++ = *append++) != '\0');
   return(save);
}

int strlen(const char *str) {
   const char *s;

   for (s = str; *s; ++s)
                ;
   return (s - str);
}


int main() {
   int id;
   int n;
   char a[ 512 ];
   char req[ 300 ];		// el pedido se arma en un arreglo propio, no sobre un texto constante
   char s[ 50 ];

   req[ 0 ] = 0;
   strcat( req, "GET /aArt/index.php?disk=Disk-01&fig=" );

   id = Socket( AF_INET_NachOS, SOCK_STREAM_NachOS );
   Connect( id, "163.178.104.62", 80 );
   Write( "Figura: ", 8, 1 );

   // read trae tambien el cambio de linea del enter, se cambia por el fin del string
   n = Read( s, 49, 0 );
   if ( n < 0 ) {
      n = 0;
   }
   if ( n > 0 && s[ n - 1 ] == '\n' ) {
      n--;
   }
   s[ n ] = 0;

   strcat( req, s );
   strcat( req, " HTTP/1.0\r\nUser-Agent: nachos\r\n\r\n" );
   Write( req, strlen( req ), 1 );
   Write( req, strlen( req ), id );

   // leer la respuesta completa, read devuelve 0 cuando el servidor cierra la conexion
   while ( ( n = Read( a, 512, id ) ) > 0 ) {
      Write( a, n, 1 );
   }

   Close( id );
}

