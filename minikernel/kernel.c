/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h" /* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc()
{
    int i;

    for (i = 0; i < MAX_PROC; i++)
        tabla_procs[i].estado = NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre()
{
    int i;

    for (i = 0; i < MAX_PROC; i++)
        if (tabla_procs[i].estado == NO_USADA)
            return i;
    return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP *proc)
{
    if (lista->primero == NULL)
        lista->primero = proc;
    else
        lista->ultimo->siguiente = proc;
    lista->ultimo = proc;
    proc->siguiente = NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista)
{

    if (lista->ultimo == lista->primero)
        lista->ultimo = NULL;
    lista->primero = lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP *proc)
{
    BCP *paux = lista->primero;

    if (paux == proc)
        eliminar_primero(lista);
    else
    {
        for (; ((paux) && (paux->siguiente != proc));
             paux = paux->siguiente)
            ;
        if (paux)
        {
            if (lista->ultimo == paux->siguiente)
                lista->ultimo = paux;
            paux->siguiente = paux->siguiente->siguiente;
        }
    }
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int()
{
    int nivel;

    printk("-> NO HAY LISTOS. ESPERA INT\n");

    /* Baja al m�nimo el nivel de interrupci�n mientras espera */
    nivel = fijar_nivel_int(NIVEL_1);
    halt();
    fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP *planificador()
{
    while (lista_listos.primero == NULL)
        espera_int(); /* No hay nada que hacer */

    /* Conceder siempre una rodaja completa al proceso que toma la CPU */
    lista_listos.primero->ticks_rodaja = TICKS_POR_RODAJA;

    return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso()
{
    BCP *p_proc_anterior;
    int nivel_previo;
    int i; /* Declaración al principio para evitar errores de compilación */

    for (i = 0; i < NUM_MUT_PROC; i++)
    {
        if (p_proc_actual->descriptores_mutex[i] != -1)
        {
            escribir_registro(1, i); /* Pasamos el descriptor en el reg 1 */
            sis_cerrar_mutex();      /* Cierra el mutex y despierta si procede */
        }
    }
    if (proc_a_expulsar == p_proc_actual)
    {
        proc_a_expulsar = NULL;
    }

    /* Liberación normal del mapa de memoria */
    liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

    p_proc_actual->estado = TERMINADO;

    /* SECCION CRITICA: eliminar de listos y planificar nuevo proceso */
    nivel_previo = fijar_nivel_int(NIVEL_3);
    eliminar_primero(&lista_listos); /* proc. fuera de listos */

    /* Realizar cambio de contexto */
    p_proc_anterior = p_proc_actual;
    p_proc_actual = planificador();

    printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
           p_proc_anterior->id, p_proc_actual->id);

    fijar_nivel_int(nivel_previo);

    liberar_pila(p_proc_anterior->pila);
    cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
    return; /* no debería llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit()
{

    if (!viene_de_modo_usuario())
        panico("excepcion aritmetica cuando estaba dentro del kernel");

    printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
    liberar_proceso();

    return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem()
{

    if (!viene_de_modo_usuario())
        panico("excepcion de memoria cuando estaba dentro del kernel");

    printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
    liberar_proceso();

    return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal()
{
    char car;

    car = leer_puerto(DIR_TERMINAL);
    printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

    return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj()
{
    BCP *actual;
    BCP *siguiente;

    /* Recorremos la lista de procesos bloqueados por reloj */
    actual = lista_bloqueados_reloj.primero;

    while (actual != NULL)
    {
        /* Guardamos el puntero al siguiente BCP antes de que 'actual'
           pueda ser modificado o extraído de la lista */
        siguiente = actual->siguiente;

        /* Decrementamos los ticks restantes del proceso */
        actual->ticks_restantes--;

        /* Si el plazo ha expirado, despertamos al proceso */
        if (actual->ticks_restantes <= 0)
        {
            /* 1. Lo quitamos de la cola de dormidos */
            eliminar_elem(&lista_bloqueados_reloj, actual);

            /* 2. Cambiamos su estado a LISTO */
            actual->estado = LISTO;

            /* 3. Lo añadimos al final de la cola de listos */
            insertar_ultimo(&lista_listos, actual);
        }

        /* Avanzamos al siguiente elemento */
        actual = siguiente;
    }
    /* Solo se consume rodaja si:
     * 1. Hay un proceso en ejecución asignado.
     * 2. Hay más de un proceso en el sistema o viene de ejecutar modo usuario (no en espera_int).
     */
    if (p_proc_actual != NULL && lista_listos.primero != NULL && lista_listos.primero->siguiente != NULL)
    {
        p_proc_actual->ticks_rodaja--;

        if (p_proc_actual->ticks_rodaja <= 0)
        {
            proc_a_expulsar = p_proc_actual;
            activar_int_SW();
        }
    }

    return;
}

/*
 * Compara dos cadenas de texto. Devuelve 0 si son iguales.
 */
static int mi_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/*
 * Copia origen en destino hasta n caracteres asegurando el fin de cadena.
 */
static void mi_strcpy(char *dest, const char *orig, int n)
{
    int i;
    for (i = 0; i < n - 1 && orig[i] != '\0'; i++)
    {
        dest[i] = orig[i];
    }
    dest[i] = '\0';
}

/*
 * Calcula la longitud de una cadena.
 */
static int mi_strlen(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

/*
 * Busca si ya existe un mutex ocupado con el nombre dado en tabla_mutex.
 * Devuelve el índice en la tabla (0..NUM_MUT-1) o -1 si no existe.
 */
static int buscar_mutex_por_nombre(const char *nombre)
{
    int i;
    for (i = 0; i < NUM_MUT; i++)
    {
        if (tabla_mutex[i].ocupado && mi_strcmp(tabla_mutex[i].nombre, nombre) == 0)
        {
            return i;
        }
    }
    return -1;
}

/*
 * Busca una entrada libre en tabla_mutex.
 * Devuelve el índice en la tabla (0..NUM_MUT-1) o -1 si está llena.
 */
static int buscar_mutex_libre()
{
    int i;
    for (i = 0; i < NUM_MUT; i++)
    {
        if (!tabla_mutex[i].ocupado)
        {
            return i;
        }
    }
    return -1;
}

/*
 * Busca un descriptor libre en el proceso actual.
 * Devuelve el índice del descriptor (0..NUM_MUT_PROC-1) o -1 si no quedan.
 */
static int buscar_descriptor_libre_proc()
{
    int i;
    for (i = 0; i < NUM_MUT_PROC; i++)
    {
        if (p_proc_actual->descriptores_mutex[i] == -1)
        {
            return i;
        }
    }
    return -1;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis()
{
    int nserv, res;

    nserv = leer_registro(0);
    if (nserv < NSERVICIOS)
        res = (tabla_servicios[nserv].fservicio)();
    else
        res = -1; /* servicio no existente */
    escribir_registro(0, res);
    return;
}

/*
 * Tratamiento de interrupciones software
 * Realiza el cambio de contexto involuntario de Round-Robin
 */
static void int_sw()
{
    BCP *p_proc_antiguo;
    int nivel_previo;

    /* 1. Sección crítica: proteger la manipulación de la cola de listos */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    if (p_proc_actual != proc_a_expulsar)
    {
        /* El proceso previo cedió voluntariamente la CPU (dormir, lock, fin).
         * Neutralizamos la interrupción software para no perjudicar al actual. */
        fijar_nivel_int(nivel_previo);
        return;
    }

    /* Limpiamos la variable una vez confirmada la expulsión */
    proc_a_expulsar = NULL;

    /* 2. Si no hay más de un proceso listo, no hay a quién cederle el procesador */
    if (lista_listos.primero == NULL || lista_listos.primero->siguiente == NULL)
    {
        /* Se le concede una nueva rodaja completa al único proceso ejecutable */
        if (p_proc_actual != NULL)
        {
            p_proc_actual->ticks_rodaja = TICKS_POR_RODAJA;
        }
        fijar_nivel_int(nivel_previo);
        return;
    }

    /* 3. Rotar el proceso actual: sacarlo de la cabeza y ponerlo al final */
    eliminar_primero(&lista_listos);
    insertar_ultimo(&lista_listos, p_proc_actual);

    /* 4. Seleccionar el nuevo proceso que pasa a ejecutar */
    p_proc_antiguo = p_proc_actual;
    p_proc_actual = planificador();

    printk("-> ROUND-ROBIN (INT SW): C.CONTEXTO de %d a %d\n",
           p_proc_antiguo->id, p_proc_actual->id);

    /* 5. Restaurar nivel previo y ejecutar cambio de contexto */
    fijar_nivel_int(nivel_previo);
    cambio_contexto(&(p_proc_antiguo->contexto_regs), &(p_proc_actual->contexto_regs));

    return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog)
{
    void *imagen, *pc_inicial;
    int error = 0;
    int proc;
    BCP *p_proc;
    int nivel_previo; /* Añadir variable para guardar el nivel */

    proc = buscar_BCP_libre();
    if (proc == -1)
        return -1; /* no hay entrada libre */

    /* A rellenar el BCP ... */
    p_proc = &(tabla_procs[proc]);

    /* crea la imagen de memoria leyendo ejecutable */
    imagen = crear_imagen(prog, &pc_inicial);
    if (imagen)
    {
        p_proc->info_mem = imagen;
        p_proc->pila = crear_pila(TAM_PILA);
        fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
                           pc_inicial,
                           &(p_proc->contexto_regs));
        p_proc->id = proc;
        p_proc->estado = LISTO;
        p_proc->ticks_rodaja = TICKS_POR_RODAJA; // Rodaja inicial completa
        int j;
        for (j = 0; j < NUM_MUT_PROC; j++)
        {
            p_proc->descriptores_mutex[j] = -1;
        }

        /* SECCION CRITICA: proteger manipulacion de lista_listos */
        nivel_previo = fijar_nivel_int(NIVEL_3);
        insertar_ultimo(&lista_listos, p_proc);
        fijar_nivel_int(nivel_previo);

        error = 0;
    }
    else
        error = -1; /* fallo al crear imagen */

    return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir sis_obtener_id_pr
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso()
{
    char *prog;
    int res;

    printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
    prog = (char *)leer_registro(1);
    res = crear_tarea(prog);
    return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
    char *texto;
    unsigned int longi;

    texto = (char *)leer_registro(1);
    longi = (unsigned int)leer_registro(2);

    escribir_ker(texto, longi);
    return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso()
{

    printk("-> FIN PROCESO %d\n", p_proc_actual->id);

    liberar_proceso();

    return 0; /* no deber�a llegar aqui */
}

// meter aqui las llamadas a sistema!!
/*
 * Tratamiento de llamada al sistema obtener_id_pr. Devuelve el
 * identifcador del proceso en ejeciución
 */
int sis_obtener_id_pr()
{
    return p_proc_actual->id;
}

/*
 * Tratamiento de llamada al sistema dormir
 * Bloquea al proceso invocador durante los segundos especificados
 */
int sis_dormir()
{
    unsigned int segundos;
    BCP *p_proc_antiguo;
    int nivel_previo;

    /* 1. Obtener el parametro recibido en el registro 1 */
    segundos = (unsigned int)leer_registro(1);

    /* Si se piden 0 segundos, no hace falta bloquearse */
    if (segundos == 0)
        return 0;

    /* 2. Seccion critica: elevar nivel a NIVEL_3 para evitar que int_reloj
          interrumpa mientras modificamos colas de BCPs */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    /* 3. Calcular ticks restantes (prohibido usar leer_reloj_CMOS) */
    p_proc_actual->ticks_restantes = segundos * TICK;

    /* 4. Cambiar estado a BLOQUEADO */
    p_proc_actual->estado = BLOQUEADO;

    /* 5. Mover el proceso: sacarlo de lista_listos y meterlo en lista_bloqueados_reloj */
    eliminar_primero(&lista_listos);
    insertar_ultimo(&lista_bloqueados_reloj, p_proc_actual);

    if (proc_a_expulsar == p_proc_actual)
    {
        proc_a_expulsar = NULL;
    }

    /* 6. Conmutar contexto voluntario:
          Guardamos referencia al que se duerme y elegimos el nuevo proceso */
    p_proc_antiguo = p_proc_actual;
    p_proc_actual = planificador();

    printk("-> DORMIR: C.CONTEXTO de %d a %d (ticks: %d)\n",
           p_proc_antiguo->id, p_proc_actual->id, p_proc_antiguo->ticks_restantes);

    /* 7. Restaurar nivel de interrupcion antes de conmutar */
    fijar_nivel_int(nivel_previo);

    /* 8. Salvar registros del proceso saliente y cargar los del nuevo */
    cambio_contexto(&(p_proc_antiguo->contexto_regs), &(p_proc_actual->contexto_regs));

    return 0;
}

static void iniciar_tabla_mutex()
{
    int i;
    for (i = 0; i < NUM_MUT; i++)
    {
        tabla_mutex[i].ocupado = 0;
        tabla_mutex[i].nombre[0] = '\0';
        tabla_mutex[i].propietario = NULL;
        tabla_mutex[i].num_locks = 0;
        tabla_mutex[i].ref_count = 0;
        tabla_mutex[i].cola_bloqueados.primero = NULL;
        tabla_mutex[i].cola_bloqueados.ultimo = NULL;
    }
}

/*
 * Tratamiento de llamada al sistema crear_mutex.
 * Parámetro en registro 1: char *nombre
 * Parámetro en registro 2: int tipo
 */

int sis_crear_mutex()
{
    char *nombre;
    int tipo;
    int desc_libre;
    int idx_mut;
    int nivel_previo;
    BCP *p_proc_antiguo;

    nombre = (char *)leer_registro(1);
    tipo = (int)leer_registro(2);

    /* 1. Validar parámetros */
    if (nombre == NULL || mi_strlen(nombre) >= MAX_NOM_MUT)
    {
        return -1;
    }
    if (tipo != NO_RECURSIVO && tipo != RECURSIVO)
    {
        return -1;
    }

    /* 2. Comprobar si el proceso tiene descriptores libres */
    desc_libre = buscar_descriptor_libre_proc();
    if (desc_libre == -1)
    {
        return -1; /* Sin descriptores libres en el proceso */
    }

    /* 3. Proteger la consulta y posible bloqueo */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    /* Comprobar si ya existe un mutex con ese nombre */
    if (buscar_mutex_por_nombre(nombre) != -1)
    {
        fijar_nivel_int(nivel_previo);
        return -1; /* Ya existe un mutex con ese nombre */
    }

    /* 4. Si la tabla está llena, bloquearse en bucle hasta que haya hueco */
    while ((idx_mut = buscar_mutex_libre()) == -1)
    {
        p_proc_actual->estado = BLOQUEADO;

        /* Sacar de lista de listos y meter en la cola de espera de hueco */
        eliminar_primero(&lista_listos);
        insertar_ultimo(&lista_bloqueados_crear_mutex, p_proc_actual);

        /* Conmutación voluntaria a otro proceso */
        p_proc_antiguo = p_proc_actual;
        p_proc_actual = planificador();

        printk("-> CREAR_MUTEX: bloqueando proc %d (tabla llena). C.CONTEXTO a %d\n",
               p_proc_antiguo->id, p_proc_actual->id);

        fijar_nivel_int(nivel_previo);
        cambio_contexto(&(p_proc_antiguo->contexto_regs), &(p_proc_actual->contexto_regs));

        /* Cuando el proceso despierta y recupera la CPU: */
        nivel_previo = fijar_nivel_int(NIVEL_3);

        /* Al despertar, otro proceso pudo haber creado un mutex con el mismo nombre */
        if (buscar_mutex_por_nombre(nombre) != -1)
        {
            fijar_nivel_int(nivel_previo);
            return -1;
        }
        /* El bucle while volverá a evaluar si efectivamente hay un mutex libre */
    }

    /* 5. Inicializar la entrada del mutex en la tabla global */
    tabla_mutex[idx_mut].ocupado = 1;
    mi_strcpy(tabla_mutex[idx_mut].nombre, nombre, MAX_NOM_MUT);
    tabla_mutex[idx_mut].tipo = tipo;
    tabla_mutex[idx_mut].propietario = NULL;
    tabla_mutex[idx_mut].num_locks = 0;
    tabla_mutex[idx_mut].ref_count = 1; /* Abierto por quien lo crea */
    tabla_mutex[idx_mut].cola_bloqueados.primero = NULL;
    tabla_mutex[idx_mut].cola_bloqueados.ultimo = NULL;

    /* 6. Asignar descriptor en el proceso actual */
    p_proc_actual->descriptores_mutex[desc_libre] = idx_mut;

    fijar_nivel_int(nivel_previo);

    return desc_libre;
}

int sis_abrir_mutex()
{
    char *nombre;
    int desc_libre;
    int idx_mut;
    int nivel_previo;

    nombre = (char *)leer_registro(1);

    /* 1. Validación de parámetros */
    if (nombre == NULL || mi_strlen(nombre) >= MAX_NOM_MUT)
    {
        return -1;
    }

    /* 2. Comprobar si el proceso tiene descriptores disponibles */
    desc_libre = buscar_descriptor_libre_proc();
    if (desc_libre == -1)
    {
        return -1; // Agotados los descriptores locales
    }

    /* 3. Sección crítica: consultar tabla global de mutex */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    idx_mut = buscar_mutex_por_nombre(nombre);
    if (idx_mut == -1)
    {
        fijar_nivel_int(nivel_previo);
        return -1; /* El mutex no existe */
    }

    /* 4. Asociar descriptor al mutex e incrementar referencias */
    p_proc_actual->descriptores_mutex[desc_libre] = idx_mut;
    tabla_mutex[idx_mut].ref_count++;

    fijar_nivel_int(nivel_previo);

    /* Devuelve el descriptor obtenido */
    return desc_libre;
}

/*
 * Tratamiento de llamada al sistema cerrar_mutex.
 * Parámetro en registro 1: unsigned int mutexid (descriptor del proceso)
 */
int sis_cerrar_mutex()
{
    unsigned int mutexid;
    int idx_mut;
    int nivel_previo;
    BCP *proc_despertado;

    mutexid = (unsigned int)leer_registro(1);

    /* 1. Validar si el descriptor local está en un rango correcto */
    if (mutexid >= NUM_MUT_PROC)
    {
        return -1;
    }

    /* 2. Sección crítica: validar si el descriptor está en uso y modificar estructuras */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    idx_mut = p_proc_actual->descriptores_mutex[mutexid];
    if (idx_mut == -1)
    {
        fijar_nivel_int(nivel_previo);
        return -1; /* El descriptor no apuntaba a ningún mutex */
    }

    /* Si el proceso actual era el dueño, liberación implícita del mutex */
    if (tabla_mutex[idx_mut].propietario == p_proc_actual)
    {
        tabla_mutex[idx_mut].propietario = NULL;
        tabla_mutex[idx_mut].num_locks = 0;

        if (tabla_mutex[idx_mut].cola_bloqueados.primero != NULL)
        {
            BCP *despertado = tabla_mutex[idx_mut].cola_bloqueados.primero;
            eliminar_primero(&(tabla_mutex[idx_mut].cola_bloqueados));

            tabla_mutex[idx_mut].propietario = despertado;
            tabla_mutex[idx_mut].num_locks = 1;

            despertado->estado = LISTO;
            insertar_ultimo(&lista_listos, despertado);
        }
    }

    /* 3. Desasociar descriptor del proceso */
    p_proc_actual->descriptores_mutex[mutexid] = -1;

    /* 4. Decrementar el contador de referencias global */
    tabla_mutex[idx_mut].ref_count--;

    /* 5. Si no quedan procesos con este mutex abierto, liberarlo */
    if (tabla_mutex[idx_mut].ref_count == 0)
    {

        /* Limpiar campos de la entrada del mutex */
        tabla_mutex[idx_mut].ocupado = 0;
        tabla_mutex[idx_mut].nombre[0] = '\0';
        tabla_mutex[idx_mut].propietario = NULL;
        tabla_mutex[idx_mut].num_locks = 0;
        tabla_mutex[idx_mut].cola_bloqueados.primero = NULL;
        tabla_mutex[idx_mut].cola_bloqueados.ultimo = NULL;

        /* 6. Al quedar un hueco libre en tabla_mutex, comprobar si hay procesos esperándolo */
        if (lista_bloqueados_crear_mutex.primero != NULL)
        {
            proc_despertado = lista_bloqueados_crear_mutex.primero;
            eliminar_primero(&lista_bloqueados_crear_mutex);

            proc_despertado->estado = LISTO;
            insertar_ultimo(&lista_listos, proc_despertado);

            printk("-> CERRAR_MUTEX: despertando proc %d bloqueado en crear_mutex\n", proc_despertado->id);
        }
    }

    fijar_nivel_int(nivel_previo);

    return 0; /* Éxito al cerrar */
}

/*
 * Tratamiento de llamada al sistema lock.
 * Parámetro en registro 1: unsigned int mutexid (descriptor local)
 */
int sis_lock()
{
    unsigned int mutexid;
    int idx_mut;
    int nivel_previo;
    BCP *p_proc_antiguo;

    mutexid = (unsigned int)leer_registro(1);

    /* 1. Validar rango del descriptor local */
    if (mutexid >= NUM_MUT_PROC)
    {
        return -1;
    }

    /* 2. Sección crítica */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    idx_mut = p_proc_actual->descriptores_mutex[mutexid];
    if (idx_mut == -1 || !tabla_mutex[idx_mut].ocupado)
    {
        fijar_nivel_int(nivel_previo);
        return -1;
    }

    /* 3. Caso: el proceso actual YA es el propietario */
    if (tabla_mutex[idx_mut].propietario == p_proc_actual)
    {
        if (tabla_mutex[idx_mut].tipo == NO_RECURSIVO)
        {
            /* Error: rebloqueo en mutex no recursivo */
            fijar_nivel_int(nivel_previo);
            return -1;
        }
        else
        {
            /* Mutex RECURSIVO: incrementamos el anidamiento y retornamos éxito */
            tabla_mutex[idx_mut].num_locks++;
            fijar_nivel_int(nivel_previo);
            return 0;
        }
    }

    /* 4. Caso: el mutex está completamente libre */
    if (tabla_mutex[idx_mut].propietario == NULL)
    {
        tabla_mutex[idx_mut].propietario = p_proc_actual;
        tabla_mutex[idx_mut].num_locks = 1;
        fijar_nivel_int(nivel_previo);
        return 0;
    }

    /* 5. Caso: el mutex pertenece a otro proceso -> Bloquearse */
    p_proc_actual->estado = BLOQUEADO;
    eliminar_primero(&lista_listos);
    insertar_ultimo(&(tabla_mutex[idx_mut].cola_bloqueados), p_proc_actual);

    if (proc_a_expulsar == p_proc_actual)
    {
        proc_a_expulsar = NULL;
    }

    p_proc_antiguo = p_proc_actual;
    p_proc_actual = planificador();

    printk("-> LOCK: bloqueando proc %d en mutex '%s'. C.CONTEXTO a %d\n",
           p_proc_antiguo->id, tabla_mutex[idx_mut].nombre, p_proc_actual->id);

    fijar_nivel_int(nivel_previo);
    cambio_contexto(&(p_proc_antiguo->contexto_regs), &(p_proc_actual->contexto_regs));

    return 0;
}

/*
 * Tratamiento de llamada al sistema unlock.
 * Parámetro en registro 1: unsigned int mutexid (descriptor local)
 */
int sis_unlock()
{
    unsigned int mutexid;
    int idx_mut;
    int nivel_previo;
    BCP *proc_despertado;

    mutexid = (unsigned int)leer_registro(1);

    /* 1. Validar rango del descriptor local */
    if (mutexid >= NUM_MUT_PROC)
    {
        return -1;
    }

    /* 2. Sección crítica */
    nivel_previo = fijar_nivel_int(NIVEL_3);

    idx_mut = p_proc_actual->descriptores_mutex[mutexid];
    if (idx_mut == -1 || !tabla_mutex[idx_mut].ocupado)
    {
        fijar_nivel_int(nivel_previo);
        return -1;
    }

    /* 3. Solo el propietario puede hacer unlock */
    if (tabla_mutex[idx_mut].propietario != p_proc_actual)
    {
        fijar_nivel_int(nivel_previo);
        return -1;
    }

    /* 4. Decrementar contador de anidamiento */
    tabla_mutex[idx_mut].num_locks--;

    /* Si todavía quedan locks anidados, el mutex continúa ocupado por el mismo proceso */
    if (tabla_mutex[idx_mut].num_locks > 0)
    {
        fijar_nivel_int(nivel_previo);
        return 0;
    }

    /* 5. num_locks llegó a 0: liberación real del mutex */
    if (tabla_mutex[idx_mut].cola_bloqueados.primero != NULL)
    {
        proc_despertado = tabla_mutex[idx_mut].cola_bloqueados.primero;
        eliminar_primero(&(tabla_mutex[idx_mut].cola_bloqueados));

        /* Transferencia directa de posesión al proceso despertado */
        tabla_mutex[idx_mut].propietario = proc_despertado;
        tabla_mutex[idx_mut].num_locks = 1;

        proc_despertado->estado = LISTO;
        insertar_ultimo(&lista_listos, proc_despertado);
    }
    else
    {
        /* Nadie espera: queda libre */
        tabla_mutex[idx_mut].propietario = NULL;
    }

    fijar_nivel_int(nivel_previo);
    return 0;
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main()
{
    /* se llega con las interrupciones prohibidas */

    instal_man_int(EXC_ARITM, exc_arit);
    instal_man_int(EXC_MEM, exc_mem);
    instal_man_int(INT_RELOJ, int_reloj);
    instal_man_int(INT_TERMINAL, int_terminal);
    instal_man_int(LLAM_SIS, tratar_llamsis);
    instal_man_int(INT_SW, int_sw);

    iniciar_cont_int();       /* inicia cont. interr. */
    iniciar_cont_reloj(TICK); /* fija frecuencia del reloj */
    iniciar_cont_teclado();   /* inici cont. teclado */
    iniciar_tabla_proc();     /* inicia BCPs de tabla de procesos */
    iniciar_tabla_mutex();

    /* crea proceso inicial */
    if (crear_tarea((void *)"init") < 0)
        panico("no encontrado el proceso inicial");

    /* activa proceso inicial */
    p_proc_actual = planificador();
    cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
    panico("S.O. reactivado inesperadamente");
    return 0;
}
