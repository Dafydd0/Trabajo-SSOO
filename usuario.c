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

#define MAX_DISP 20
#define MAX_USUARIOS 5
#define MAX_TAM_NOMBRE 20

#define ANADIR 1
#define CAMBIAESTADO 2
#define ELIMINAR 3
#define SALIR 4

#define NO_ASIGNADO 10

/*

Este programa es el encargado de crear los usuarios que podrán gestionar los
dispositivos que se almacenarán en ls base de datos

*/

typedef struct dispositivo
{
  char nombre[MAX_TAM_NOMBRE]; // Nombre
  float consumo;               // Consumo
  int opciones;                // Opciones
  bool ON;                     // Encendido
  // Fecha encendido
  int year;
  int month;
  int day;
  int hour;
  int min;
} dispo;

struct buff_msg
{
  long mtype;
  dispo dispo;
};

pid_t pid;
time_t tiempo;
struct tm *fecha;

int salir = 0; // Variable que dicta cuando salir del bucle infinito, es decir, cuando se quiere cerrar la base de datos
int interfaz_inicio(char id);

char obtenerId();
void liberarId(char id);

int escr_msg(int qid, struct buff_msg *qbuf);
int leer_msg(int qid, long type, struct buff_msg *qbuf);

void iniciaRecursos(char id);
void eliminaRecursos(char id);

void exitFun();

int main()
{

  char id = obtenerId();
  iniciaRecursos(id);
  sem_t *cambios = NULL;
  sem_t *mutex = NULL;
  sem_t *cola = NULL;

  key_t clave;
  dispo *seg = NULL;
  int shmid;

  key_t claveCola;
  int msgqueue_id;
  struct buff_msg qbuffer;

  if (id == '!')
    printf("Se ha producido un error, ya hay %d usuarios\n", MAX_USUARIOS);
  else
  {
    char claveMutex[] = {'m', 'u', 't', 'e', 'x', id, '\0'};
    char claveCambios[] = {'c', 'a', 'm', 'b', 'i', 'o', id, '\0'};
    char claveMemoria = id;
    claveCola = ftok(".", 'q');
    if ((msgqueue_id = msgget(claveCola, IPC_CREAT | 0660)) == -1)
      printf("Error al iniciar la cola\n");

    else
    {
      if ((cambios = sem_open(claveCambios, 0)) == SEM_FAILED)
        printf("Se ha producido un error al abrir el semaforo de cambios\n");

      else
      {
        if ((mutex = sem_open(claveMutex, 0)) == SEM_FAILED)
          printf("Se ha producido un error al abrir el semaforo de mutex\n");

        else
        {
          if ((cola = sem_open("cola", 0)) == SEM_FAILED)
            printf("Se ha producido un error al abrir el semaforo de cola\n");

          else
          {
            clave = ftok(".", claveMemoria);

            if ((shmid = shmget(clave, (MAX_DISP) * sizeof(dispo), IPC_CREAT | 0660)) == -1)
              printf("Se ha producido un erroor al obener el id del segmento de memoria compartida\n");

            else
            {
              if ((seg = shmat(shmid, NULL, 0)) == (dispo *)-1)
                printf("Se ha producido un error al mapear el segmento\n");
              else
              {
                struct sigaction act;

                // Inicialización de memoria compartida
                sem_wait(mutex);
                for (int i = 0; i < MAX_DISP; i++)
                {
                  // Al inicializar la memoria compartida ponemos todos los dispositivos a "0"
                  seg[i].consumo = -1;
                  seg[i].opciones = NO_ASIGNADO;
                  seg[i].year = 0000;
                  seg[i].month = 00;
                  seg[i].day = 00;
                  seg[i].hour = 00;
                  seg[i].min = 00;
                }
                sem_post(mutex);

                /* Se crea el proceso hijo */
                pid = fork();

                switch (pid)
                {

                case -1: /* error del fork() */
                  perror("fork");
                  break;

                case 0: /* proceso hijo*/

                  // Este proceso se encarga de comprobar las actualizaciones en los dispositivos y comunicarselo a la base de datos

                  act.sa_handler = exitFun; /*función a ejecutar*/
                  act.sa_flags = 0;         /* ninguna acción especifica */
                  sigemptyset(&act.sa_mask);
                  sigaction(1, &act, NULL);
                  dispo tabla[MAX_DISP];

                  // Entramos en un bucle infinito para detectar cambios en la MC, en caso de que haya se lo comunicamos a la base de datos por una cola de mensaje
                  while (salir == 0)
                  {
                    sem_wait(cambios);
                    sem_wait(mutex);
                    for (int i = 0; i < MAX_DISP; i++)
                    {
                      tabla[i] = seg[i];
                      seg[i].opciones = NO_ASIGNADO;
                    }
                    sem_post(mutex);

                    for (int i = 0; i < MAX_DISP; i++)
                    {
                      if (tabla[i].opciones != NO_ASIGNADO)
                      {
                        qbuffer.mtype = 1;
                        qbuffer.dispo = tabla[i];
                        sem_wait(cola);
                        escr_msg(msgqueue_id, &qbuffer);
                        sem_post(cola);
                      }
                    }
                    sleep(1); // Esperamos un poco para evitar errores
                  }

                  for (int i = 0; i < MAX_DISP; i++)
                    seg[i].consumo = -1;
                  break;

                default: /* padre*/
                  // El proceso padre se encarga de mostrar la interfaz de usuario
                  while (salir == 0)
                  {
                    int select = interfaz_inicio(id);
                    switch (select)
                    {
                    case (1): // LISTAR LOS DISPOSITIVOS
                    {
                      printf("+---------------+---------------+---------------+-----------------------+\n");
                      printf("|    NOMBRE\t|   ENCENDIDO\t|    CONSUMO\t|    FECHA ENCENDIDO\t|\n");
                      printf("+---------------+---------------+---------------+-----------------------+\n");
                      for (int i = 0; i < MAX_DISP; i++)
                      {
                        if (seg[i].consumo != -1)
                        {
                          printf("|%9.13s\t|%9.05s\t|%10.02f\t|    %02d/%02d/%04d %02d:%02d\t|\n", seg[i].nombre, seg[i].ON ? "SÍ" : "NO", seg[i].consumo, seg[i].day, seg[i].month, seg[i].year, seg[i].hour, seg[i].min);
                        }
                      }
                      printf("+---------------+---------------+---------------+-----------------------+\n");
                      break;
                    }
                    case (2): // ELIMINAR USUARIO Y DISPOSITIVOS
                    {
                      salir = 1;
                      liberarId(id);
                      sem_wait(mutex);
                      for (int i = 0; i < MAX_DISP; i++)
                      {
                        if (seg[i].consumo != -1)
                          seg[i].opciones = ELIMINAR;
                      }
                      sem_post(mutex);
                      sem_post(cambios);
                      sleep(3);
                      kill(pid, 1);
                      break;
                    }
                    case (3): // ELIMINAR USUARIO Y CERRAR BASE DE DATOS
                    {
                      salir = 1;
                      liberarId(id);
                      sem_wait(mutex);
                      seg[0].opciones = SALIR;
                      sem_post(mutex);
                      sem_post(cambios);
                      sleep(3);
                      kill(pid, 1);
                      break;
                    }
                    }
                  }
                  break;
                }
                shmdt(seg);
              }
            }
            sem_close(mutex);
          }
          sem_close(cola);
        }
        sem_close(cambios);
      }
      if (pid != 0)
      {
        eliminaRecursos(id);
      }
    }
  }
  return (0);
}

/*

Función que muestra la interfaz del usuario y devuelve la opción seleccionada

*/
int interfaz_inicio(char id)
{
  int select = 0;
  printf("\nTERMINAL DEL USUARIO %c\n", id);
  printf("Seleccione que desea hacer:\n");
  printf("\t1->Listar mis dispositivos\n");
  printf("\t2->Eliminar usuario y dispositivos de la base de datos\n");
  printf("\t3->Eliminar usuario y cerrar sevidor\n\n");
  printf("Opción: ");
  scanf("%d", &select);
  printf("\n");
  while ((select > 3) || (select < 1))
  {
    printf("Por favor, introduzca una opción adecuada: ");
    scanf("%d", &select);
    printf("\n");
  }
  return (select);
}

/*

Función que devuelve el ID del usuario

*/
char obtenerId()
{
  sem_t *usuarios;
  key_t clave;
  int *seg = NULL;
  int shmid;

  int hueco = -1;

  if ((usuarios = sem_open("usuarios", 0)) == SEM_FAILED)
    printf("Se ha producido un error al abrir el semaforo de usuarios\n");

  else
  {
    clave = ftok(".", 'G');

    if ((shmid = shmget(clave, (MAX_USUARIOS) * sizeof(int), IPC_CREAT | 0660)) == -1)
      printf("Se ha produciro un error al obener el id del segmento de memoria compartida\n");

    else
    {
      if ((seg = shmat(shmid, NULL, 0)) == (int *)-1)
        printf("Se ha producido un rrror al mapear el segmento\n");
      else
      {
        sem_wait(usuarios);
        for (int i = 0; i < MAX_USUARIOS && hueco == -1; i++)
        {
          if (seg[i] == 0)
          {
            hueco = i;
            seg[i] = 1;
          }
        }
        sem_post(usuarios);
        shmdt(seg);
      }
      sem_close(usuarios);
    }
  }
  char result;
  if (hueco == -1)
    result = '!';
  else
    result = hueco + 48;
  return result;
}

/*

Función que libera el ID del usuario

*/
void liberarId(char id)
{
  sem_t *usuarios;
  key_t clave;
  int *seg = NULL;
  int shmid;

  int hueco;

  if ((usuarios = sem_open("usuarios", 0)) == SEM_FAILED)
    printf("Se ha producido un error al abrir el semaforo\n");

  else
  {
    clave = ftok(".", 'G');

    if ((shmid = shmget(clave, (MAX_USUARIOS) * sizeof(int), IPC_CREAT | 0660)) == -1)
      printf("Se ha producido un error al obtener el id del segmento de memoria compartida\n");

    else
    {
      if ((seg = shmat(shmid, NULL, 0)) == (int *)-1)
        printf("Se ha producido un error al mapear el segmento\n");
      else
      {
        hueco = id - 48;
        sem_wait(usuarios);
        seg[hueco] = 0;
        sem_post(usuarios);
        shmdt(seg);
      }
      sem_close(usuarios);
    }
  }
}

/*

Función para escribir en la cola de mensajes

*/
int escr_msg(int qid, struct buff_msg *qbuf)
{
  int resultado;

  resultado = msgsnd(qid, qbuf, sizeof(dispo), 0);

  return (resultado);
}

/*

Función para leer de la cola de mensajes

*/
int leer_msg(int qid, long type, struct buff_msg *qbuf)
{
  int resultado;

  resultado = msgrcv(qid, qbuf, sizeof(dispo), type, 0);

  return (resultado);
}

/*

Función que inicializa los recursos a utilizar

*/
void iniciaRecursos(char id)
{
  char claveMutex[] = {'m', 'u', 't', 'e', 'x', id, '\0'};
  char claveCambios[] = {'c', 'a', 'm', 'b', 'i', 'o', id, '\0'};
  char claveMemoria = id;
  if (sem_open(claveCambios, O_CREAT, 0600, 0) == SEM_FAILED)
    printf("Se ha producido un error al crear el semáforo cambios\n");

  if (sem_open(claveMutex, O_CREAT, 0600, 1) == SEM_FAILED)

    printf("Error en la creación del semáforo mutex\n");

  key_t clave;
  int shmid;

  clave = ftok(".", claveMemoria);

  if ((shmid = shmget(clave, (MAX_DISP) * sizeof(dispo), IPC_CREAT | IPC_EXCL | 0660)) == -1)
    printf("Se ha producido un error, el segmento de memoria compartida ya existe\n");
}

/*

Función que elimina los recursos utilizados

*/
void eliminaRecursos(char id)
{
  char claveMutex[] = {'m', 'u', 't', 'e', 'x', id, '\0'};
  char claveCambios[] = {'c', 'a', 'm', 'b', 'i', 'o', id, '\0'};
  char claveMemoria = id;
  if (sem_unlink(claveCambios) == 0)
  {
    // printf("El semáforo cambios se ha eliminado con éxito\n");
  }
  else
    printf("Se ha producido un erro al eliminar el semáforo cambios\n");

  if (sem_unlink(claveMutex) == 0)
  {
    // printf("El semáforo mutex se ha eliminado con éxito\n");
  }

  else
    printf("Se ha producido un error al eliminar el semáforo mutex\n");

  key_t clave;
  int shmid;

  clave = ftok(".", claveMemoria);

  if ((shmid = shmget(clave, (MAX_DISP) * sizeof(dispo), IPC_CREAT | 0660)) == -1)
    printf("Se ha producido un error al obtener el id del segmento de memoria\n");

  else
    shmctl(shmid, IPC_RMID, NULL);
}

/*

Función que modifica el valor de la variable global 'salir' para que se salga del bucle y se cierre la base de datos

*/
void exitFun(void)
{
  salir = 1;
}
