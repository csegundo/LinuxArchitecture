/*********************************************************************
*	Si hay solo un blinkstick conectado:							 *
*		1) gcc -pthread -Wall -g blindrv_user.c -o drv 				 *
*		2) sudo ./drv 												 *
*																	 *
*	Si hay 2 blinksticks conectados:								 *
*		1) gcc -pthread -D NEW_STICK -Wall -g blinkdrv_user.c -o drv *
*		2) sudo ./drv 												 *
**********************************************************************/

#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <linux/input.h>
#include <pthread.h> 

#define ENCENDER_TODO		"0:101010,1:101010,2:101010,3:101010,4:101010,5:101010,6:101010,7:101010,8:101010"
#define APAGAR_TODO			"0:000000,1:000000,2:000000,3:000000,4:000000,5:000000,6:000000,7:000000,8:000000"

#define DISPOSITIVO_1		"/dev/usb/blinkstick0"
int blinkstick0;

#define ARCOIRIS(index) for(index = 0; index < 20; ++index)

#ifdef NEW_STICK
#define DISPOSITIVO_2		"/dev/usb/blinkstick1"
int blinkstick1;
#endif

struct region{
    int xR;	int yU;
    int xL;	int yD;
};

const char* led1 = "0:0x101010";
const char* led2 = "1:0x101010";
const char* led3 = "2:0x101010";
const char* led4 = "3:0x101010";
const char* led5 = "4:0x101010";
const char* led6 = "5:0x101010";
const char* led7 = "6:0x101010";
const char* led8 = "7:0x101010";

// Colores disponibles para las regiones de la opcion 6
char* ledsPorRegion[16] = {"0xB73D3D", "0xB7643D", "0xB7943D", "0xB3B73D", "0x81994F", "0x5B8849", "0x3A7E4A", "0x296951", "0x29695D",
									"0x1D4359", "0x1D3059", "0x12144B", "0x382A62", "0x654876", "0x93288F", "0x95206D"};


int status_exit=0;

void getScreenSize(int* sizeX, int* sizeY);
struct region* createRegions();
void getMouseCood(int* x,int* y);
int inRegX(int x, struct region r);
int inRegY(int y, struct region r);
void mouseMovement();
int mostrarMenu();
void help();
int arcoiris();
int escribirArcoiris();
int apagarLeds();
int moverHaciaLaDerecha();
int moverHaciaLaIzquierda();
int getVolume();
void volume();
void stop();

int main(){
	int eleccionMenu;
	pthread_t t1;
	pthread_t t2;

	if ((blinkstick0 = open(DISPOSITIVO_1, O_WRONLY)) < 0)
		return -1;
	#ifdef NEW_STICK
		if ((blinkstick1 = open(DISPOSITIVO_2, O_WRONLY)) < 0)
			return -1;
	#endif
	
	eleccionMenu = mostrarMenu();

	while(eleccionMenu != 0){
		switch(eleccionMenu){
			case 1: help();
					break;
			case 2: arcoiris();
					break;
			case 3: moverHaciaLaDerecha();
					break;
			case 4: moverHaciaLaIzquierda();
					break;
			case 5: 
					pthread_create(&t1, NULL, volume, NULL); 
					pthread_create(&t2, NULL, stop, NULL);
					pthread_join(t2,NULL);
					break;
			case 6: 
					pthread_create(&t1, NULL, mouseMovement, NULL); 
					pthread_create(&t2, NULL, stop, NULL);
					pthread_join(t2,NULL);
					break;
		}
		if(eleccionMenu != 0) eleccionMenu = mostrarMenu();
	}

	close(blinkstick0);

	#ifdef NEW_STICK
		close(blinkstick1);
	#endif

	return 0;
}

int mostrarMenu() {
	char * lectura = malloc(10*sizeof(char));
	int opcionEscogida = 1;
	int numCorrectos = 1;
	struct passwd *userInfo = getpwuid(getuid());

	if(userInfo != NULL)
		printf("\n\tBienvenido %s!\n", userInfo->pw_name);


	printf("--------------------------------------------------\n");

	printf("1. Mostrar informacion\n");
	printf("2. Arcoiris\n");
	printf("3. Mover hacia la derecha   ----->\n");
	printf("4. Mover hacia la izquierda <-----\n");
	printf("5. Volumen\n");
	printf("6. Direccion raton\n");
	printf("0. Salir\n\n");
	printf("\tSeleccionar una opcion: ");

	do{
		if(numCorrectos != 1 || opcionEscogida < 0 || opcionEscogida > 5)
			printf("Opcion escogida erronea.\nIntroduce otra a continuacion: ");
		fgets(lectura,10, stdin);
		numCorrectos = sscanf(lectura,"%d",&opcionEscogida);
	}while(numCorrectos != 1 || opcionEscogida < 0 || opcionEscogida > 6);

	return opcionEscogida;
}

void help() {

	char op1[] = "1. Muestra este mensaje";
	char op2[] = "2. Muestra los colores del arcoiris en cascada";
	char op3[] = "3. Movimiento de los leds hacia la derecha -------->";
	char op4[] = "4. Movimiento de los leds hacia la izquierda <------";
	char op5[] = "5. Enciende los leds segun ajustemos el volumen del sistema";
	char op6[] = "6. Enciende los leds mientras detecte movimiento de raton";

	printf("\n\n%s\n%s\n%s\n%s\n%s\n%s\n\n", op1, op2, op3, op4, op5, op6);
}

void getScreenSize(int* sizeX, int* sizeY){
    int num;
    FILE *fp;
    char* buffer = malloc(100);  

    fp = popen("xrandr | grep '*' | cut -d 'x'  -f1 | cut --bytes=4-8","r"); //get sizeX
    if(fp){
        while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) { }
        if(sscanf(buffer,"%d",&num)!=1)
            printf("ERROR\n");
        *sizeX = num;
        pclose(fp);
    }
    fp = popen("xrandr | grep '*' | cut -d x -f2 | cut -d ' ' -f1","r"); //get sizeY
    if(fp){
        while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) { }
        if(sscanf(buffer,"%d",&num)!=1)
            printf("ERROR\n");
        *sizeY = num;
        pclose(fp);
    }
    free(buffer);
}

struct region* createRegions(){
    struct region* regions = malloc(sizeof(struct region)*16);
    int* sizeX = malloc(sizeof(int));
    int* sizeY = malloc(sizeof(int));
    int spacex,spacey;
    int xfirst,xsecond;
    int yfirst, ysecond;

    xfirst = 0, xsecond = 0;
    yfirst = 0, ysecond = 0;
    getScreenSize(sizeX,sizeY);
    spacex = *sizeX / 16;
    spacey = *sizeY / 16;
    int i =0;
    while(i < 16){
        struct region reg;
        xfirst = (i*spacex);
        yfirst = (i*spacey);
        xsecond += spacex;
        ysecond += spacey;
        reg.xL = xfirst;
        reg.xR = xsecond;
        reg.yU = yfirst;
        reg.yD = ysecond;
        regions[i] = reg;
        i++;
    }
    free(sizeX);
    free(sizeY);
    return regions;
}

void getMouseCood(int* x, int* y){
    int num;
    FILE *fp;
    char* buffer = malloc(80);  

    fp = popen("xdotool getmouselocation | cut -d : -f2 | cut -d y -f1","r"); //get X
    if(fp){
        while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) { }
        if(sscanf(buffer,"%d",&num)!=1)
            printf("ERROR\n");
        *x = num;
        pclose(fp);
    }
    fp = popen("xdotool getmouselocation | cut -d y -f2 |cut -d : -f2 | cut -b 1-4","r"); //get X
    if(fp){
        while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) { }
        if(sscanf(buffer,"%d",&num)!=1)
            printf("ERROR\n");
        *y = num;
        pclose(fp);
    }

    free(buffer);
}
int inRegY(int y, struct region r){
    if(y >= r.yU && y < r.yD){
        return 1;
    }
    return  0;
}

int inRegX(int x, struct region r){
    if(x >= r.xL && x < r.xR){
        return 1;
    }
    return  0;
}

char* getled(int x, int y,struct region* regions){
	int i,j;
	char* str = malloc(9);
	for(i = 0; i < 16;i++){
		if(inRegX(x,regions[i]) > 0)
			break;
	}for(j=0; j < 16; j++){
		if(inRegY(y,regions[j])>0)
			break;
	}
	int led = i % 8;
	int stick = i/8;
	if(stick == 1){
		led = 8-led;
	}
	sprintf(str,"%d%d",stick,led);
	strcat(str,":");
	strcat(str,ledsPorRegion[j]);
	strcat(str,"\n");
	return str;
}


void mouseMovement(){
    int x,y;
    int use;
    struct region* regions = malloc(sizeof(struct region)*16);
    char *l = malloc(40);

    use = -1;
    regions = createRegions();
    while(!status_exit){
        strcpy(l," ");
        getMouseCood(&x,&y);
        char* str = getled(x,y,regions);

        use = *str-'0';
       	++str;
       	if(use == 0){
       		#ifdef NEW_STICK
       		write(blinkstick1,"\n",1);
       		#endif
        	write(blinkstick0,str,11);
       	}
        else{
        	write(blinkstick0,"\n",1);
        	#ifdef
        	write(blinkstick1,str,11);
        	#endif
        }
   }

}

int moverHaciaLaDerecha(){
	int vuelta, led;
	
	for (vuelta = 0; vuelta < 10; ++vuelta){
		for (led = 0; led < 8; ++led){
			char* tmp = malloc(11*sizeof(char));
			*tmp = led + '0';
			strcat(tmp, ":0x110000");
			if(write(blinkstick0, tmp, 11) < 0)
				return -1;
			#ifdef NEW_STICK
				if(write(blinkstick1, tmp, 11) < 0)
					return -1;
			#endif
			usleep(100000);
			free(tmp);
		}
	}
	
	apagarLeds();

	return 0;
}

int moverHaciaLaIzquierda(){
	int vuelta, led;
	
	for (vuelta = 0; vuelta < 10; ++vuelta){
		for (led = 7; led >= 0; --led){
			char* tmp = malloc(10*sizeof(char));
			*tmp = led + '0';
			strcat(tmp, ":0x001100");
			if(write(blinkstick0, tmp, 10) < 0)
				return -1;
			#ifdef NEW_STICK
				if(write(blinkstick1, tmp, 10) < 0)
					return -1;
			#endif
			usleep(100000);
			free(tmp);
		}
	}
	apagarLeds();

	return 0;
}

int arcoiris(){
	char decision;
	int indice = 0, ret;

	printf("\n\t¡¡¡ATENCION!!! Esta opcion podria deslumbrar por contener colores de fuerte intensidad.\n");
	printf("\t¿Continuar? [y/n]: ");
	scanf("%c", &decision);

	if(decision == 'y'){
		ARCOIRIS(indice){
			ret = escribirArcoiris();
		}

		apagarLeds();
	}

	return (ret < 0) ? -1 : 0;
}
int escribirArcoiris(){	
	if(write(blinkstick0, "0:0x110000", 10) < 0) return -1;
	if(write(blinkstick0, "1:0xC38748", 10) < 0) return -1;
	if(write(blinkstick0, "2:0xC3B848", 10) < 0) return -1;
	if(write(blinkstick0, "3:0x001100", 10) < 0) return -1;
	if(write(blinkstick0, "4:0x1C9B7E", 10) < 0) return -1;
	if(write(blinkstick0, "5:0x000011", 10) < 0) return -1;
	if(write(blinkstick0, "6:0x291B66", 10) < 0) return -1;
	if(write(blinkstick0, "7:0x5F2471", 10) < 0) return -1;

	#ifdef NEW_STICK
		if(write(blinkstick1, "0:0x110000", 10) < 0) return -1;
		if(write(blinkstick1, "1:0xC38748", 10) < 0) return -1;
		if(write(blinkstick1, "2:0xC3B848", 10) < 0) return -1;
		if(write(blinkstick1, "3:0x001100", 10) < 0) return -1;
		if(write(blinkstick1, "4:0x1C9B7E", 10) < 0) return -1;
		if(write(blinkstick1, "5:0x000011", 10) < 0) return -1;
		if(write(blinkstick1, "6:0x291B66", 10) < 0) return -1;
		if(write(blinkstick1, "7:0x5F2471", 10) < 0) return -1;
	#endif

	return 0;
}

int apagarLeds(){
	if(write(blinkstick0, APAGAR_TODO, 80) < 0)
		return -1;

	#ifdef NEW_STICK
		if(write(blinkstick1, APAGAR_TODO, 80) < 0)
			return -1;
	#endif

	return 0;
}

/* FUNCIONES PARA CONTROLAR LOS LEDS SEGUN AJUSTEMOS EL VOLUMEN DEL SISTEMA */
void stop(){
	status_exit = 0;
    printf("Press 'Enter' to exit the program: ... ");
    while ( getchar() != '\n');
    status_exit = 1;
}

int getVolume(){

    int num;
    FILE *fp;
    char* buffer = malloc(80);
    char aux[80];

    fp = popen("pactl list sinks | grep '^[[:space:]]Volume:' | head -n $(( $SINK + 1 )) |  cut -d / -f2 | cut -d % -f1","r");
    if(fp){
        while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) { }
        while(*buffer == ' ') {buffer++;}
        strncpy(aux,buffer,strlen(buffer));
        strcpy(aux,buffer);
        if(sscanf(aux,"%d",&num)!=1)
            printf("ERROR\n");
        //printf("%d\n",num);
        pclose(fp);
    }

     return num;
}

void volume(){
    int n;
    int use=-1;
    //int maxUse=0;
    char *l = malloc(300);

    while(!status_exit){
        strcpy(l," ");
        
        n = getVolume();

        if(n<2){
            if(use != 0){
                write(blinkstick0,"\n",1);
                #ifdef NEW_STICK
                write(blinkstick1,"\n",1);
                #endif
                use = 0;
            }
        }else if(n > 2 && n < 22){
            if(use != 1){
                strcat(l,led1); 
                strcat(l,"\n");
                write(blinkstick0,l,11);
                #ifdef NEW_STICK
                write(blinkstick1,l,11);
                #endif
                use=1;
            }
        }else if(n>=22 && n < 40){
            if(use != 2){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,"\n");
                write(blinkstick0,l,22);
                #ifdef NEW_STICK
                write(blinkstick1,l,22);
                #endif
                use = 2;
            }
        }else if(n>=40 && n<55){
            if(use != 3){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,",");
                strcat(l,led3);
                strcat(l,"\n");
                write(blinkstick0,l,33);
                #ifdef NEW_STICK
                write(blinkstick1,l,33);
                #endif
                use = 3;
            }
        }else if(n>=55 && n<70){
            if(use != 4){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,",");
                strcat(l,led3);
                strcat(l,",");
                strcat(l,led4);
                strcat(l,"\n");
                write(blinkstick0,l,44);
                #ifdef NEW_STICK
                write(blinkstick1,l,44);
                #endif
                use = 4;
            }
        }else if(n>=70 && n<85){
            if(use != 5){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,",");
                strcat(l,led3);
                strcat(l,",");
                strcat(l,led4);
                strcat(l,",");
                strcat(l,led5);
                strcat(l,"\n");
                write(blinkstick0,l,55);
                #ifdef NEW_STICK
                write(blinkstick1,l,55);
                #endif
                use = 5;
            }
        }else if(n>=85 && n<100){
            if(use != 6){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,",");
                strcat(l,led3);
                strcat(l,",");
                strcat(l,led4);
                strcat(l,",");
                strcat(l,led5);
                strcat(l,",");
                strcat(l,led6);
                strcat(l,"\n");
                write(blinkstick0,l,66);
                #ifdef NEW_STICK
                write(blinkstick1,l,66);
                #endif
                use = 6;
            }
        }else if(n>=100 && n<115){
            if(use != 7){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,",");
                strcat(l,led3);
                strcat(l,",");
                strcat(l,led4);
                strcat(l,",");
                strcat(l,led5);
                strcat(l,",");
                strcat(l,led6);
                strcat(l,",");
                strcat(l,led7);
                strcat(l,"\n");
                write(blinkstick0,l,77);
                #ifdef NEW_STICK
                write(blinkstick1,l,77);
                #endif
                use = 7;
            }
        }else if(n>=115){
            if(use != 8){
                strcat(l,led1);
                strcat(l,",");
                strcat(l,led2);
                strcat(l,",");
                strcat(l,led3);
                strcat(l,",");
                strcat(l,led4);
                strcat(l,",");
                strcat(l,led5);
                strcat(l,",");
                strcat(l,led6);
                strcat(l,",");
                strcat(l,led7);
                strcat(l,",");
                strcat(l,led8);
                strcat(l,"\n");
                write(blinkstick0,l,88);
                #ifdef NEW_STICK
                write(blinkstick1,l,88);
                #endif
                use = 8;
            }
        }else{
            write(blinkstick0,"\n",1);
            #ifdef NEW_STICK
            write(blinkstick1,"\n",1);
            #endif
        }
    }
    free(l);
}
