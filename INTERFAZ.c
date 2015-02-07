œ#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>


#include "interfaz.h"
#include "CONFP.h"

kernel_shm_t * KERNEL = NULL;
char dir_proto[64] = KERNEL_MEM_ADDR;
char ip6charbuf[INET6_ADDRSTRLEN],ip6charbuf2[INET6_ADDRSTRLEN];

////////////////////////////////////////////////////////////
//                                                        //  
//                      t_connect                         //                 
//                                                        //
////////////////////////////////////////////////////////////


int t_connect(const t_direccion *tsap_destino, t_direccion *tsap_origen) {
  int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
  struct in6_addr ip_local;
  paquete_tcp *pkt=(paquete_tcp *)alloc_kernel_mem(sizeof(paquete_tcp));
  conexion_t *cx = KERNEL->cx;
  eventos *peventos = KERNEL->peventos;
  int salida;
  
  
  if (er < 0)
    return EXKERNEL;
  
 
  bloquear_acceso(&KERNEL->semaforo);  
  //fprintf(stderr, "hacemos un t_connect\n");
  
   if (is_address_cero(&tsap_origen->ip)) {  
    obtener_IP_local((const char *)KERNEL,&ip_local);
    memcpy(&tsap_origen->ip,&ip_local,sizeof(struct in6_addr));
  }
  
  if ((memcmp(&tsap_origen->ip,&ip_local,sizeof(struct in6_addr)) != 0) || (memcmp(&tsap_destino->ip,&ip_local,sizeof(struct in6_addr)) == 0)) {
    desbloquear_acceso(&KERNEL->semaforo);  
    ltm_exit_kernel((void**) & KERNEL);
    return EXINVA;
  }
  
  if (comprobar_conexion_connect(KERNEL,tsap_destino,tsap_origen) != NULL ) {
    desbloquear_acceso(&KERNEL->semaforo);  
    ltm_exit_kernel((void**) & KERNEL);
    return EXCDUP;  // ya existe la conexion
  }
  if (tsap_origen->puerto == 0) {  
    srand(time(NULL));
    do {
      tsap_origen->puerto = (int16_t) (1 + rand() % 32767);
    } while (busca_puerto(tsap_origen->puerto));
  }
 
  if (KERNEL->num_conexiones == MAX_CONEX) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);  
    return EXMAXC; // demasiadas conexiones
  }
  cx = crea_conexion(KERNEL,tsap_destino,tsap_origen);
  KERNEL->num_conexiones++;
  cx->estado = CR;

  pkt->cabecera.tipo_pkt = CR;
  pkt->cabecera.puerto_origen = tsap_origen->puerto;
  pkt->cabecera.puerto_destino = tsap_destino->puerto;
  pkt->cabecera.num_secuencia = 0;
  
  
  enviar_tpdu(tsap_destino->ip, pkt, sizeof(cabecera_t));
  
  
  peventos = crea_evento(KERNEL,cx, pkt->cabecera.num_secuencia,CR,NUM_RTX);

  cx->estado = ZZ;
  desbloquear_acceso(&KERNEL->semaforo);
  bloquea_llamada(&(cx->barC)); 
  
  // al despertar tenemos que comprobar si se ha rechazado la conexion o no
  
  bloquear_acceso(&KERNEL->semaforo);
  //fprintf(stderr,"hemos vuelto al connect!!\n");

  if (cx->estado == EXNET) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXNET;
  }
  if (cx->estado == CN) {
    KERNEL->num_conexiones--;
    elimina_conexion(cx);
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return(EXNOTSAP); //conexión rechazada
  }
  
  salida = cx->puerto_local;
  //fprintf(stderr,"ANTES DE SALIR DE t_connect la lista de cx esta asi:\n");
  //termineitor2(KERNEL->cx);
  //fprintf(stderr, "salimos del t_connect\n");
  desbloquear_acceso(&KERNEL->semaforo);
  ltm_exit_kernel((void**) & KERNEL);
  return salida;
}


////////////////////////////////////////////////////////////
//                                                        //  
//                      t_listen                          //                 
//                                                        //
////////////////////////////////////////////////////////////

int t_listen(t_direccion *tsap_escucha, t_direccion *tsap_remota) { // escucha = origen y remota = destino
  int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
  conexion_t *cx = KERNEL->cx;
  int salida;
  // char ip6charbuf[INET6_ADDRSTRLEN], ip6charbuf2[INET6_ADDRSTRLEN];;
  struct in6_addr ip_local;
  if (er < 0)
    return EXKERNEL;
  
  bloquear_acceso(&KERNEL->semaforo);
  // fprintf(stderr, "hacemos un t_listen\n");
    
  obtener_IP_local((const char *)KERNEL,&ip_local); 
  if (is_address_cero(&tsap_escucha->ip)) tsap_escucha->ip = ip_local;
  
  if (memcmp(&tsap_escucha->ip,&ip_local,sizeof(struct in6_addr)) != 0) {
    desbloquear_acceso(&KERNEL->semaforo);  
    ltm_exit_kernel((void**) & KERNEL);
    return EXINVA; //argumentos incorrectos
  }
  
  if (KERNEL->num_conexiones == MAX_CONEX){
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);  
    return EXMAXC; //demasiadas conexiones
  }
  
  if (comprobar_conexion_listen(KERNEL,tsap_escucha,tsap_remota) != NULL) {
    desbloquear_acceso(&KERNEL->semaforo);  
    ltm_exit_kernel((void**) & KERNEL);
    return EXCDUP;  // ya existe la conexion
  }
  
  
  //  printf("Creamos la conex en t_listen\n");
  cx = crea_conexion(KERNEL,tsap_remota,tsap_escucha);
  KERNEL->num_conexiones++;
    
    

  //  printf("cx->ip_local = %s y puerto %d\ncx->ip_remota = %s y puerto %d\n",inet_ntop(AF_INET6,&cx->ip_local,ip6charbuf,INET6_ADDRSTRLEN), cx->puerto_local,inet_ntop(AF_INET6,&cx->ip_remota,ip6charbuf2,INET6_ADDRSTRLEN), cx->puerto_remoto);
  
  cx->estado = ZZ;
  desbloquear_acceso(&KERNEL->semaforo);  
  bloquea_llamada(&(cx->barC));
  bloquear_acceso(&KERNEL->semaforo);
  tsap_escucha->puerto = cx->puerto_local;
    tsap_remota->ip = cx->ip_remota;
    tsap_remota->puerto = cx->puerto_remoto;
  if (cx->estado == EXNET) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXNET;
  }
  salida = cx->puerto_local;
  //fprintf(stderr,"ANTES DE SALIR DE t_listen la lista de cx esta asi:\n");
  //termineitor2(KERNEL->cx);  
  //fprintf(stderr, "salimos del t_listen\n");
  desbloquear_acceso(&KERNEL->semaforo);
  ltm_exit_kernel((void**) & KERNEL);
  
  return salida;
}

////////////////////////////////////////////////////////////
//                                                        //  
//                      t_flush                           //                 
//                                                        //
////////////////////////////////////////////////////////////

int t_flush(int id) {
  int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
  int8_t nosquedamos=1;
  conexion_t *cx;
  if (er < 0)
    return EXKERNEL;
  
  bloquear_acceso(&KERNEL->semaforo);
  cx = busca_conex(id);

  if (cx == NULL) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return(EXBADLOC);
  }

  while (nosquedamos == 1) {
    
    if (cx->buffer_tx == NULL) nosquedamos = 0;
    else {
      cx->estado = ZZ;
      desbloquear_acceso(&KERNEL->semaforo);
      bloquea_llamada(&(cx->barC));
      bloquear_acceso(&KERNEL->semaforo);
      if (cx->estado == EXNET) {
	desbloquear_acceso(&KERNEL->semaforo);
	ltm_exit_kernel((void**) & KERNEL);
	return EXNET;
      }
      else if (cx->estado == DR)  {
	cx->flujo_salida = CLOSE_SEND;
	cx->flujo_entrada = CLOSE_SEND;
	desbloquear_acceso(&KERNEL->semaforo);
	ltm_exit_kernel((void**) & KERNEL);
	return EXDISC;
      }
    }
    // comprobar si llego disconnect
  }
  
  desbloquear_acceso(&KERNEL->semaforo);
  ltm_exit_kernel((void**) & KERNEL);
    
  return EXOK;
}
  
////////////////////////////////////////////////////////////
//                                                        //  
//                      t_disconnect                      //                 
//                                                        //
////////////////////////////////////////////////////////////

int t_disconnect(int id) {
  int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
  conexion_t *cx;
  paquete_tcp *pkt;
  eventos *peventos=NULL;
  
  if (er < 0) 
    return EXKERNEL;

  
  bloquear_acceso(&KERNEL->semaforo);
  cx = busca_conex(id);
  
  if (cx == NULL) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return(EXBADLOC);
  }
   
  //fprintf(stderr,"el cx->flujo_entrada = %i\n",cx->flujo_entrada);
  if (cx->flujo_entrada == CLOSE_SEND) {
    //  fprintf(stderr,"t_disconnect ha eliminado la cx con puerto local %i (if flujo_entrada == CLOSE_SEND!)\n", cx->puerto_local);  
    elimina_conexion(cx);
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXOK;
  }
  
  pkt = (paquete_tcp *)alloc_kernel_mem(sizeof(paquete_tcp));

  pkt->cabecera.tipo_pkt = DR;
  pkt->cabecera.puerto_origen = cx->puerto_local;
  pkt->cabecera.puerto_destino = cx->puerto_remoto;
  pkt->cabecera.num_secuencia = cx->num_secuencia;
  pkt->cabecera.cantidad_datos = 0;
  pkt->cabecera.mapa_bits = 0;
  pkt->cabecera.posic_ventana = 0;
  pkt->cabecera.ultimo = CLOSE_SEND;
  
  enviar_tpdu(cx->ip_remota,pkt,sizeof(cabecera_t));
  peventos = crea_evento(KERNEL,cx, pkt->cabecera.num_secuencia,DR,NUM_RTX);
  
  while (cx->estado != DC) {
    cx->estado = ZZ;
    desbloquear_acceso(&KERNEL->semaforo);
    bloquea_llamada(&(cx->barC));
    bloquear_acceso(&KERNEL->semaforo);
    if (cx->estado == EXNET) {
      desbloquear_acceso(&KERNEL->semaforo);
      ltm_exit_kernel((void**) & KERNEL);
      return EXNET;
    }
  }
  while (cx->buffer_tx != NULL) cx->buffer_tx = elimina_buffer(cx->buffer_tx,TX);
  while (cx->buffer_rx != NULL) cx->buffer_rx = elimina_buffer(cx->buffer_rx,RX);
  // fprintf(stderr,"t_disconnect ha eliminado la cx con puerto local %i\n", cx->puerto_local);  
  elimina_conexion(cx);

  desbloquear_acceso(&KERNEL->semaforo);
  ltm_exit_kernel((void**) & KERNEL);

  return EXOK;
}

////////////////////////////////////////////////////////////
//                                                        //  
//                      t_send                            //                 
//                                                        //
////////////////////////////////////////////////////////////
size_t t_send(int id, const void *datos, size_t longitud, int8_t *flags) {
  int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
  conexion_t *cx=KERNEL->cx;  
  size_t datos_pal_buffer=longitud,longitud_enviada=longitud;  
  lista_paquetes *lista_pkts;
  
  if (er < 0)
    return EXKERNEL;
  
  // fprintf(stderr,"Entramos en el t_send con longitud de datos = %i\n",(int)longitud); 
  bloquear_acceso(&KERNEL->semaforo);
  
  
  cx = busca_conex(id);
  //fprintf(stderr,"con id de conexion %i\n",cx->puerto_local);
  if (cx == NULL) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXBADLOC;
  }
  if ((cx->flujo_salida == CLOSE_SEND) && (cx->buffer_tx == NULL)) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXDISC;
  }
  
  if (cx->flujo_salida == CLOSE) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return(EXCLOSE);
  }
  /* 
     if ((int)longitud > MAX_DATOS) {
     //printf("Nos vamos del send porque los datos a enviar son mayores que el MAX_DATOS\n");
     desbloquear_acceso(&KERNEL->semaforo);
     ltm_exit_kernel((void**) & KERNEL);
     return(EXMAXDATA);
    
     }  // en teoria hacemos segmentacion asi que esto comentado.
  */
  
  // cx apuntara a la conexion que estamos usando o si devuelve null nos salimos.
  //printf("LA POSICION DE RX (EN SEND) ES %i\n",cx->posic_ventana_rx);
 
  lista_pkts = cx->buffer_tx; // pillamos la lista de paquetes de la conexion
  
  if (((int)longitud < 0) || (datos < 0) || (flags < 0)) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXINVA;
  }
  lista_pkts = cx->buffer_tx;
  while (longitud > 0) {   // aqui entramos en caso de que aun haya pkts en el buffer por transmitir
    anhade_buffer_tx(cx,datos,longitud, &datos_pal_buffer, flags);  // actualiza el puntero a los datos tambien
    lista_pkts = cx->buffer_tx;
    while (lista_pkts != NULL) {
      enventana_pkt(cx, lista_pkts, &longitud);
      lista_pkts = lista_pkts->sig;
    }
    if  (longitud > 0) {
      cx->estado = ZZ;
      desbloquear_acceso(&KERNEL->semaforo);
      bloquea_llamada(&(cx->barC));
      bloquear_acceso(&KERNEL->semaforo);
      if (cx->estado == EXNET) {
	desbloquear_acceso(&KERNEL->semaforo);
	ltm_exit_kernel((void**) & KERNEL);
	return EXNET;
      }
      if (cx->estado == DR) {
	while (cx->buffer_tx != NULL) cx->buffer_tx = elimina_buffer(cx->buffer_tx,TX);
	desbloquear_acceso(&KERNEL->semaforo);
	ltm_exit_kernel((void**) & KERNEL);
	return longitud_enviada-longitud; // devolvemos los bytes enviados hasta el momento
      }
    }
  }
  //fprintf(stderr,"Salimos del t_send\n");
  desbloquear_acceso(&KERNEL->semaforo);
  ltm_exit_kernel((void**) & KERNEL);
  return longitud_enviada;
}

////////////////////////////////////////////////////////////
//                                                        //  
//                      t_receive                         //                 
//                                                        //
////////////////////////////////////////////////////////////

size_t t_receive(int id, void *datos, size_t longitud, int8_t *flags) {
  int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
  conexion_t *cx=KERNEL->cx;
  lista_paquetes *lista_pkts;
  int cantidad_recibida=0, cantidad_temporal=0,long_aux;

  if (er < 0)
    return EXKERNEL;
  bloquear_acceso(&KERNEL->semaforo);
  //  fprintf(stderr,"Entramos en el t_receive ");
  cx = busca_conex(id);
  //fprintf(stderr,"con id de conexion %i.\n",cx->puerto_local);  
  if (cx == NULL) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return(EXBADLOC);
  }
  if ((cx->flujo_entrada == CLOSE_SEND) && (cx->buffer_rx == NULL)) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return EXDISC;
  }
  if ((cx->flujo_entrada == CLOSE) && (cx->buffer_rx == NULL)) {
    desbloquear_acceso(&KERNEL->semaforo);
    ltm_exit_kernel((void**) & KERNEL);
    return(EXCLOSE);
  }

  if (cx->estado == DR) *flags |= CLOSE_SEND;
  
  long_aux = (int)longitud;  
  lista_pkts = cx->buffer_rx;
 
  //  long_aux es la longitud que espera el receive y se va decrementando segun vamos procesando el buffer de rx ordenadamente
  //  cuando llegue a 0 es que ya se han procesado todos los datos esperados por el receive   
 
  cantidad_temporal = cuenta_datos(lista_pkts,long_aux,cx);     // he cambiado (int)longitud por long_aux
    
  while (long_aux > 0) {
    // hay que bloquearse hasta que nos lleguen los datos suficientes
    // printf("Nos quedamos dormidos esperando que en la lista haya suficientes datos\n");
    if ((cx->flujo_entrada == CLOSE) || (cx->flujo_entrada == CLOSE_SEND)) {
      cantidad_temporal = cuenta_datos(lista_pkts,long_aux,cx);
      // fprintf(stderr,"activamos close! en comienzo while (t_recv)\n");
      *flags |= CLOSE;
      while (cantidad_temporal > 0) lista_pkts=copia_datos(cx,&datos,&cantidad_recibida,&cantidad_temporal,&long_aux);
      if (cx->flujo_entrada == CLOSE_SEND) while (cx->buffer_rx != NULL) cx->buffer_rx = elimina_buffer(cx->buffer_rx,RX);
      desbloquear_acceso(&KERNEL->semaforo);
      ltm_exit_kernel((void**) & KERNEL);
      return cantidad_recibida;
    }  
  
    cx->estado = ZZ;
    desbloquear_acceso(&KERNEL->semaforo);
    bloquea_llamada(&(cx->barC)); 
    bloquear_acceso(&KERNEL->semaforo);
    lista_pkts = cx->buffer_rx;  
      
    cantidad_temporal = cuenta_datos(lista_pkts,long_aux,cx);  // han llegado cantidad_temporal datos ordenados
      
    if (cx->estado == EXNET) {
      desbloquear_acceso(&KERNEL->semaforo);
      ltm_exit_kernel((void**) & KERNEL);
      return EXNET;
    }
    if (cx->estado == DR) {
      cx->flujo_entrada = CLOSE_SEND;
      *flags |= CLOSE_SEND;
      while (cantidad_temporal > 0) lista_pkts=copia_datos(cx,&datos,&cantidad_recibida,&cantidad_temporal,&long_aux);
      while (cx->buffer_rx != NULL) cx->buffer_rx = elimina_buffer(cx->buffer_rx,RX);
      //fprintf(stderr,"Ha llegado un DR al receive y quedaban aun %i datos esperados.\n", long_aux);  
      desbloquear_acceso(&KERNEL->semaforo);
      ltm_exit_kernel((void**) & KERNEL);
      return cantidad_recibida;
    }
    
    while (cantidad_temporal > 0) {
      if (cx->flujo_entrada == CLOSE) {
	//	fprintf(stderr,"activamos close! en después de despertar (t_recv)\n");
	*flags |= CLOSE;   
      }
      lista_pkts=copia_datos(cx,&datos,&cantidad_recibida,&cantidad_temporal,&long_aux);
    }
    lista_pkts = cx->buffer_rx;
  }  
  
  lista_pkts = cx->buffer_rx;
  
  // fprintf(stderr,"Salimos del t_receive\n");
  desbloquear_acceso(&KERNEL->semaforo);
  ltm_exit_kernel((void**) & KERNEL);
  return cantidad_recibida;
}
