
#include "CONFP.h"
#include "ltmdaemon.h"
#include "ltmipcs.h"


const struct timeval timeout = {0, 1500}; // 1.5 ms
//extern int32_t num_retx_pkt=0;
//extern float media_num_retx_pkt=0;
int32_t num_sec_inicial=0;


lista_paquetes *busca_pkt_buf(lista_paquetes *buffer_tx, int32_t num_secuencia) {
  
  //fprintf(stderr,"ESTAMOS BUSCANDO %i\n", num_secuencia);
  while (buffer_tx != NULL) {
    //fprintf(stderr,"Y hay... %i con tamaño_datos....%i y dato %X\n",buffer_tx->pkt.cabecera.num_secuencia,buffer_tx->pkt.cabecera.cantidad_datos,buffer_tx->pkt.datos[0]);
    if (buffer_tx->pkt.cabecera.num_secuencia == num_secuencia) {
      return buffer_tx;
    }
    buffer_tx = buffer_tx->sig;
  }
  return NULL;
}

void ordena_buffer(conexion_t *cx, int RXTX) {
  lista_paquetes *lista_pkts,*paquete,*anterior=NULL;
  int8_t primero=1;
  
  // paquete va a ser el pkt que hay que ordenar, que sera el primero de la lista
  // ademas "primero" sera el puntero cabeza de la lista
  if (RXTX == RX) lista_pkts = cx->buffer_rx;
  else lista_pkts = cx->buffer_tx;
  if (lista_pkts == NULL) return;
  paquete = lista_pkts;
  lista_pkts = lista_pkts->sig;
  while (lista_pkts != NULL) {
    if (primero == 1) {
      if (lista_pkts->pkt.cabecera.num_secuencia < paquete->pkt.cabecera.num_secuencia) {
	paquete->sig = lista_pkts->sig;
	lista_pkts->sig = paquete;
	if (RXTX == RX) cx->buffer_rx = lista_pkts;
	else cx->buffer_tx = lista_pkts;
	anterior=lista_pkts;
	lista_pkts=paquete;
	primero=0;
      }
    }
    else if (lista_pkts->pkt.cabecera.num_secuencia < paquete->pkt.cabecera.num_secuencia) {
	paquete->sig = lista_pkts->sig;
	lista_pkts->sig = paquete;
	anterior->sig=lista_pkts;
	anterior = lista_pkts;
	lista_pkts = paquete;
    }
    lista_pkts=lista_pkts->sig;
  }
  
  return;
}
/*
int termineitor2(conexion_t *cx) {
    int contador=0;//i;
    char ip6charbuf[INET6_ADDRSTRLEN], ip6charbuf2[INET6_ADDRSTRLEN];
    
    while (cx != NULL) {
        contador++;
        fprintf(stderr,"cx->ip_local %s puerto_local %i\n cx->ip_remota %s puerto_remoto %d\n",inet_ntop(AF_INET6,&cx->ip_local,ip6charbuf,INET6_ADDRSTRLEN), cx->puerto_local,inet_ntop(AF_INET6,&cx->ip_remota,ip6charbuf2,INET6_ADDRSTRLEN), cx->puerto_remoto);
        cx = cx->sig;
    }
    printf("El numero de conexion es %i\n",contador);
    return 1;
}
*/
// elimina el primer nodo del buffer

lista_paquetes *elimina_buffer(lista_paquetes *lista_pkts, int8_t RXTX) {
    lista_paquetes *lista_pkts_aux=lista_pkts;
    
    if (lista_pkts == NULL) return lista_pkts;
    lista_pkts=lista_pkts->sig;
    free_kernel_mem(lista_pkts_aux);
    if (RXTX == TX) KERNEL->num_pkts_buf_tx++;
    else KERNEL->num_pkts_buf_rx++;
    return lista_pkts;

}

// elimina un nodo del buffer de TX segun su num_secuencia

lista_paquetes *elimina_buffer_tx(lista_paquetes *lista_pkts, int32_t num_secuencia) {
  lista_paquetes *lista_pkts_aux,*anterior,*primero;

  lista_pkts_aux = lista_pkts;
  anterior = lista_pkts;
  primero = lista_pkts;
  while (lista_pkts != NULL) {
    if (lista_pkts->pkt.cabecera.num_secuencia == num_secuencia) {
      if (anterior == lista_pkts) primero=lista_pkts->sig;
      lista_pkts_aux = lista_pkts;
      anterior->sig = lista_pkts->sig;
      lista_pkts = lista_pkts->sig;
      free_kernel_mem(lista_pkts_aux);
      KERNEL->num_pkts_buf_tx++;
    }
    anterior=lista_pkts;
    if (lista_pkts != NULL) lista_pkts = lista_pkts->sig;
  }
  return primero;
}


// cuenta los datos del buffer_rx y verifica que estan los paquetes en orden listos para leer
int cuenta_datos(lista_paquetes *lista_pkts, int longitud,conexion_t *cx) {
  int datos=0;
  int32_t num_aux=num_sec_inicial; // numero auxiliar para comprobar que estan los datos que corresponde en el buffer
 
  
  if (lista_pkts == NULL) return 0;
  if (lista_pkts->pkt.cabecera.ultimo == CLOSE) cx->flujo_entrada = CLOSE;
  if (lista_pkts->pkt.cabecera.num_secuencia != num_sec_inicial) return 0;
  while ((lista_pkts != NULL) && (datos < longitud)) {
    if (lista_pkts->pkt.cabecera.num_secuencia == num_aux) {
      datos += lista_pkts->pkt.cabecera.cantidad_datos;
      num_aux++;
    }
    else {
      num_sec_inicial +=num_aux;
      num_aux = 0;
      return datos;// no estan todos los que quiero seguidos, pero devuelvo cuantos hay seguidos
    }
    lista_pkts = lista_pkts->sig;
  }
  num_sec_inicial = num_aux;
  return datos;
}


// construye un paquete ACK

cabecera_t construye_ack(conexion_t *cx, int num_secuencia) {
  cabecera_t cabecera;
  cabecera.tipo_pkt = ACK;
  cabecera.puerto_origen = cx->puerto_local;
  cabecera.puerto_destino = cx->puerto_remoto;
  cabecera.num_secuencia = num_secuencia;
  cabecera.mapa_bits = mapea_ventana_rx(cx);
  cabecera.posic_ventana = cx->posic_ventana_rx;
  cabecera.cantidad_datos = 0;
  cabecera.ultimo = 0;
  return cabecera;
}

// comprueba si la ventana tiene una posicion ocupada o devuelve la posicion si esta disponible

int8_t comprueba_ventana(conexion_t *cx,int32_t num_secuencia,int8_t RXTX) {
    int8_t pos_array;
    pos_array = num_secuencia % TAMANHO_VENTANA;  
    if (RXTX == TX) {
      if (cx->ventana_tx[pos_array].pkt == NULL) return pos_array;
      else return -1; // ventana no disponible
    }
    else if (cx->ventana_rx[pos_array].pkt == NULL) return pos_array;
    else return -1;
}

// comprueba si toda la ventana_tx esta llena o devuelve el indice de la primera posicion vacia

int8_t comprueba_toda_ventana(conexion_t *cx) {
  int8_t i;
  for (i=0;i<TAMANHO_VENTANA;i++) {
    if (cx->ventana_tx[i].pkt == NULL) return i;
  }
  return -1; // ventana completamente llena
}

// comprueba la ventana de recepcion y hace el mapa de bits con los paquetes que se han recibido.
uint32_t mapea_ventana_rx(conexion_t *cx) {
  uint32_t mapa=0x00000000;
  int8_t i,pos_array;
  int32_t posic_ventana_aux;
  posic_ventana_aux = cx->posic_ventana_rx;
  pos_array = posic_ventana_aux % TAMANHO_VENTANA;
  for (i=0;i<TAMANHO_VENTANA;i++) {           
    if (cx->ventana_rx[pos_array].pkt != NULL) mapa = mapa | (uint32_t)pow(2,i); // hacemos OR logico para calcular el mapa de bits de los pkts recibidos
    posic_ventana_aux++;
    pos_array = posic_ventana_aux % TAMANHO_VENTANA;
  }
  return mapa;
}

// procesa los paquetes que van llegando al bprincipal por el case DT

int8_t procesar_paquetes_rx(paquete_tcp *pkt, conexion_t *cx) {
  int8_t pos_array;
  //if (pkt->cabecera.ultimo == CLOSE) fprintf(stderr," COJONESS_Pro_RX\n");
  if (pkt->cabecera.num_secuencia < cx->posic_ventana_rx) {
    KERNEL->num_pkts_buf_rx++;
    return -2; // lo que ha llegado es un pkt antiguo que ya teniamos
  }
  
  if ((cx->posic_ventana_rx <= pkt->cabecera.num_secuencia) && (pkt->cabecera.num_secuencia < cx->posic_ventana_rx + TAMANHO_VENTANA)) {   // comprueba si está dentro de la ventana
    pos_array = comprueba_ventana(cx,pkt->cabecera.num_secuencia,RX);
    if (pos_array >= 0) {
      anhadir_pkt(DT,cx,pkt->cabecera.num_secuencia,pkt->datos,pkt->cabecera.cantidad_datos,RX);
      ordena_buffer(cx,RX);
      if (pkt->cabecera.ultimo == CLOSE) {
	cx->flujo_entrada = CLOSE; // si ultimo pkt => cerramos flujo
      }
      cx->ventana_rx[pos_array].pkt = &(cx->buffer_rx->pkt);
      cx->ventana_rx[pos_array].pevento = NULL;
      return 1;
    }
  }
  KERNEL->num_pkts_buf_rx++;
  return -1;
    
}



// construye y añade un paquete a la lista de paquetes (al reves la lista!!!)

void anhadir_pkt(int tipo_pkt,conexion_t *cx,int num_secuencia,const void *datos,size_t tamanho_datos, int8_t RXTX) {
  lista_paquetes *lista_pkts;
  
  lista_pkts = (lista_paquetes *)alloc_kernel_mem(sizeof(lista_paquetes));
  lista_pkts->pkt.cabecera.tipo_pkt = tipo_pkt;
  lista_pkts->pkt.cabecera.puerto_origen = cx->puerto_local;
  lista_pkts->pkt.cabecera.puerto_destino = cx->puerto_remoto;
  lista_pkts->pkt.cabecera.num_secuencia = num_secuencia;
  lista_pkts->pkt.cabecera.cantidad_datos = tamanho_datos;
  lista_pkts->pkt.cabecera.ultimo = 0;
  if (RXTX == TX) {  
    if (cx->flujo_salida == CLOSE) {
      lista_pkts->pkt.cabecera.ultimo = CLOSE;
    }
  }
  memcpy(&lista_pkts->pkt.datos,datos,tamanho_datos);
  if (RXTX == TX) {
    lista_pkts->sig = cx->buffer_tx;
    cx->buffer_tx = lista_pkts;
  }
  else {
    lista_pkts->sig = cx->buffer_rx;
    cx->buffer_rx = lista_pkts;
  }
}

//Copia el contenido de datos de un paquete, lo guarda en memoria y luego borra el paquete del buffer

lista_paquetes *copia_datos(conexion_t *cx, void **datos, int *cantidad_recibida, int *cantidad_temporal, int *long_aux){
  lista_paquetes *lista_pkts=cx->buffer_rx;
  memcpy(*datos,&lista_pkts->pkt.datos,lista_pkts->pkt.cabecera.cantidad_datos);
  *datos += lista_pkts->pkt.cabecera.cantidad_datos;
  *cantidad_recibida += lista_pkts->pkt.cabecera.cantidad_datos;
  *cantidad_temporal -= lista_pkts->pkt.cabecera.cantidad_datos;
  *long_aux -= lista_pkts->pkt.cabecera.cantidad_datos;      
  cx->buffer_rx = elimina_buffer(cx->buffer_rx,RX);
  lista_pkts = cx->buffer_rx;
  return lista_pkts;
}

//si puede, mete el pkt en la ventana y lo envía

void enventana_pkt(conexion_t *cx, lista_paquetes *lista_pkts, size_t *longitud) {
  int pos_array;
  eventos *pevento;

  if (lista_pkts->pkt.cabecera.num_secuencia < cx->posic_ventana_tx + TAMANHO_VENTANA) {
    pos_array = comprueba_ventana(cx,cx->posic_ventana_tx,TX);
    if (pos_array != comprueba_ventana(cx,lista_pkts->pkt.cabecera.num_secuencia,TX)) pos_array = -1;
  }
  else pos_array = -1;
  if (pos_array >= 0) {
    pos_array = lista_pkts->pkt.cabecera.num_secuencia % TAMANHO_VENTANA;
    if (cx->ventana_tx[pos_array].pkt == NULL) { // el pkt del buffer cabe en la ventana
      cx->ventana_tx[pos_array].pkt = &lista_pkts->pkt;
      enviar_tpdu(cx->ip_remota, &lista_pkts->pkt, lista_pkts->pkt.cabecera.cantidad_datos+TAM_CAB);
      pevento = crea_evento(KERNEL,cx,lista_pkts->pkt.cabecera.num_secuencia,DT,NUM_RTX);
      cx->ventana_tx[pos_array].pevento = pevento;	
      *longitud -= lista_pkts->pkt.cabecera.cantidad_datos;
    }
  }
}

//mete paquetes en el buffer de tx hasta que se acaben los datos o no quede sitio en el buffer y devuelve el puntero a los datos que queden

void anhade_buffer_tx(conexion_t *cx, const void *datos, size_t longitud,size_t *datos_pal_buffer, int8_t *flags) {
  size_t tamanho_datos;
  while ((*datos_pal_buffer>0) && (KERNEL->num_pkts_buf_tx>0)){
    if (*datos_pal_buffer >= TAM_DATOS) tamanho_datos = TAM_DATOS; // comprobamos la longitud de lo que queda por enviar
    else tamanho_datos = *datos_pal_buffer;     // para ver si usamos toda la capacidad del campo datos o menos.
    *datos_pal_buffer -= tamanho_datos;
    if (((*flags & CLOSE) == CLOSE) && (*datos_pal_buffer == 0))  {
      cx->flujo_salida = CLOSE;
    }
    anhadir_pkt(DT,cx,cx->num_secuencia,datos,tamanho_datos,TX);  // añadimos el pkt a la lista_paquetes
    ordena_buffer(cx,TX);
    cx->num_secuencia++;
    KERNEL->num_pkts_buf_tx--;
    datos += tamanho_datos;
  }
}

// comprueba mapa de bits y aNULLea las posiciones correspondientes y sus eventos

void comprobar_mapa_bits(conexion_t *cx,uint32_t mapa_bits) {
  int8_t i,pos_array;
  uint32_t mapa_aux;
  int32_t posic_ventana_aux;
  int8_t actualizado=0;
  posic_ventana_aux = cx->posic_ventana_tx;
  pos_array = posic_ventana_aux % TAMANHO_VENTANA; 
  for (i=0;i<TAMANHO_VENTANA;i++) {
    mapa_aux = mapa_bits & 1;
    if (mapa_aux == 1){
      if (cx->ventana_tx[pos_array].pkt != NULL) {
	cx->buffer_tx = elimina_buffer_tx(cx->buffer_tx,cx->ventana_tx[pos_array].pevento->num_secuencia);
	cx->ventana_tx[pos_array].pkt = NULL;
	elimina_evento(KERNEL,cx,cx->ventana_tx[pos_array].pevento->num_secuencia);
	cx->ventana_tx[pos_array].pevento = NULL;
      }
    }
    else if (!actualizado) {
      cx->posic_ventana_tx += i;
      actualizado=1;
    }
    posic_ventana_aux++;
    pos_array = posic_ventana_aux % TAMANHO_VENTANA;
    mapa_bits = mapa_bits >> 1;
  }
}

// aNULLea una ventana y desplaza segun la cantidad aNULLeada

void aNULLear_ventana(conexion_t *cx,int32_t posic_ventana) {
  int8_t i;
  int32_t num_pos_aux,pos_array;
  num_pos_aux = cx->posic_ventana_tx;
  for (i=0;i<(posic_ventana-num_pos_aux);i++) {
    pos_array = cx->posic_ventana_tx % TAMANHO_VENTANA;
    cx->buffer_tx = elimina_buffer_tx(cx->buffer_tx,cx->posic_ventana_tx);
    cx->ventana_tx[pos_array].pkt = NULL;
    elimina_evento(KERNEL,cx,cx->posic_ventana_tx); 
    cx->ventana_tx[pos_array].pevento = NULL;
    cx->posic_ventana_tx++; 
  }
}


int busca_puerto(int16_t puerto) {
  conexion_t *cx = KERNEL->cx;
  while (cx != NULL) {
    if ((memcmp(&cx->puerto_local,&puerto,sizeof(int16_t)) == 0) || (memcmp(&cx->puerto_remoto,&puerto,sizeof(int16_t)) == 0)) return 1;
    cx = cx->sig;
  }
  return 0;
   
}

int busca_puerto_remoto(conexion_t *cx, int16_t puerto) {
  conexion_t *conex = cx;
  
  while (conex != NULL){
    if (conex->puerto_remoto == puerto) return 1;
    conex = conex->sig;
  }
  return 0;
  
}

//devuelve la cx si la encuentra o sino devuelve NULL
conexion_t *comprobar_conexion_bprincipal(kernel_shm_t *KERNEL,const t_direccion *tsap_destino,const t_direccion *tsap_origen) {
    conexion_t *cx=NULL;
    cx = KERNEL->cx;
    while (cx != NULL) {
        if (cx->puerto_local == tsap_destino->puerto) {
            if (memcmp(&cx->ip_local,&tsap_destino->ip,sizeof(struct in6_addr)) == 0) return cx;
        }
        cx = cx->sig;
    }
    return NULL; // conexion se puede crear
}


//devuelve la cx si ya existe o NULL si se puede crear
conexion_t *comprobar_conexion_connect(kernel_shm_t *KERNEL,const t_direccion *tsap_destino,const t_direccion *tsap_origen) {
  conexion_t *cx=NULL;
  
  cx = KERNEL->cx;
  while (cx != NULL) {
    if (memcmp(&cx->ip_local,&tsap_origen->ip,sizeof(struct in6_addr)) == 0) {  // encontro ip_local
      if (tsap_origen->puerto != 0) {  // puerto dado distinto de 0
	if (cx->puerto_remoto == tsap_origen->puerto) {
	  return cx;  // coincide ip_local <-> puerto_local con otra ya creada
	}
      }
    }
    cx = cx->sig;
  }
  return NULL; // conexion se puede crear
}



//devuelve cx si ya existe o NULL si se puede crear
conexion_t *comprobar_conexion_listen(kernel_shm_t *KERNEL,const t_direccion *tsap_escucha,const t_direccion *tsap_remota) {
  conexion_t *cx=KERNEL->cx;
  
  while (cx != NULL) {
    if ((memcmp(&cx->ip_remota,&tsap_remota->ip,sizeof(struct in6_addr)) == 0) && memcmp(&cx->ip_local,&tsap_escucha->ip,sizeof(struct in6_addr)) == 0) {
      if ((cx->puerto_remoto == tsap_remota->puerto) && (cx->puerto_local == tsap_escucha->puerto)) return cx; // coincide ip_remota <-> puerto_remoto
        
    }
    cx = cx->sig;
  }
  return NULL;
}
	  
int is_address_cero(const struct in6_addr *addr_to_test) {
  struct in6_addr ipcero;
  inet_pton(AF_INET6, "::", &ipcero);
  if (memcmp(&ipcero, addr_to_test, sizeof(struct in6_addr)) == 0) return 1;
  else return 0;
}

// devuelve el puntero a la conexion con la id que pasas por parametro (que coincide con el puerto local)

conexion_t *busca_conex(int id) {
  conexion_t *cx = KERNEL->cx;
  
  while (cx != NULL) {
    if (cx->puerto_local == id) return(cx);
    cx = cx->sig;
  }
  return(cx);
}


// devuelve un puntero a la conexion en la que la IP es "::" o NULL si no hay una IP "::".
conexion_t *busca_conex_zero(kernel_shm_t *KERNEL) {
  conexion_t *pconex;
  
  if (KERNEL->cx == NULL) {
    return(NULL);
  }// lista de conexiones vacia
  pconex = KERNEL->cx;
  while (pconex != NULL) {
    if (is_address_cero(&pconex->ip_remota)){
      return(pconex);
    }
    pconex = pconex->sig;
  }
  return(pconex);
}

// crea conexion no asigna el estado, el estado se asigna despues de llamar a crea_conexion

conexion_t *crea_conexion(kernel_shm_t *KERNEL,const t_direccion *tsap_destino,const t_direccion *tsap_origen){
  conexion_t *conex;
  
  conex = KERNEL->cx;
  if (KERNEL->cx == NULL) {  // si no hay ninguna conexion en la lista, crea la primera
    conex = (conexion_t *)alloc_kernel_mem(sizeof(conexion_t));
    inicia_barrera(&conex->barC);
    conex->ip_local = tsap_origen->ip;
    conex->ip_remota = tsap_destino->ip;
    conex->puerto_local = tsap_origen->puerto;
    conex->puerto_remoto = tsap_destino->puerto;
    conex->ventana_tx = (ventana_rtx *)alloc_kernel_mem(sizeof(ventana_rtx)*TAMANHO_VENTANA);   // inicializamos
    conex->ventana_rx = (ventana_rtx *)alloc_kernel_mem(sizeof(ventana_rtx)*TAMANHO_VENTANA); 
    conex->posic_ventana_tx = 0;
    conex->posic_ventana_rx = 0; 
    conex->flujo_salida = 0;
    conex->flujo_entrada = 0;
    conex->buffer_tx = alloc_kernel_mem(sizeof(struct _lista_paquetes)); // toda 
    conex->buffer_rx = alloc_kernel_mem(sizeof(struct _lista_paquetes)); // la
    conex->buffer_tx = NULL; // la pesca
    conex->buffer_rx = NULL;
    conex->sig = NULL;
    KERNEL->cx = conex; // porque es el primer elemento
  }
  else {  // si no es la primera conexion que se crea se va al final de la lista y se crea
    while (conex->sig != NULL) {
      conex = conex->sig;
    }
    conex->sig = (conexion_t *)alloc_kernel_mem(sizeof(conexion_t));
    conex = conex->sig;
    inicia_barrera(&conex->barC);
    conex->ip_local = tsap_origen->ip;
    conex->ip_remota = tsap_destino->ip;
    conex->puerto_local = tsap_origen->puerto;
    conex->puerto_remoto = tsap_destino->puerto;
    conex->ventana_tx = (ventana_rtx *)alloc_kernel_mem(sizeof(ventana_rtx)*TAMANHO_VENTANA);   // inicializamos
    conex->ventana_rx = (ventana_rtx *)alloc_kernel_mem(sizeof(ventana_rtx)*TAMANHO_VENTANA);
    conex->posic_ventana_tx = 0;
    conex->posic_ventana_rx = 0;  
    conex->flujo_salida = 0;
    conex->flujo_entrada = 0;
    conex->buffer_tx = alloc_kernel_mem(sizeof(struct _lista_paquetes)); // toda
    conex->buffer_rx = alloc_kernel_mem(sizeof(struct _lista_paquetes)); // la
    conex->buffer_tx = NULL; // pesca
    conex->buffer_rx = NULL;
    conex->sig = NULL;
  }
  
  return(conex);
  
  /*
////////////////////// ticher edition
conex = alloc_kernel...;
conex->sig = KERNEL->cx;
KERNEL->cx = conex;

return conex;*/
}

void elimina_conexion(conexion_t *cx) {
  conexion_t *cx_aux = KERNEL->cx;

  if (cx_aux == cx) {  // si queremos eliminar el primer nodo de la lista
    cx_aux = cx_aux->sig;
    KERNEL->cx = cx_aux;
    free_kernel_mem(cx);
  }
  else {
    while (cx_aux->sig != cx) cx_aux = cx_aux->sig; // nos colocamos delante de la cx a eliminar
    cx_aux->sig = cx->sig; // apuntamos a la que esta delante de la que borramos
    free_kernel_mem(cx); // y la borramos
  }
}

//Crea un evento con los parámetros indicados, si es el único de la lista llamará a interrumpe_daemon()
eventos *crea_evento(kernel_shm_t *KERNEL, conexion_t *cx,int32_t num_secuencia, int tipo_evento, int rtx){
  struct timeval lahora,vence;
  eventos *peventos;
  
  peventos = KERNEL->peventos;
  if (gettimeofday(&lahora,NULL) < 0) printf("FALLO gettimeofday\n");
  timeradd(&lahora,&timeout,&vence);  // vence = hora + timeout
  if (peventos == NULL) {
    peventos = (eventos *)alloc_kernel_mem(sizeof(eventos));
    peventos->vence = vence;
    peventos->cx = cx;
    peventos->rtx = rtx;
    peventos->num_secuencia = num_secuencia;
    peventos->tipo_evento = tipo_evento;
    peventos->sig = NULL;
    KERNEL->peventos = peventos;
    interrumpe_daemon();  // al ser el primer elemento que se añade hay que llamar a interrumpe_daemon (en interfaz.c solo!) no solo en interfaz!!!!!
    return(peventos);
  }
  else {
    while (peventos->sig != NULL) {
      peventos = peventos->sig;
    }
    peventos->sig = (eventos *)alloc_kernel_mem(sizeof(eventos));
    peventos = peventos->sig;
    peventos->vence = vence;
    peventos->cx = cx;
    peventos->rtx = rtx;
    peventos->num_secuencia = num_secuencia;
    peventos->tipo_evento = tipo_evento;
    peventos->sig = NULL;
  }
  return(peventos);
}

void elimina_evento(kernel_shm_t *KERNEL, conexion_t *cx, int32_t num_secuencia){
  eventos *peventos=KERNEL->peventos;
  eventos *anterior;
  if (peventos != NULL) {
    anterior = peventos;
    if ((peventos->cx == cx) && (peventos->num_secuencia == num_secuencia)) {     // eliminar el primer elemento de la lista
      KERNEL->peventos = KERNEL->peventos->sig;
      free_kernel_mem(peventos);
    }
    while (peventos->sig != NULL) {
      peventos = peventos->sig;
      if ((peventos->cx == cx) && (peventos->num_secuencia == num_secuencia)) {
	while (anterior->sig != peventos) anterior = anterior->sig;
	anterior->sig = peventos->sig;
	free_kernel_mem(peventos);
      }
    }
  }
}

eventos *busca_evento(eventos *peventos, int tipo_evento){
  while (peventos != NULL) {
    if (peventos->tipo_evento == tipo_evento) return(peventos);
    peventos = peventos->sig;
  }
  return(peventos);
}


int calcula_shortest(eventos *peventos) {
  struct timeval resultado, lahora;
  int shortest;
  
  if (peventos == NULL) return -1;  // no hay eventos
  if (gettimeofday(&lahora,NULL) < 0) printf("FALLO EN GETTIMEOFDAY\n");
  timersub(&peventos->vence,&lahora,&resultado);
  shortest = resultado.tv_sec*1000 + resultado.tv_usec/1000;
  if (shortest <= 0)  return 0; // ha vencido el evento
  else return(shortest);  // devolvemos shortest en ms
}


conexion_t *rtx_CR() {  
  eventos *peventos = KERNEL->peventos;
  eventos *evento_aux = (eventos *)alloc_kernel_mem(sizeof(eventos));
  paquete_tcp *pkt=(paquete_tcp *)alloc_kernel_mem(sizeof(paquete_tcp));
  conexion_t *cx=NULL;

  if (peventos->rtx == 0) {
    
    cx = peventos->cx;
    elimina_evento(KERNEL,peventos->cx,peventos->num_secuencia);
    return cx;
  }
  else {
    //printf("retransmito CR\n");
    peventos->rtx--;
    pkt->cabecera.tipo_pkt = CR;
    pkt->cabecera.puerto_origen = peventos->cx->puerto_local;
    pkt->cabecera.puerto_destino = peventos->cx->puerto_remoto;
    pkt->cabecera.num_secuencia = peventos->num_secuencia;
    evento_aux->rtx = peventos->rtx;
    evento_aux->num_secuencia = peventos->num_secuencia;
    evento_aux->cx = peventos->cx;
    evento_aux->tipo_evento = CR;
    evento_aux->sig = NULL;
    elimina_evento(KERNEL,evento_aux->cx,evento_aux->num_secuencia);
    enviar_tpdu(evento_aux->cx->ip_remota, pkt, sizeof(cabecera_t));
    crea_evento(KERNEL,evento_aux->cx, pkt->cabecera.num_secuencia,CR,evento_aux->rtx);
    free_kernel_mem(evento_aux);
    free_kernel_mem(pkt);
    return NULL;
  }
}

int rtx_CC() {  
  eventos *peventos = KERNEL->peventos;
  eventos *evento_aux = (eventos *)alloc_kernel_mem(sizeof(eventos));
  paquete_tcp *pkt=(paquete_tcp *)alloc_kernel_mem(sizeof(paquete_tcp));

  if (peventos->rtx == 0) {
    elimina_evento(KERNEL,peventos->cx,peventos->num_secuencia);
    return(EXNET);
  }
   else {
     //printf("retransmito CC\n");
    peventos->rtx--;
    pkt->cabecera.tipo_pkt = CC;
    pkt->cabecera.puerto_origen = peventos->cx->puerto_local;
    pkt->cabecera.puerto_destino = peventos->cx->puerto_remoto;
    pkt->cabecera.num_secuencia = peventos->num_secuencia;
    evento_aux->rtx = peventos->rtx;
    evento_aux->num_secuencia = peventos->num_secuencia;
    evento_aux->cx = peventos->cx;
    evento_aux->tipo_evento = CC;
    evento_aux->sig = NULL;
    elimina_evento(KERNEL,evento_aux->cx,evento_aux->num_secuencia);
    enviar_tpdu(evento_aux->cx->ip_remota, pkt, sizeof(cabecera_t));
    crea_evento(KERNEL,evento_aux->cx, pkt->cabecera.num_secuencia,CC,evento_aux->rtx);
    free_kernel_mem(evento_aux);
    free_kernel_mem(pkt);
    return 0;
  }
}

int rtx_DR() { 
  eventos *peventos = KERNEL->peventos;
  eventos *evento_aux = (eventos *)alloc_kernel_mem(sizeof(eventos));
  paquete_tcp *pkt=(paquete_tcp *)alloc_kernel_mem(sizeof(paquete_tcp));

  if (peventos->rtx == 0) {
    elimina_evento(KERNEL,peventos->cx,peventos->num_secuencia);
    return(EXNET);
  }
  else {
    //printf("retransmito DR\n");
    peventos->rtx--;
    pkt->cabecera.tipo_pkt = DR;
    pkt->cabecera.puerto_origen = peventos->cx->puerto_local;
    pkt->cabecera.puerto_destino = peventos->cx->puerto_remoto;
    pkt->cabecera.num_secuencia = peventos->num_secuencia;
    evento_aux->rtx = peventos->rtx;
    evento_aux->num_secuencia = peventos->num_secuencia;
    evento_aux->cx = peventos->cx;
    evento_aux->tipo_evento = DR;
    evento_aux->sig = NULL;
    elimina_evento(KERNEL,evento_aux->cx,evento_aux->num_secuencia);
    enviar_tpdu(evento_aux->cx->ip_remota, pkt, sizeof(cabecera_t));
    crea_evento(KERNEL,evento_aux->cx, pkt->cabecera.num_secuencia,DR,evento_aux->rtx);
    free_kernel_mem(evento_aux);
    free_kernel_mem(pkt);
    return 0;
  }
}

int rtx_DT() {
  eventos *peventos = KERNEL->peventos;
  eventos *evento_aux = (eventos *)alloc_kernel_mem(sizeof(eventos));
  paquete_tcp *pkt=NULL;
  int8_t indice_ventana;

  if (peventos->rtx == 0) {
    elimina_evento(KERNEL,peventos->cx,peventos->num_secuencia);
    return(EXNET);
  }
  else {
    //fprintf(stderr,"retransmito DT\n");
    peventos->rtx--;
    indice_ventana = peventos->num_secuencia % TAMANHO_VENTANA;
    pkt = peventos->cx->ventana_tx[indice_ventana].pkt;
    evento_aux->rtx = peventos->rtx;
    evento_aux->num_secuencia = peventos->num_secuencia;
    evento_aux->cx = peventos->cx;
    evento_aux->tipo_evento = DT;
    evento_aux->sig = NULL;
    elimina_evento(KERNEL,evento_aux->cx,evento_aux->num_secuencia);
    enviar_tpdu(evento_aux->cx->ip_remota, pkt, pkt->cabecera.cantidad_datos+TAM_CAB);
    // if (pkt->cabecera.ultimo != 0) fprintf(stderr,"al hacer rtx pkt.cabecera.ultimo=%i\n",pkt->cabecera.ultimo);
    evento_aux->cx->ventana_tx[indice_ventana].pevento = crea_evento(KERNEL,evento_aux->cx, pkt->cabecera.num_secuencia,DT,evento_aux->rtx);
    free_kernel_mem(evento_aux);
    return 0;
  }
}
