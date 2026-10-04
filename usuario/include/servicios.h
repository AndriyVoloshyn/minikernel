/*
 *  usuario/include/servicios.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene los prototipos de funciones de
 * biblioteca que proporcionan la interfaz de llamadas al sistema.
 *
 *      SE DEBE MODIFICAR AL INCLUIR NUEVAS LLAMADAS
 *
 */

#ifndef SERVICIOS_H
#define SERVICIOS_H

#define NO_RECURSIVO 0
#define RECURSIVO 1

/* Evita el uso del printf de la bilioteca est�ndar */
#define printf escribirf

/* Funcion de biblioteca */
int escribirf(const char *formato, ...);

/* Llamadas al sistema proporcionadas */
int crear_proceso(char *prog);
int terminar_proceso();
int escribir(char *texto, unsigned int longi);
int obtener_id_pr();
int dormir(unsigned int segundos);

/* nuevas llamadas del sistema para el mutex*/
int crear_mutex(char *nombre, int tipo);
int abrir_mutex(char *nombre);
int cerrar_mutex(unsigned int mutexid);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);

#endif /* SERVICIOS_H */

