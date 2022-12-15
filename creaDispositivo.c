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
#define MAX_TOTAL MAX_DISP *MAX_USUARIOS

#define MAX_TAM_NOMBRE 20
#define TAM_BUFFER_MC 20

#define ANADIR 1
#define CAMBIAESTADO 2
#define ELIMINAR 3
#define SALIR 4

#define NO_ASIGNADO 10

/*

Este programa es el encargado de gestionar los dispositivos
de los usuarios y comunicarselo a la base de datos para que almacene los cambios

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

pid_t pid;
time_t tiempo;
struct tm *fecha;

int interfaz_ini(char id);
void createDisp(sem_t *mutex, sem_t *cambios, dispo *seg, sem_t *MC, dispo *dispS);
void changeStateDisp(sem_t *mutex, sem_t *cambios, dispo *seg);
void deleteDisp(sem_t *mutex, sem_t *cambios, dispo *seg);
void showDisp(dispo *seg);
void deleteAll(sem_t *mutex, sem_t *cambios, dispo *seg);
char selectUser();
int iniciaRecursos(char id, sem_t **cambios, sem_t **mutex, dispo **seg);
void cierraRecursos(sem_t **cambios, sem_t **mutex, dispo **seg);

int main(int argc, char *argv[])
{

  tiempo = time(NULL);
  char id = selectUser();

  sem_t *cambios = NULL;
  sem_t *mutex = NULL;
  dispo *seg = NULL;

  if (iniciaRecursos(id, &cambios, &mutex, &seg) == 0)
    printf("Se ha producido un error al inicializar los semaforos y la memoria compartida\n");
  else
  {

    key_t key;
    int shmid;

    sem_t *MC = NULL;
    key_t keyS;
    dispo *dispS = NULL;
    int shmidDB;

    keyS = ftok(".", 'M');
    if ((MC = sem_open("MC", 0)) == SEM_FAILED)
      printf("Se ha producido un error al abrir el semaforo\n");
    else
    {
      if ((shmidDB = shmget(keyS, (MAX_DISP) * sizeof(dispo), IPC_CREAT | 0660)) == -1)
        printf("Se ha producido un error al obener el id del segmento de memoria compartida\n");
      else
      {
        if ((dispS = shmat(shmidDB, NULL, 0)) == (dispo *)-1)
          printf("Se ha producido un error al mapear el segmento\n");
        else
        {
          int opcion;   // Opción elegida por el usuario
          int exit = 1; // 1 -> No salir, 0 -> Salir
          while (exit)
          {
            opcion = interfaz_ini(id);
            switch (opcion)
            {
            case (0): // SALIR
            {
              exit = 0;
              printf("Cerrando terminal\n\n");
              break;
            }
            case (1): // AÑADIR DISPOSITIVO
            {
              createDisp(mutex, cambios, seg, MC, dispS);
              break;
            }
            case (2): // CAMBIAR ESTADO SENSOR
            {
              changeStateDisp(mutex, cambios, seg);
              break;
            }
            case (3): // BORRAR DISPOSITIVO
            {
              deleteDisp(mutex, cambios, seg);
              break;
            }
            case (4): // MOSTRAR TODOS LOS DISPOTIVOS
            {
              showDisp(seg);
              break;
            }
            case (5): // BORRAR TODOS LOS DISPOSITIVOS
            {
              deleteAll(mutex, cambios, seg);
              break;
            }
            case (6): // CAMBIAR DE USUARIO
            {
              id = selectUser(); // ID del usuario
              cierraRecursos(&cambios, &mutex, &seg);
              if (iniciaRecursos(id, &cambios, &mutex, &seg) == 0)
              {
                printf("Se ha producido un error al cambiar de usuario\n");
                exit = 1;
              }

              break;
            }
            }
          }
          shmdt(dispS);
        }
      }
      sem_close(MC);
    }
    shmdt(seg);
    sem_close(mutex);
    sem_close(cambios);
  }
  return (0);
}

/*

Función que muestra la interfaz del usuario

*/
int interfaz_ini(char id)
{

  int opcion = 0;
  if (id == '-') // En caso de error elegimos la opción de salir
    return opcion;

  printf("\nTERMINAL DEL USUARIO: %c\n", id);
  printf("Seleccione una opción:\n");
  printf("\t0->Salir\n");
  printf("\t1->Registrar nuevo dispositivo\n");
  printf("\t2->Cambiar estado de un dispositivo\n");
  printf("\t3->Eliminar un dispositivo\n");
  printf("\t4->Listar todos mis dispositivos\n");
  printf("\t5->Borrar todos los dispositivos del usuario: %c\n", id);
  printf("\t6->Cambiar de usuario\n\n");
  printf("Opción: ");
  scanf("%d", &opcion);
  printf("\n");
  while ((opcion > 6) || (opcion < 0))
  {
    printf("Introduzca una opción válida: ");
    scanf("%d", &opcion);
    printf("\n");
  }
  return (opcion);
}

/*

Función que crea un dispositivo nuevo o incrementa en 1 el número de dispositivos si ya existía uno con el mismo nombre

*/
void createDisp(sem_t *mutex, sem_t *cambios, dispo *seg, sem_t *MC, dispo *dispS)
{

  char name[1000];
  float consumo;
  printf("Introduzca nombre: ");
  scanf("%999s", name);

  sem_wait(MC);

  int existe = 0;  // Para comprobar si el dispositivo ya existe o es nuevo
  int indice = -1; // Índice del
  for (int i = 0; (existe == 0) && (i < TAM_BUFFER_MC); i++)
  {

    if (dispS[i].consumo == -1 && indice == -1)
      indice = i;

    if (strcmp(name, dispS[i].nombre) == 0)
    {
      existe = 1;
      indice = i;
    }
  }
  sem_post(MC);

  if (existe)
  { // Si el dispositivo existe, el consumo ya lo conocemos

    sem_wait(MC);
    consumo = dispS[indice].consumo;
    sem_post(MC);
  }
  else
  {
    do
    {
      printf("El dispositivo introducido es nuevo, por favor, introduzca el consumo: ");
      scanf("%f", &consumo);
      if (consumo <= 0)
        printf("Introduzca un consumo válido por favor\n");
    } while (consumo <= 0);
    sem_wait(MC);
    strcpy(dispS[indice].nombre, name); // Guardamos el nombre

    // Guardamos la fecha
    tiempo = time(NULL);
    fecha = localtime(&tiempo);

    dispS[indice].year = fecha->tm_year + 1900;
    dispS[indice].month = fecha->tm_mon;
    dispS[indice].day = fecha->tm_mday;
    dispS[indice].hour = fecha->tm_hour;
    dispS[indice].min = fecha->tm_min;
    // Guardamos el consumo
    dispS[indice].consumo = consumo;
    sem_post(MC);
    printf("\nDatos actualizados, enviando a la base de datos\n");
  }

  sem_wait(mutex);
  int hueco = -1; // Índice del hueco donde insertar el nuevo dispositivo
  for (int i = 0; i < MAX_DISP; i++)
  {
    if (seg[i].consumo == -1)
    { // Si el consumo es -1, significa que el dispositivo está vacío
      hueco = i;
      break;
      // i = MAX_DISP; // Para salir del for
    }
  }
  if (hueco == -1)
    printf("No queda ningún hueco libre en el usuario\n");
  else
  {
    strcpy(seg[hueco].nombre, name);
    seg[hueco].consumo = consumo;
    seg[hueco].ON = true;
    seg[hueco].opciones = ANADIR;
    // Fecha
    tiempo = time(NULL);
    fecha = localtime(&tiempo);

    seg[hueco].year = fecha->tm_year + 1900;
    seg[hueco].month = fecha->tm_mon;
    seg[hueco].day = fecha->tm_mday;
    seg[hueco].hour = fecha->tm_hour;
    seg[hueco].min = fecha->tm_min;
    sem_post(cambios);
    printf("Dispositivo añadido con exito\n");
  }
  sem_post(mutex);
}

/*

Función que cambia el estado de un dispotivo ENCENDIDO/APAGADO

*/
void changeStateDisp(sem_t *mutex, sem_t *cambios, dispo *seg)
{

  int hueco = -1;
  showDisp(seg);
  printf("Introduzca el id del dispositivo del que desea cambiar el estado: ");
  scanf("%d", &hueco);

  if (seg[hueco].consumo == -1 || hueco < 0 || hueco >= MAX_DISP)
    printf("Por favor, introduzca un id válido\n");
  else
  {
    sem_wait(mutex);
    if (seg[hueco].ON)
    { // Si es 1, lo ponemos a 0 y ponemos la fecha de encendido a 0

      seg[hueco].year = 0000;
      seg[hueco].month = 00;
      seg[hueco].day = 00;
      seg[hueco].hour = 00;
      seg[hueco].min = 00;
      seg[hueco].ON = 0;
      seg[hueco].opciones = CAMBIAESTADO;
    }
    else
    { // Si es 0, lo ponemos a 1 y ponemos la fecha de encendido a la actual

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
    printf("Se ha cambiado el estado del dispositivo correctamente\n");
  }
}

/*

Función que borra un dispositivo del usuario en cuestión

*/
void deleteDisp(sem_t *mutex, sem_t *cambios, dispo *seg)
{

  int hueco = -1;
  showDisp(seg);
  printf("Introduzca el id del dispositivo que desea borrar: ");
  scanf("%d", &hueco);

  if (seg[hueco].consumo == -1 || hueco < 0 || hueco >= MAX_DISP)
    printf("Por favor, introduzca un id válido\n");
  else
  {
    sem_wait(mutex);
    // Ponemos el consumo a -1, la fecha a 0 y marcamos para borrar
    seg[hueco].consumo = -1;
    seg[hueco].opciones = ELIMINAR;
    seg[hueco].year = 0000;
    seg[hueco].month = 00;
    seg[hueco].day = 00;
    seg[hueco].hour = 00;
    seg[hueco].min = 00;

    sem_post(cambios);
    sem_post(mutex);
    printf("Dispositivo borrado con éxito\n");
  }
}

/*

Función que imprime todos los dispositivos asociados a un usuario

*/
void showDisp(dispo *seg)
{

  printf("+--------+--------------+---------------+---------------+-----------------------+\n");
  printf("|  ID\t |    NOMBRE\t|   ENCENDIDO\t|    CONSUMO\t|    FECHA ENCENDIDO\t|\n");
  printf("+--------+--------------+---------------+---------------+-----------------------+\n");
  for (int i = 0; i < MAX_DISP; i++)
  {
    if (seg[i].consumo != -1)
    {
      printf("|  %d\t |%9.05s\t|%9.13s\t|%10.02f\t|    %02d/%02d/%04d %02d:%02d\t|\n", i, seg[i].nombre, seg[i].ON ? "SÍ" : "NO", seg[i].consumo, seg[i].day, seg[i].month, seg[i].year, seg[i].hour, seg[i].min);
    }
  }
  printf("+--------+--------------+---------------+---------------+-----------------------+\n");
}

/*

Función que borra todos los dispositivos asociados a un usuario

*/
void deleteAll(sem_t *mutex, sem_t *cambios, dispo *seg)
{

  char tecla[1000];

  printf("Borrar todos los dispositivos es una acción irreversible, ¿Está seguro de que desea hacerlo? s/n: ");
  scanf("%999s", tecla);

  if (tecla[0] == 's')
  {
    sem_wait(mutex);
    for (int i = 0; i < MAX_DISP; i++)
    { // Marcamos todos los dispositivos para borrar
      seg[i].consumo = -1;
      seg[i].opciones = ELIMINAR;
      seg[i].year = 0000;
      seg[i].month = 00;
      seg[i].day = 00;
      seg[i].hour = 00;
      seg[i].min = 00;
    }
    sem_post(cambios);
    sem_post(mutex);
    printf("Dispositivos borrados con exito\n");
  }
  else
  {
    printf("Operación cancelada\n");
  }
}

/*

Función que devuelve el usuario seleccionado

*/
char selectUser()
{

  sem_t *usuarios;
  key_t key;
  int *seg = NULL;
  int shmid;
  int cont = 0;

  char result[1000];

  char validos[MAX_USUARIOS];

  if ((usuarios = sem_open("usuarios", 0)) == SEM_FAILED)
    printf("Error al abrir el semaforo\n");

  else
  {
    key = ftok(".", 'G');

    if ((shmid = shmget(key, (MAX_USUARIOS) * sizeof(int), IPC_CREAT | 0660)) == -1)
      printf("Se ha producido un error al obener el id del segmento de memoria compartida\n");

    else
    {
      if ((seg = shmat(shmid, NULL, 0)) == (int *)-1)
        printf("Se ha producico un error al mapear el segmento\n");
      else
      {
        sem_wait(usuarios);
        for (int i = 0; i < MAX_USUARIOS; i++)
        {
          if (seg[i])
            cont++;
        }
        if (cont != 0)
        {
          printf("Los usuarios disponibles son: \n");
          for (int i = 0; i < MAX_USUARIOS; i++)
          {
            if (seg[i])
            {

              validos[i] = i + 48;
              printf("\t- USUARIO %c\n", i + 48);
              seg[i] = 1;
            }
          }
        }
        else
        {
          printf("No hay usuarios disponibles\n");
          result[0] = '-';
        }
        sem_post(usuarios);
        shmdt(seg);
      }
      sem_close(usuarios);
    }
  }
  // Comprueba si el usuario elegido existe
  int apto = 0;
  while ((apto == 0) && (cont != 0))
  {
    printf("Indique el usuario sobre el que desea operar: ");
    scanf("%999s", result);
    for (int i = 0; i < MAX_USUARIOS; i++)
    {
      if (validos[i] == result[0])
        apto = 1;
    }
    if (apto == 0)
      printf("Eleccion no valida\n");
  }
  return result[0];
}

/*

Función que inicia los recursos de la memoria compartida

*/
int iniciaRecursos(char id, sem_t **cambios, sem_t **mutex, dispo **seg)
{

  key_t key;
  int shmid;
  int result = 1;
  char claveMutex[] = {'m', 'u', 't', 'e', 'x', id, '\0'};
  char claveCambios[] = {'c', 'a', 'm', 'b', 'i', 'o', id, '\0'};
  char claveMemoria = id;

  if ((*cambios = sem_open(claveCambios, 0)) == SEM_FAILED)
    result = 0;

  else
  {
    if ((*mutex = sem_open(claveMutex, 0)) == SEM_FAILED)
      result = 0;

    else
    {
      key = ftok(".", claveMemoria);

      if ((shmid = shmget(key, (MAX_DISP) * sizeof(dispo), IPC_CREAT | 0660)) == -1)
        result = 0;

      else
      {
        if ((*seg = shmat(shmid, NULL, 0)) == (dispo *)-1)
          result = 0;
      }
    }
  }
  return (result);
}
/*

Función que cierra los recursos

*/
void cierraRecursos(sem_t **cambios, sem_t **mutex, dispo **seg)
{

  shmdt(*seg);
  sem_close(*cambios);
  sem_close(*mutex);
}