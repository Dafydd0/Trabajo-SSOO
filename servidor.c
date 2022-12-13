#include <stdio.h> 
#include <stdlib.h> 
#include <ctype.h> 
#include <sys/msg.h> 
#include <time.h> 
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdbool.h>

#define TAM_BUFFER_MC 20
#define NUM_SEMAFOROS 2
#define NUM_PREDEFINIDOS 4
#define MAX_TAM_NOMBRE 20

#define MAX_DISP 20
#define MAX_USUARIOS 5
#define MAX_TOTAL MAX_DISP*MAX_USUARIOS


#define ANADIR 1
#define CAMBIAESTADO 2
#define ELIMINAR 3
#define SALIR 4

#define NO_ASIGNADO 10

#define COSTE_KWH 0.21945

/**
 * Programa display.
 * Su función es servir de central de control para los diversos clientes,
 * debe crear la cola de mensajes y la zona de memoria compartida a utilizar,
 * así como imprimir por pantalla una tabla que se vaya actualizando
 * automáticamente según se vayan conectando o desconectando dispositivos.
 */



/**
 * Estructura dispositivo.
 * Contiene el nombre y el consumo de cada dispositivo que se registre.
 */
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

/**
 * Estructura msgbuf
 * Contiene el tipo de mensajes que se van a enviar a la cola de mensajes.
 */
struct msgbuf{
  long mtype;
  disp dispo;
};

time_t tiempo;
struct tm *fecha;

void imp_Tabla(disp tabla[MAX_TOTAL],disp*MC);
void iniciaRecursos();
void eliminaRecursos();
int escr_msg(int qid,struct msgbuf *qbuf);
int leer_msg(int qid,long type,struct msgbuf *qbuf);

int main(){  
  eliminaRecursos();
  iniciaRecursos();
 
  key_t clave; 
  disp *seg = NULL;
  int shmid;
  sem_t *semMC;
  
  key_t claveCola; 
  int msgqueue_id; 
  struct msgbuf qbuffer; 
   
  clave=ftok(".",'M'); 
  if ((semMC = sem_open("MC",0))==SEM_FAILED)
    printf("Error al abrir el semaforo\n");
  else
    {
      if((shmid = shmget(clave,(TAM_BUFFER_MC)*sizeof(disp),IPC_CREAT|0660))==-1) 
	{ 
	  printf("No se pudo obener la id del segmento de memoria compartida\n"); 
	} 
      else{ 
	if((seg=shmat(shmid,NULL,0))== (disp *)-1) 
	  printf("Error al mapear el segmento\n"); 
	else{
	  claveCola = ftok(".",'q');
	  if ((msgqueue_id=msgget(claveCola,IPC_CREAT|0660))==-1)
	    printf("Error al iniciar la cola\n");
	  else{
	      //Elementos predefinidos
	      
	      disp predef[NUM_PREDEFINIDOS];
	      char *nombreDispPredef[NUM_PREDEFINIDOS]={"impresora","PC","calefacción","aire"};
	      float consumoDispPredef[NUM_PREDEFINIDOS]={0.09,0.28,5.6,4.8};

	      for(int i=0;i<NUM_PREDEFINIDOS;i++)
		{
		  strcpy(predef[i].nombre,nombreDispPredef[i]);
		  predef[i].consumo=consumoDispPredef[i];
		  predef[i].opciones=NO_ASIGNADO;
		  predef[i].ON=true;
		/*  //Fecha
		  tiempo = time(NULL);
  		  fecha = localtime(&tiempo);	
  */
		  predef[i].year = 0000;
		  predef[i].month = 00;
		  predef[i].day = 00;
		  predef[i].hour = 00;
		  predef[i].min = 00;
		}
	      //Dispositivo vacío
	      disp vacio;
	      vacio.consumo=-1;
	      vacio.opciones=NO_ASIGNADO;
	      vacio.ON=false;
	      vacio.year=0000;
	      vacio.month=00;
	      vacio.day=00;
	      vacio.hour=00;
	      vacio.min=00;
	      strcpy(vacio.nombre,"void");

	      //Añadimos los dispositivos predifinidos para inicializar la memoria compartida
	      sem_wait(semMC);
	      for (int i=0; i<NUM_PREDEFINIDOS; i++){
		seg[i] = predef[i];
	      }
	      for (int i=NUM_PREDEFINIDOS; i<TAM_BUFFER_MC; i++)
		seg[i]=vacio;
	      sem_post(semMC);

	      //Creamos una tabla local que almacene los dispositivos de la empresa que queramos controlar y la inicailizamos vacía
	      disp tablaDispos[MAX_TOTAL];
	      for (int i=0;i<MAX_TOTAL;i++){
		tablaDispos[i] = vacio;
	      }
	      printf("\n");
	      printf("Los dispositivos predefinidos son:\n");
	      imp_Tabla(tablaDispos,seg);
	      printf("\n\n");
	      printf("Esperando modificaciones...\n");
		  
	      //El display entrará en un bucle que no finalizará hasta que reciba esa instrucción de un cliente.  
	      bool exit = false;
	      while(exit==false)
		{
		  leer_msg(msgqueue_id,0,&qbuffer);
		  switch(qbuffer.dispo.opciones)
		    {

		      // Para añadir un nuevo dispositivo, lo escribimos en la tabla local y pedimos que nos la imprima actualizada
		    case ANADIR:{
		      int hueco = -1;
		      for (int i=0;i<MAX_TOTAL && hueco == -1; i++){
			if (tablaDispos[i].consumo == -1)
			  hueco = i;
			
		      }
		      if (hueco == -1)
			printf("ERROR: la tabla está completa. Borre algún elemento para añadir otro\n");
		      else{
		      		      
			tablaDispos[hueco] = qbuffer.dispo;
			tablaDispos[hueco].opciones = NO_ASIGNADO;
			
			for (int i=0;i<MAX_TOTAL; i++){
		      		if(strcmp(seg[i].nombre,qbuffer.dispo.nombre)==0){ //Actualizamos la MC
		        	    seg[i] = qbuffer.dispo;
		        	    seg[i].opciones = NO_ASIGNADO;
		        	}
		        }
			
			imp_Tabla(tablaDispos,seg);
		      }
		      break;
		    }
		      // Para eliminar un dispositivo de la lista, identifica su nombre y busca si hay algún dispositivo de ese tipo almacenado, y si es así, sustituye su espacio en la cola por un dispositivo del tipo vacio
		      
		    case ELIMINAR:{
		      bool found = false;
		      for (int i=0;i<MAX_TOTAL&&found==false;i++)
			{
			  if(strcmp(tablaDispos[i].nombre,qbuffer.dispo.nombre)==0&&tablaDispos[i].ON==qbuffer.dispo.ON)
			    {
			      found=true;
			      tablaDispos[i]=vacio;
			    }
			}
			
			for (int i=0;i<MAX_TOTAL;i++){

				if(strcmp(seg[i].nombre, qbuffer.dispo.nombre) == 0 ){
					seg[i].year = qbuffer.dispo.year;
			      		seg[i].month = qbuffer.dispo.month;
				        seg[i].day = qbuffer.dispo.day;
				        seg[i].hour = qbuffer.dispo.hour;
				        seg[i].min = qbuffer.dispo.min;
			   	      }
			}
		      imp_Tabla(tablaDispos,seg);
		      found=false;
		      break;
		    }
		    case CAMBIAESTADO:{
		    
		      bool found = false;	
		      for (int i=0;i<MAX_TOTAL&&found==false;i++)
			{
			  if(strcmp(tablaDispos[i].nombre,qbuffer.dispo.nombre)==0)
			    {
			      found=true;

			      tablaDispos[i].ON=qbuffer.dispo.ON;
			      tablaDispos[i].year = qbuffer.dispo.year;
			      tablaDispos[i].month = qbuffer.dispo.month;
			      tablaDispos[i].day = qbuffer.dispo.day;
			      tablaDispos[i].hour = qbuffer.dispo.hour;
			      tablaDispos[i].min = qbuffer.dispo.min;
			}
		      }
		      for (int i=0;i<MAX_TOTAL;i++){

			if(strcmp(seg[i].nombre, qbuffer.dispo.nombre) == 0){

			      seg[i] = qbuffer.dispo;
			     /* seg[i].year = qbuffer.dispo.year;
			      seg[i].month = qbuffer.dispo.month;
			      seg[i].day = qbuffer.dispo.day;
			      seg[i].hour = qbuffer.dispo.hour;
			      seg[i].min = qbuffer.dispo.min;*/
			
			     }
			}
			
		      imp_Tabla(tablaDispos,seg);
		      found=false;
		      break;
		    }
		      //Para cerrar el programa, se elimina el contenido de la MC y de la cola, posteriormente, se sale del bucle y se libera la memoria reservada.
		      
		    case SALIR:
		      exit=true;
		      printf("Cerrando servidor\n");
		      break;

		    default:
		      printf("Algo raro ha sucedido...\n%s.%s=%d",qbuffer.dispo.nombre,"opciones",qbuffer.dispo.opciones);
		      break;
		    }
	      }
	    }
	  shmdt(seg);
	}
      }
      sem_close(semMC);
    }
  eliminaRecursos();
  return(0);
}


  
/**
 * Función imp_tabla
 * Imprime por pantalla una tabla con todos los dispositivos de cada tipo y el consumo de cada uno de ellos, así como el consumo total.
 */
void imp_Tabla(disp tabla[MAX_TOTAL],disp *MC)
{		
  //abro el semáforo MC
  sem_t *sem_MC=NULL;
  if((sem_MC=sem_open("MC", 0600))==NULL)
    printf("ERROR: no se ha podido abrir el semaforo en imp_Tabla\n");
  else{
    //Calculamos cuantos tipos de dispositivos tenemos en la memoria compartida
    sem_wait(sem_MC);
    int elementos =0;
    for (int i = 0; i<TAM_BUFFER_MC; i++){
      if (MC[i].consumo != -1)
	elementos++;
    }
    sem_post(sem_MC);
  
    //inicializamos el contador a 0, se ocupará de decir cuántos elementos de cada tipo hay.
    int contador[elementos];
    for(int i=0;i<elementos;i++)
      {
	contador[i]=0;
      }
  printf("+---------------+-----------------------+-----------------------+-----------------------+-----------------------+\n");
      printf("|    NOMBRE\t|    Nº DISPOSITIVOS\t| CONSUMO UNITARIO(kWh)\t|   CONSUMO TOTAL(kWh)\t|    FECHA ENCENDIDO\t|\n");

  printf("+---------------+-----------------------+-----------------------+-----------------------+-----------------------+\n");
    
    //Repetiremos para cada elemento de la tabla un algoritmo que busca en memoria compartida al elemento cuyo nombre coincida.
    for (int i=0;i<MAX_TOTAL;i++)
      {	
	for (int j=0;j<elementos;j++)
	  {
	    if (sem_wait(sem_MC)!=0)
	      printf("ERROR: el semáforo no ha podido ser bajado\n");
	    if(strcmp(tabla[i].nombre,MC[j].nombre)==0&&tabla[i].ON==1)
	      contador[j]++;
	      //Si añado aquí lo de poner a 0 la fecha, no es una solución definitiv aya que si quisiera encender un dispositivo seguiría teniendo el problema de que no se actualiza
	    if (sem_post(sem_MC)!=0)
	      printf("ERROR: el semáforo no ha podido ser subido\n");
	  }
      }
    
    
    //imprimimos los datos obtenidos de la tabla.
    float consumoTotal=0;
    for (int i=0;i<elementos;i++)
      {
	consumoTotal += contador[i]*MC[i].consumo;
	if (sem_wait(sem_MC)!=0)
	  printf("ERROR: el semáforo no ha podido ser subido\n");
	printf("|%13.13s\t|%13d\t\t|%9.2f\t\t|%9.2f\t\t|%02d/%02d/%04d %02d:%02d\t|\n",MC[i].nombre,contador[i],MC[i].consumo,(contador[i]*MC[i].consumo),MC[i].day, MC[i].month, MC[i].year, MC[i].hour, MC[i].min);
	if (sem_post(sem_MC)!=0)
	  printf("ERROR: el semáforo no ha podido ser bajado\n");
      }
    printf("+---------------+-----------------------+-----------------------+-----------------------+-----------------------+\n");
    printf("Consumo total: %10.2f KWh \t|\tCoste KWh: %10.5f €/KWh \t|\tPrecio total: %10.2f €/h \t|\n\n\n\n\n",consumoTotal, COSTE_KWH, COSTE_KWH*consumoTotal);
  } 
}

void iniciaRecursos(){
  //Creamos el semaforo
  if (sem_open("MC",O_CREAT,0600,1) != SEM_FAILED){  }
  else{
    printf("Error en la creación del semáforo MC\n");
  }
    //Creamos el semaforo
  if (sem_open("cola",O_CREAT,0600,1) != SEM_FAILED){
  }
  else{
    printf("Error en la creación del semáforo cola\n");
  }
     
      
  key_t clave; 
  int shmid;
  //Creamos la memoria compartida
  clave=ftok(".",'M'); 

  if((shmid = shmget(clave,(TAM_BUFFER_MC)*sizeof(disp),IPC_CREAT|IPC_EXCL|0660))==-1) 
    { 
      printf("El segmento de memoria compartida ya existe\n"); 
    } 
  else{ 
    //printf("Nuevo segmento creado\n");    
  }

  //Memoria compartida y semaforo para la asignacion de usuarios
  
    //Creamos el semaforo
  if (sem_open("usuarios",O_CREAT,0600,1) != SEM_FAILED){
  }
  else{
    printf("Error en la creación del semáforo usuarios\n");
  }
  key_t claveUsu; 
  int shmidUsu;
  //Creamos la memoria compartida
  claveUsu=ftok(".",'U'); 

  shmidUsu = shmget(claveUsu,(MAX_USUARIOS)*sizeof(int),IPC_CREAT|IPC_EXCL|0660); 
  
  int *segUsu = shmat(shmidUsu,NULL,0);
  for (int i=0; i<MAX_USUARIOS; i++)
    {
      segUsu[i]= 0;
    }
  shmdt(segUsu);    
}

void eliminaRecursos(){
  //Cerramos el semaforo
  if (sem_unlink("cola") == 0){
  }
  else{
  }

    //Cerramos el semaforo
  if (sem_unlink("MC") == 0){
  }
  else{
  }
     
  key_t clave; 
  int shmid;

  //Cerramos la memoria compartida
  clave=ftok(".",'M'); 

  if((shmid = shmget(clave,(TAM_BUFFER_MC)*sizeof(disp),IPC_CREAT|0660))==-1) 
    { 
      printf("No se ha podido obtener el id del segmento de memoria\n"); 
    } 
  else{ 
    shmctl(shmid,IPC_RMID,NULL);   
  }

  //Cerramos la cola
  int msgqueue_id;
  msgqueue_id=msgget(clave,IPC_CREAT|0660);
  msgctl(msgqueue_id,IPC_RMID,NULL);

  
  //Memoria compartida y semaforo para la asignacion de usuarios
  
  //Eliminamos el semaforo
  if (sem_unlink("usuarios")== 0){
  }
  else{
  }
  key_t claveUsu; 
  int shmidUsu;
  //Eliminamos la memoria compartida
  claveUsu=ftok(".",'U'); 

  if((shmidUsu = shmget(claveUsu,(MAX_USUARIOS)*sizeof(int),IPC_CREAT|0660))==-1) 
    { 
      printf("El segmento de memoria usuarios ya existe\n"); 
    } 
  else{
    shmctl(shmidUsu,IPC_RMID,NULL);
    
  }
}


/**
 * Función escr_msg
 * Sirve para escribir un mensaje en la cola, en este caso, para añadir un dispositivo a la lista
 */
int escr_msg(int qid,struct msgbuf *qbuf){
  int resultado;
  resultado=msgsnd(qid,qbuf,sizeof(disp),0);
  return resultado;
}


/**
 * Función leer_msg
 * Sirve para leer un mensaje de la cola, en este caso, para obtenerlo de la lista
 */
int leer_msg(int qid,long type,struct msgbuf *qbuf){
  int resultado;
  resultado=msgrcv(qid,qbuf,sizeof(disp),type,0);
  return resultado;
}
