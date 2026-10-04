/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#define NO_RECURSIVO 0
#define RECURSIVO 1

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;			/* dir. inicial de la pila */
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */
	int ticks_restantes;		/* ticks de reloj que le faltan para despertar*/
	int descriptores_mutex[NUM_MUT_PROC];	/* Mapeo descriptor local -> indice tabla mutex*/
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;

/* Definicion de la estructura del mutex*/

typedef struct {
    char nombre[MAX_NOM_MUT];  /* Nombre del mutex (máx 8 caracteres con \0) */
    int tipo;                  /* NO_RECURSIVO o RECURSIVO */
    int ocupado;               /* 1 si el mutex existe/está creado en el sistema, 0 si libre */
    BCP *propietario;          /* Puntero al BCP que tiene el lock actual (o NULL si libre) */
    int num_locks;             /* Contador de anidamiento para recursivos */
    int ref_count;             /* Número de descriptores apuntando a este mutex */
    lista_BCPs cola_bloqueados;/* Procesos esperando a obtener este mutex (lock) */
} mutex_t;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/*
 * Variable global que representa la cola de procesos bloqueados por reloj
 */
lista_BCPs lista_bloqueados_reloj = {NULL, NULL};

/* tabla global del mutex del sistema*/
mutex_t tabla_mutex[NUM_MUT];

/* Cola de procesos esperando a que se libere una entrada en la tabla_mutex*/
lista_BCPs lista_bloqueados_crear_mutex ={NULL, NULL};

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int sis_obtener_id_pr();
int sis_dormir();
int sis_crear_mutex();
int sis_abrir_mutex();
int sis_cerrar_mutex();
int sis_lock();
int sis_unlock();

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	
					{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{sis_obtener_id_pr},
					{sis_dormir},
					{sis_crear_mutex},
					{sis_abrir_mutex},
					{sis_cerrar_mutex},
					{sis_lock},
					{sis_unlock}};

#endif /* _KERNEL_H */

