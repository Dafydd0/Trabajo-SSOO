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
#define NUM_PREDEFINIDOS 5
#define MAX_TAM_NOMBRE 20

#define MAX_DISP 20
#define MAX_USUARIOS 5
#define MAX_TOTAL MAX_DISP *MAX_USUARIOS

#define ANADIR 1
#define CAMBIAESTADO 2
#define ELIMINAR 3
#define SALIR 4

#define NO_ASIGNADO 10

#define COSTE_KWH 0.21945

/*

La función de esta base de datos es la de crear y gestionar una cola de mensajes
y una zona de memoria compartida para los diversos usuarios.
Además, se encarga de mostrar en una tabla actualizada en tiempo real la conexión
y desconexión de diversos dispositivos que se gestionan

*/

/*

Estructura que define los dispositivos

*/
typedef struct dispositivo
{
  char nombre[MAX_TAM_NOMBRE];
  float consumo;
  int opciones;
  bool ON;
  // Fecha encendido
  int year;
  int month;
  int day;
  int hour;
  int min;
} disp;

/*

Estructura msgbuf, esta contiene el tipo de mensajes que se van a enviar por cola de mensajes

*/
struct msgbuf
{
  long mtype;
  disp dispo;
};

time_t tiempo;
struct tm *fecha;

void imprimirTabla(disp tabla[MAX_TOTAL], disp *MC);
void iniciaRecursos();
void eliminaRecursos();
int escr_msg(int qid, struct msgbuf *qbuf);
int leer_msg(int qid, long type, struct msgbuf *qbuf);

int main()
{
  // Eliminamos la memoria compartida y todo lo demás para luego volverlo a abrir,
  // así evitamos problemas de que la memoria esté creada o que ya haya dispositivos en la memoria

  eliminaRecursos();
  iniciaRecursos();

  key_t clave;
  disp *seg = NULL;
  int shmid;
  sem_t *semMC;

  key_t claveCola;
  int msgqueue_id;
  struct msgbuf qbuffer;

  clave = ftok(".", 'M');
  if ((semMC = sem_open("MC", 0)) == SEM_FAILED)
    printf("Error al abrir el semaforo\n");
  else
  {
    if ((shmid = shmget(clave, (TAM_BUFFER_MC) * sizeof(disp), IPC_CREAT | 0660)) == -1)
      printf("Se ha producido un error al obtener el id del segmento de memoria compartida\n");

    else
    {
      if ((seg = shmat(shmid, NULL, 0)) == (disp *)-1)
        printf("Se ha producido un error al mapear el segmento\n");
      else
      {
        claveCola = ftok(".", 'q');
        if ((msgqueue_id = msgget(claveCola, IPC_CREAT | 0660)) == -1)
          printf("Se ha producido un error al iniciar la cola\n");
        else
        {
          // Creamos unos dispositivos ya predefinidos para que la base de datos no esté vacía
          disp predefinido[NUM_PREDEFINIDOS];
          char *nombreEd[NUM_PREDEFINIDOS] = {
              "Impresora",
              "Ordenador",
              "Router",
              "Switch",
              "Cámara",
          };
          float consumoEd[NUM_PREDEFINIDOS] = {0.8, 0.2, 0.02, 0.01, 0.015};

          for (int i = 0; i < NUM_PREDEFINIDOS; i++)
          {
            strcpy(predefinido[i].nombre, nombreEd[i]);
            predefinido[i].consumo = consumoEd[i];
            predefinido[i].opciones = NO_ASIGNADO;
            predefinido[i].ON = true;

            tiempo = time(NULL);
            fecha = localtime(&tiempo);

            predefinido[i].year = 0000;
            predefinido[i].month = 00;
            predefinido[i].day = 00;
            predefinido[i].hour = 00;
            predefinido[i].min = 00;
          }
          // Creamos un dispositivo vacío para cuando haya que borrar alguno
          disp empty;
          empty.consumo = -1;
          empty.opciones = NO_ASIGNADO;
          empty.ON = false;
          empty.year = 0000;
          empty.month = 00;
          empty.day = 00;
          empty.hour = 00;
          empty.min = 00;
          strcpy(empty.nombre, "empty");

          // Añadimos los dispositivos predifinidos para inicializar la memoria compartida
          sem_wait(semMC);
          for (int i = 0; i < NUM_PREDEFINIDOS; i++)
          {
            seg[i] = predefinido[i];
          }
          for (int i = NUM_PREDEFINIDOS; i < TAM_BUFFER_MC; i++)
            seg[i] = empty;
          sem_post(semMC);

          // tablaDispositivos es una tabla local para almacenar los dispositivos
          disp tablaDispositivos[MAX_TOTAL];
          for (int i = 0; i < MAX_TOTAL; i++)
          {
            tablaDispositivos[i] = empty;
          }

          printf("Los dispositivos predefinidos son:\n");
          imprimirTabla(tablaDispositivos, seg);

          // La base de datos estará en un bucle infinito hasta que un cliente le diga que se cierre
          bool exit = false;
          while (!exit)
          {
            leer_msg(msgqueue_id, 0, &qbuffer);
            switch (qbuffer.dispo.opciones)
            {

            case ANADIR: // Añadimos el dispositivo a la tabla local y lo imprimimos
            {
              int hueco = -1;
              for (int i = 0; i < MAX_TOTAL && hueco == -1; i++)
              {
                if (tablaDispositivos[i].consumo == -1)
                  hueco = i;
              }
              if (hueco == -1)
                printf("Se ha producido un error, no queda hueco libre en la tabla\n");
              else
              {

                tablaDispositivos[hueco] = qbuffer.dispo;
                tablaDispositivos[hueco].opciones = NO_ASIGNADO;

                for (int i = 0; i < MAX_TOTAL; i++)
                {
                  if (strcmp(seg[i].nombre, qbuffer.dispo.nombre) == 0)
                  { // Actualizamos la MC
                    seg[i] = qbuffer.dispo;
                    seg[i].opciones = NO_ASIGNADO;
                  }
                }

                imprimirTabla(tablaDispositivos, seg);
              }
              break;
            }
            case CAMBIAESTADO: // Cambiamos de ON a OFF o al revés
            {

              bool found = false;
              for (int i = 0; i < MAX_TOTAL && found == false; i++)
              {
                if (strcmp(tablaDispositivos[i].nombre, qbuffer.dispo.nombre) == 0)
                {
                  found = true;
                  // Cambiamos la fecha, 0 si se apaga y la fecha actual si se enciende
                  tablaDispositivos[i].ON = qbuffer.dispo.ON;
                  tablaDispositivos[i].year = qbuffer.dispo.year;
                  tablaDispositivos[i].month = qbuffer.dispo.month;
                  tablaDispositivos[i].day = qbuffer.dispo.day;
                  tablaDispositivos[i].hour = qbuffer.dispo.hour;
                  tablaDispositivos[i].min = qbuffer.dispo.min;
                }
              }
              for (int i = 0; i < MAX_TOTAL; i++)
              {

                if (strcmp(seg[i].nombre, qbuffer.dispo.nombre) == 0)
                {

                  seg[i] = qbuffer.dispo;
                }
              }

              imprimirTabla(tablaDispositivos, seg);
              found = false;
              break;
            }
            case ELIMINAR: // Si queremos eliminar un dispositivo lo igualamos al dispositivo 'empty' que creamos antes
            {
              bool found = false;
              for (int i = 0; i < MAX_TOTAL && found == false; i++)
              {
                if (strcmp(tablaDispositivos[i].nombre, qbuffer.dispo.nombre) == 0 && tablaDispositivos[i].ON == qbuffer.dispo.ON)
                {
                  found = true;
                  tablaDispositivos[i] = empty;
                }
              }

              for (int i = 0; i < MAX_TOTAL; i++)
              {

                if (strcmp(seg[i].nombre, qbuffer.dispo.nombre) == 0)
                {
                  seg[i].year = qbuffer.dispo.year;
                  seg[i].month = qbuffer.dispo.month;
                  seg[i].day = qbuffer.dispo.day;
                  seg[i].hour = qbuffer.dispo.hour;
                  seg[i].min = qbuffer.dispo.min;
                }
              }
              imprimirTabla(tablaDispositivos, seg);
              found = false;
              break;
            }

              // Para cerrar el programa, primero se elimina el contenido de la memoria compartida y de la cola de mensajes
              // Luego, se sale del bucle en el que está ejecutándose el código y se libera la memoria que había sido reservada para el programa
            case SALIR:
              exit = true;
              printf("Cerrando servidor\n");
              break;

            default:
              printf("Esto es un poco embarazoso pero ha sucedido algo que no esperábamos...\n%s.%s=%d", qbuffer.dispo.nombre, "opciones", qbuffer.dispo.opciones);
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
  return (0);
}

/*

Función que muestra la tabla con todos los dispositivos, consumo unitario y total

*/
void imprimirTabla(disp tabla[MAX_TOTAL], disp *MC)
{
  // abro el semáforo MC
  sem_t *sem_MC = NULL;
  if ((sem_MC = sem_open("MC", 0600)) == NULL)
    printf("Se ha producido un error, no se ha podido abrir el semaforo\n");
  else
  {
    // Calculamos cuantos tipos de dispositivos tenemos
    sem_wait(sem_MC);
    int dispositivos = 0;
    for (int i = 0; i < TAM_BUFFER_MC; i++)
    {
      if (MC[i].consumo != -1)
        dispositivos++;
    }
    sem_post(sem_MC);

    // Contador sirve para saber cuantos dispositivos de cada tipo tenemos
    int contador[dispositivos];
    for (int i = 0; i < dispositivos; i++)
    {
      contador[i] = 0;
    }
    printf("+---------------+-----------------------+-----------------------+-----------------------+-----------------------+\n");
    printf("|    NOMBRE\t|    Nº DISPOSITIVOS\t| CONSUMO UNITARIO(kWh)\t|   CONSUMO TOTAL(kWh)\t|    FECHA ENCENDIDO\t|\n");

    printf("+---------------+-----------------------+-----------------------+-----------------------+-----------------------+\n");

    // Por cada elemento de la tabla, buscamos aquél que coincida en nombre
    for (int i = 0; i < MAX_TOTAL; i++)
    {
      for (int j = 0; j < dispositivos; j++)
      {
        if (sem_wait(sem_MC) != 0)
          printf("ERROR: el semáforo no ha podido ser bajado\n");
        if (strcmp(tabla[i].nombre, MC[j].nombre) == 0 && tabla[i].ON == 1)
          contador[j]++;
        if (sem_post(sem_MC) != 0)
          printf("ERROR: el semáforo no ha podido ser subido\n");
      }
    }

    // Imprimimos la tabla
    float consumoTotal = 0;
    for (int i = 0; i < dispositivos; i++)
    {
      consumoTotal += contador[i] * MC[i].consumo;
      if (sem_wait(sem_MC) != 0)
        printf("ERROR: el semáforo no ha podido ser subido\n");
      printf("|%9.13s\t|%12d\t\t|%14.2f\t\t|%14.2f\t\t|    %02d/%02d/%04d %02d:%02d\t|\n", MC[i].nombre, contador[i], MC[i].consumo, (contador[i] * MC[i].consumo), MC[i].day, MC[i].month, MC[i].year, MC[i].hour, MC[i].min);
      if (sem_post(sem_MC) != 0)
        printf("ERROR: el semáforo no ha podido ser bajado\n");
    }
    printf("+---------------+-----------------------+-----------------------+-----------------------+-----------------------+\n");
    printf("Consumo total: %10.2f KWh \t|\tCoste KWh: %10.5f €/KWh \t|\tPrecio total: %10.2f €/h \t|\n\n\n\n\n", consumoTotal, COSTE_KWH, COSTE_KWH * consumoTotal);
  }
}

void iniciaRecursos()
{
  // Creamos el semaforo
  if (sem_open("MC", O_CREAT, 0600, 1) == SEM_FAILED)
    printf("Se ha producido un error en la creación del semáforo MC\n");

  // Creamos el semaforo
  if (sem_open("cola", O_CREAT, 0600, 1) == SEM_FAILED)

    printf("Se ha producido un error en la creación del semáforo cola\n");

  key_t clave;
  int shmid;
  // Creamos la memoria compartida
  clave = ftok(".", 'M');

  if ((shmid = shmget(clave, (TAM_BUFFER_MC) * sizeof(disp), IPC_CREAT | IPC_EXCL | 0660)) == -1)
    printf("Se ha producido un error, el segmento de memoria compartida ya existe\n");

  // Memoria y semáforos para los usuarios

  // Creamos el semaforo
  if (sem_open("usuarios", O_CREAT, 0600, 1) == SEM_FAILED)
    printf("Se ha producido un error en la creación del semáforo usuarios\n");

  key_t keyUsers;
  int shmidGest;
  // Creamos la memoria compartida
  keyUsers = ftok(".", 'G');

  shmidGest = shmget(keyUsers, (MAX_USUARIOS) * sizeof(int), IPC_CREAT | IPC_EXCL | 0660);

  int *segGest = shmat(shmidGest, NULL, 0);
  for (int i = 0; i < MAX_USUARIOS; i++)
    segGest[i] = 0;

  shmdt(segGest);
}

void eliminaRecursos()
{
  if (sem_unlink("cola") == 0)
    printf("El semáforo cola se ha eliminado con éxito\n");
  
  else
    printf("Se ha producido un error al eliminar el semáforo cola\n");
  
  if (sem_unlink("MC") == 0)
    printf("El semáforo MC se ha eliminado con éxito\n");
  
  else
    printf("Se ha producido un error al eliminar el semáforo MC\n");
  

  key_t clave;
  int shmid;

  // Cerramos la memoria compartida
  clave = ftok(".", 'M');

  if ((shmid = shmget(clave, (TAM_BUFFER_MC) * sizeof(disp), IPC_CREAT | 0660)) == -1)
    printf("Se ha producido un error al obtener el id del segmento de memoria\n");
  
  else
    shmctl(shmid, IPC_RMID, NULL);
  

  // Cerramos la cola
  int msgqueue_id;
  msgqueue_id = msgget(clave, IPC_CREAT | 0660);
  msgctl(msgqueue_id, IPC_RMID, NULL);

  // Memoria y semáforos de los usuarios

  if (sem_unlink("gestores") == 0)
    printf ("El semáforo usuarios se ha eliminado con éxito\n");
  
  else
    printf("Se ha producido un error al eliminar el semáforo gestusuarios\n");
  
  key_t keyUsers;
  int shmidGest;
  // Eliminamos la memoria compartida
  keyUsers = ftok(".", 'G');

  if ((shmidGest = shmget(keyUsers, (MAX_USUARIOS) * sizeof(int), IPC_CREAT | 0660)) == -1)
  
    printf("Se ha producido un error, el segmento de memoria usuarios ya existe\n");
  
  else
    shmctl(shmidGest, IPC_RMID, NULL);
}

/*

Función para leer de la cola de mensajes

*/
int leer_msg(int qid, long type, struct msgbuf *qbuf)
{
  int resultado;
  resultado = msgrcv(qid, qbuf, sizeof(disp), type, 0);
  return resultado;
}

/*

Función para escribir en la cola de mensajes

*/
int escr_msg(int qid, struct msgbuf *qbuf)
{
  int resultado;
  resultado = msgsnd(qid, qbuf, sizeof(disp), 0);
  return resultado;
}
