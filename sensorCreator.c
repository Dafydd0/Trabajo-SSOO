#include <stdio.h> 
#include <stdlib.h> 
#include <ctype.h> 
#include <sys/msg.h>
#include <time.h> 
#include <string.h>
#include <semaphore.h>
#include <sys/shm.h> 
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <wait.h>
#include <stdbool.h>

#define MAX_DISP 10
#define MAX_GEST 5
#define MAX_TOTAL MAX_DISP*MAX_GEST

#define MAX_TAM_NOMBRE 20
#define TAM_BUFFER_MC 20


#define ANADIR 1
#define CAMBIAESTADO 2
#define ELIMINAR 3
#define SALIR 4

#define NO_ASIGNADO 10


typedef struct dispositivo{
  char nombre[MAX_TAM_NOMBRE];
  float consumo;
  int opciones;
  bool ON;
  //struct tm *fecha_encendido;
  //Fecha encendido
  int year;
  int month;
  int day;
  int hour;
  int min;
}disp;

pid_t pid;
time_t tiempo;
struct tm *fecha;

int indice_to_delete = -1;

int interfaz_ini(char id);
void creaDispo(sem_t*mutex,sem_t*cambios,disp *seg,sem_t*MC,disp*segServ);
void borraDispo(sem_t*mutex,sem_t*cambios,disp *seg);
void borraTodo(sem_t*mutex,sem_t*cambios,disp *seg);
void listaDispo(disp *seg);
void cambiaEstadoDispo(sem_t*mutex,sem_t*cambios,disp *seg);
char seleccionaUsuario();
int iniciaRecursos(char id,sem_t**cambios,sem_t**mutex,disp**seg);
void cierraRecursos(sem_t**cambios,sem_t**mutex,disp**seg);

int main (int argc, char * argv[]){
  tiempo = time(NULL);
  char id = seleccionaUsuario();
  
  sem_t *cambios = NULL;
  sem_t *mutex = NULL;
  disp *seg = NULL;
  if (iniciaRecursos(id,&cambios,&mutex,&seg) == 0){
    printf("ERROR: se produjo un error al inicializar los semaforos y la memoria compartida\n");
  }
  else{
    key_t clave; 
    int shmid;

    sem_t *MC = NULL;
    key_t claveServ;
    disp *segServ = NULL;
    int shmidServ;
  
    claveServ = ftok(".",'M');
    if ((MC = sem_open("MC",0))==SEM_FAILED)
      printf("Error al abrir el semaforo\n");
    else{
      if((shmidServ = shmget(claveServ,(MAX_DISP)*sizeof(disp),IPC_CREAT|0660))==-1) 
	printf("No se pudo obener la id del segmento de memoria compartida\n"); 
      else{
	if((segServ=shmat(shmidServ,NULL,0))== (disp *)-1) 
	  printf("Error al mapear el segmento\n");  
	else {
	  int select;
	  int end = 1;
	  while (end){
	    select = interfaz_ini(id);
	    switch(select){
	    case(0): //SALIR
	      {
		end = 0;
		printf("Cerrando terminal\n\n");
		break;
	      }
	    case(1): //AÑADIR DISPOSITIVO
	      {
		creaDispo(mutex,cambios,seg,MC,segServ);
		break;
	      }
	    case(2): //CAMBIAR ESTADO SENSOR
	      {
		cambiaEstadoDispo(mutex,cambios,seg);
		break;
	      }
	    case(3): //BORRAR DISPOSITIVO
	      {
		borraDispo(mutex,cambios,seg);
		break;
	      }
	    case(4): //MOSTRAR TODOS LOS DISPOTIVOS
	      {
		listaDispo(seg);
		break;
	      }
	    case(5): //BORRAR TODOS LOS DISPOSITIVOS
	      {
		borraTodo(mutex,cambios,seg);
		break;
	      }
	    case(6): //CAMBIAR DE GESTOR
	      {
      		id = seleccionaUsuario();
		cierraRecursos(&cambios,&mutex,&seg);
		if (iniciaRecursos(id,&cambios,&mutex,&seg)==0){
		  printf("Se ha producido un error al cambiar de gestor, cerrando\n");
		  end = 1;
		}

		break;
	      }
	    }
	  }
	  shmdt(segServ);
	}
      }
      sem_close(MC);
    }
    shmdt(seg);	   
    sem_close(mutex);
    sem_close(cambios);
  }
  return(0);
}

/*

Función que muestra la interfaz del usuario

*/
int interfaz_ini(char id){
   int select = 0;
   if (id == '-')
	select = 0;
   else{

   	printf("\nTERMINAL DEL USUARIO: %c\n",id);
  	printf("Seleccione una opción:\n");

	printf("0->Salir\n");
   	printf("1->Registrar nuevo sensor\n");
   	printf("2->Cambiar estado de un sensor\n");
   	printf("3->Eliminar un sensor\n");
   	printf("4->Listar todos mis sensores\n");
   	printf("5->Borrar todos los sensores del gestor\n");
   	printf("6->Cambiar de gestor\n\n");
   	printf("Opción: ");
   	scanf("%d",&select);
   	printf("\n");
   	while ((select>6) || (select<0)){
     	printf("Por favor, introduzca una opción adecuada: ");
     	scanf("%d",&select);
     	printf("\n");
   	}
   }
   return (select);
}

/*

Función que crea un dispositivo nuevo o incrementa en 1 el número de dispositivos si ya existía uno con el mismo nombre

*/
void creaDispo(sem_t*mutex,sem_t*cambios,disp *seg,sem_t*MC,disp*segServ){
  char tipo[1000];
  float consumo;
  printf("Introduzca nombre: ");
  scanf("%999s",tipo);

  //printf("Antes del wait\n");
  sem_wait(MC);
  //printf("Despues del wait\n");
  int repetido = 0;
  int h = -1;//Índice del 
  for (int i = 0; i<TAM_BUFFER_MC && repetido == 0; i++){
    if (segServ[i].consumo==-1 && h == -1){
      h = i;
    }
    if (strcmp(tipo,segServ[i].nombre)==0){
      repetido = 1;
      h = i;
    }
  }
  sem_post(MC);
  
  if (repetido == 1){
    sem_wait(MC);
    consumo = segServ[h].consumo;
    sem_post(MC);
  }
  else{
    do{
  	printf("El dispositivo es nuevo, introduzca el consumo: ");
    	scanf("%f",&consumo);
	if (consumo <= 0)
		printf("El consumo introducido no es válido.\n");
    }while (consumo <= 0);
    sem_wait(MC);
    strcpy(segServ[h].nombre,tipo);
    
    //Fecha
    tiempo = time(NULL);
    fecha = localtime(&tiempo);
  
    segServ[h].year = fecha->tm_year + 1900;
    segServ[h].month = fecha->tm_mon;
    segServ[h].day = fecha->tm_mday;
    segServ[h].hour = fecha->tm_hour;
    segServ[h].min = fecha->tm_min;
    
    segServ[h].consumo = consumo;
    sem_post(MC);
    printf("\nDatos actualizados en el servidor\n");
  }
  
  sem_wait(mutex);
  int hueco = -1;
  for (int i=0; i<MAX_DISP;i++){
    if (seg[i].consumo == -1){
      hueco = i;
      i = MAX_DISP;
    }
  }
  if (hueco == -1)
    printf("No queda hueco en el gestor\n");
  else{
    strcpy(seg[hueco].nombre,tipo);
    seg[hueco].consumo = consumo;
    seg[hueco].ON = true;
    seg[hueco].opciones = ANADIR;
    //Fecha
    tiempo = time(NULL);
    fecha = localtime(&tiempo);
  
    seg[hueco].year = fecha->tm_year + 1900;
    seg[hueco].month = fecha->tm_mon;
    seg[hueco].day = fecha->tm_mday;
    seg[hueco].hour = fecha->tm_hour;
    seg[hueco].min = fecha->tm_min;
    sem_post(cambios);
    printf("Sensor añadido con exito");
  }
  sem_post(mutex);
  printf("\n");
}

/*

Función que cambia el estado de un dispotivo ENCENDIDO/APAGADO

*/
void cambiaEstadoDispo(sem_t*mutex,sem_t*cambios,disp *seg){
  int hueco = -1;
  listaDispo(seg);
  printf("Introduzca el id del sensor a conmutar: ");
  scanf("%d",&hueco);
  
  if (hueco<0||hueco>=MAX_DISP||seg[hueco].consumo==-1)
    printf("No ha seleccionado un id válido\n");
  else{
    sem_wait(mutex);
    if (seg[hueco].ON == 1){//Si es 1, lo ponemos a 0
    
      seg[hueco].year=0000;
      seg[hueco].month=00;
      seg[hueco].day=00;
      seg[hueco].hour=00;
      seg[hueco].min=00;
      seg[hueco].ON = 0;
      seg[hueco].opciones = CAMBIAESTADO;

    }
    else{//Si es 0, lo ponemos a 1
    
      tiempo = time(NULL);
      fecha = localtime(&tiempo);
    
      seg[hueco].year = fecha->tm_year + 1900;
      seg[hueco].month = fecha->tm_mon;
      seg[hueco].day = fecha->tm_mday;
      seg[hueco].hour = fecha->tm_hour;
      seg[hueco].min = fecha->tm_min;
      seg[hueco].ON = 1;
      seg[hueco].opciones = CAMBIAESTADO;

    }
    sem_post(cambios);
    sem_post(mutex);
    printf("Sensor conmutado con exito");
  }
  printf("\n");
}

/*

Función que borra un dispositivo del usuario en cuestión

*/
void borraDispo(sem_t*mutex,sem_t*cambios,disp *seg){

  int hueco = -1;
  listaDispo(seg);

  printf("Introduzca el id del sensor a borrar: ");
  scanf("%d",&hueco);
  
  if (hueco<0||hueco>=MAX_DISP||seg[hueco].consumo==-1)
    printf("No ha seleccionado un id válido\n");
  else{
    sem_wait(mutex);
    
    seg[hueco].consumo = -1;
    seg[hueco].opciones = ELIMINAR;
    seg[hueco].year=0000;
    seg[hueco].month=00;
    seg[hueco].day=00;
    seg[hueco].hour=00;
    seg[hueco].min=00;
    
    sem_post(cambios);
    sem_post(mutex);
    printf("Sensor borrado con exito");
  }
  printf("\n");
}

/*

Función que imprime todos los dispositivos asociados a un usuario

*/
void listaDispo(disp *seg){
  printf("%c--------+--------------+---------------+---------------+-----------------------+\n", 201);
  printf("|  ID\t |    NOMBRE\t|   ENCENDIDO\t|    CONSUMO\t|    FECHA ENCENDIDO\t|\n");
  printf("+--------+--------------+---------------+---------------+-----------------------+\n");
  for (int i = 0; i<MAX_DISP;i++){
    if (seg[i].consumo != -1)
      {
	printf("|  %d\t |%9.05s\t|%9.05s\t|%10.02f\t|    %02d/%02d/%04d %02d:%02d\t|\n",i , seg[i].nombre,seg[i].ON?"SÍ":"NO", seg[i].consumo,seg[i].day, seg[i].month, seg[i].year, seg[i].hour, seg[i].min);
			    
      }
  }
  printf("+--------+--------------+---------------+---------------+-----------------------+\n");
}

/*

Función que borra todos los dispositivos asociados a un usuario

*/
void borraTodo(sem_t*mutex,sem_t*cambios,disp *seg){

  char tecla[1000];
  
  printf("¿Esta seguro de que desea borrar todos los sensores? s/n: ");
  scanf("%999s",tecla);
  
  if (tecla[0] =='s'){
    sem_wait(mutex);
    for (int i = 0; i<MAX_DISP; i++){
      seg[i].consumo = -1;
      seg[i].opciones = ELIMINAR;
      seg[i].year=0000;
      seg[i].month=00;
      seg[i].day=00;
      seg[i].hour=00;
      seg[i].min=00;
    }
    sem_post(cambios);
    sem_post(mutex);
    printf("Sensores borrados con exito\n");
  }
  else{
    printf("Operación cancelada\n");
  }
  printf("\n");
}

/*

Función que devuelve el usuario seleccionado

*/
char seleccionaUsuario(){
  sem_t*gestores;
  key_t clave; 
  int *seg = NULL;
  int shmid;
  int cont = 0;

  char result[1000];

  char validos[MAX_GEST];
  
  if ((gestores = sem_open("gestores",0)) == SEM_FAILED){
    printf("Error al abrir el semaforo\n");
  }
  else{   
    clave=ftok(".",'G'); 

    if((shmid = shmget(clave,(MAX_GEST)*sizeof(int),IPC_CREAT|0660))==-1) 
      { 
	printf("No se pudo obener la id del segmento de memoria compartida\n"); 
      } 
    else{ 
      if((seg=shmat(shmid,NULL,0))== (int *)-1) 
	printf("Error al mapear el segmento\n"); 
      else{
	sem_wait(gestores);
	for (int i=0; i<MAX_GEST; i++){
	   if(seg[i] == 1)
	     cont++;
	}
	if (cont!=0){
	   printf("Los gestores disponibles son: \n");
	   for (int i=0; i<MAX_GEST; i++){
	     if (seg[i] == 1){
	       validos[i] = i+48;
	       printf("\t- GESTOR %c\n",i+48);
	       seg[i]=1;
	     }
	   }
	}
	else{
	   printf("No hay gestores disponibles\n");
	   result[0] = '-';
	}
	sem_post(gestores);
	shmdt(seg);
      }
      sem_close(gestores);
    }
  }
  //Comprueba si el gestor elegido existe
  int apto = 0;
  while ((apto == 0) && (cont != 0)){
    printf("Indique el gestor sobre el que desea operar: ");
    scanf("%999s",result);
    for (int i=0; i<MAX_GEST; i++){
      if (validos[i]==result[0])
	apto =1;
    }
    if (apto == 0)
      printf("Eleccion no valida\n");
  }
  return result[0];
}
	
/*

Función que inicia los recursos de la memoria compartida

*/		
int iniciaRecursos(char id,sem_t**cambios,sem_t**mutex,disp**seg){
  key_t clave; 
  int shmid;
  int result = 1;
  char claveMutex[] = {'m','u','t','e','x',id,'\0'};  
  char claveCambios[] = {'c','a','m','b','i','o',id,'\0'};
  char claveMemoria = id;

  if ((*cambios = sem_open(claveCambios,0)) == SEM_FAILED){
    result = 0;
  }
  else {
    if ((*mutex = sem_open(claveMutex,0)) == SEM_FAILED){
      result = 0;
    }
    else{   
      clave=ftok(".",claveMemoria); 

      if((shmid = shmget(clave,(MAX_DISP)*sizeof(disp),IPC_CREAT|0660))==-1) 
	{ 
	  result = 0;
	} 
      else{ 
	if((*seg=shmat(shmid,NULL,0))== (disp *)-1) 
	  result = 0;
      }
    }
  }
  return (result);
}					    
/*

Función que cierra los recursos

*/
void cierraRecursos(sem_t**cambios,sem_t**mutex,disp**seg){
  shmdt(*seg);	   
  sem_close(*cambios);
  sem_close(*mutex);
}
