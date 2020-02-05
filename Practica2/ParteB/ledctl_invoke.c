#include <linux/errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <stdlib.h>


#define __NR_LEDCTL		332	// Observar la "syscall_64.tbl" y ver cual es el valor que pondremos


long ledctl(unsigned int leds);

int main(int argc, char* argv[]){
	unsigned int mascaraBits;
	int resul = -1;

	// Nuestro comando de uso es "ledctl_invoke <mascaraDeBits>" y argc contiene valor 2 (argv[0]=nombreDelPrograma / argv[1]=parametro)
	if(argc >= 2)
	{
		if(sscanf(argv[1], "%x", &mascaraBits) == 1)
		{
			if(mascaraBits>=0 && mascaraBits<=7)
			{
				resul = ledctl(mascaraBits);
				if(resul < 0)
					perror("Se ha producido un error inesperado.");
			}
			else
			{
				fprintf(stderr, "Introduzca un valor hexadecimal.\n");
				resul = 1;
			}
		}
		else
		{
			fprintf(stderr, "Parametro no valido.\n");
			resul = 1;
		}
	}
	else
		perror("Parametro no introducido.");

	return resul;
}

long ledctl(unsigned int leds){
	return (long)syscall(__NR_LEDCTL, leds);
}
