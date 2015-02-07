#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "ltmdaemon.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/time.h>

///////////////////////////////////////////////////////////////////////////

void bucle_principal(void) {
  bool daemon_stop = false;
  paquete_tcp *pkt=(paquete_tcp *)alloc_kernel_mem(sizeof(paquete_tcp));
  eventos *peventos=(eventos *)alloc_kernel_mem(sizeof(eventos));
  int8_t flag_procesa_pkt;
  int rec;
  int shortest = -1;
  struct in6_addr ip_remota, ip_local;  // ip_remota = ip_origen
  // char ip6charbuf[INET6_ADDRSTRLEN], ip6charbuf2[INET6_ADDRSTRLEN];
  conexion_t *cx=KERNEL->cx;
  t_direccion tsap_origen, tsap_destino;
  lista_paquetes *lista_pkts;
  struct timeval quehoraes;  


  obtener_IP_local((const char *)KERNEL,&ip_local);
  inicia_semaforo(&KERNEL->semaforo);
  KERNEL->num_pkts_buf_tx = NUM_BUF_PKTS;
  KERNEL->num_pkts_buf_rx = NUM_BUF_PKTS;
  do {
    desbloquear_acceso(&KERNEL->semaforo);
    peventos = KERNEL->peventos;
    shortest = calcula_shortest(peventos);
    // fprintf(stderr,"El shortest es: %i\n", shortest);
    //printf("EL SEMAFORO ANTES DE DESBLOQUEAR ESTA A %i\n", KERNEL->semaforo);

    switch (ltm_wait4event(shortest)) {

    case TIME_OUT:
      bloquear_acceso(&KERNEL->semaforo);
      //  fprintf(stderr, "El protocolo despierta por TMOUT (hora %ld)\n", time(0));
      peventos = KERNEL->peventos;
      if (peventos == NULL) break; // salimos de time_out
      //num_retx_pkt++;
      switch(peventos->tipo_evento) {

      case CR:
	if ((cx = rtx_CR()) != NULL) {
	  if (cx->estado == ZZ) {
	    cx->estado = EXNET;
	    despierta_conexion(&(cx->barC));
	  }
	}
	break;

      case DT:
	if (rtx_DT() == EXNET) {
	  //  fprintf(stderr,"EXNET DT! ");  
	  cx = comprobar_conexion_listen(KERNEL,&tsap_destino,&tsap_origen);
	  if (cx != NULL) {
	    fprintf(stderr,"y encontro la conex!\n");  
	    if (cx->estado == ZZ) {
	      cx->estado = EXNET;
	      despierta_conexion(&(cx->barC));
	    }
	  }
	}
	break;
      case DR:
	if (rtx_DR() == EXNET) {
	  // fprintf(stderr,"EXNET DR! ");  
	  cx = comprobar_conexion_bprincipal(KERNEL,&tsap_destino,&tsap_origen);
	  if (cx != NULL) {
	    fprintf(stderr,"y encontro la conex!\n");  
	    if (cx->estado == ZZ) {
	      cx->estado = EXNET;
	      despierta_conexion(&(cx->barC));
	    }
	  }
	}
	break;
      case CC:
	if (rtx_CC() == EXNET) {
	  cx = comprobar_conexion_bprincipal(KERNEL,&tsap_destino,&tsap_origen); 
	  if (cx != NULL) {
	    if (cx->estado == ZZ) {
	      cx->estado = EXNET;
	      despierta_conexion(&(cx->barC));
	    }
	  }
	}
	break;
      }

       
      break; // case time_out

    case INTERRUP:
      bloquear_acceso(&KERNEL->semaforo);
      // fprintf(stderr,"interrumpt!");
      break;

    case PAQUETE:
      bloquear_acceso(&KERNEL->semaforo);
      // fprintf(stderr,"\n Se ha recibido un PAQUETE ");
      rec = recibir_tpdu(pkt, TAM_PKT, &ip_remota);
     
      if (rec >= 0) {
	memset(&tsap_origen, 0, sizeof (t_direccion));
	memset(&tsap_destino, 0, sizeof (t_direccion));
	memcpy(&tsap_origen.ip,&ip_remota,sizeof(struct in6_addr));
	memcpy(&tsap_origen.puerto,&pkt->cabecera.puerto_origen,sizeof(int16_t));
	memcpy(&tsap_destino.ip,&ip_local,sizeof(struct in6_addr));
	memcpy(&tsap_destino.puerto,&pkt->cabecera.puerto_destino,sizeof(int16_t));

	switch(pkt->cabecera.tipo_pkt) {

	case CR:
	  // fprintf(stderr,"Ha llegado un CR desde %s!\n", inet_ntop(AF_INET6,&ip_remota,ip6charbuf,INET6_ADDRSTRLEN));


	  cx = busca_conex_zero(KERNEL);
	  if (cx != NULL) {  // hay una conexion "::" en la lista
	    // fprintf(stderr,"Hay una IP :: en la lista\n");
	  
	    if ((cx->puerto_remoto != 0) && (cx->puerto_remoto != tsap_origen.puerto)) { // no coincide el puerto_remoto
	      pkt->cabecera.tipo_pkt = CN;
	      pkt->cabecera.puerto_destino = tsap_origen.puerto; // el puerto destino a donde envio el paquete es el puerto que viene en el tsap_origen
	      enviar_tpdu(ip_remota,pkt,sizeof(cabecera_t));
	      if (cx->estado == ZZ) {
		cx->estado = CN;
		despierta_conexion(&(cx->barC));
	      }
	      break;
	    }
	    else cx->puerto_remoto = tsap_origen.puerto; // cx->puerto_remoto es 0 o coincide el cx->puerto_remoto con el tsap_origen
	   
	    if ((cx->puerto_local != 0) && (cx->puerto_local != tsap_destino.puerto)) { // no coincide puerto_local
	      pkt->cabecera.tipo_pkt = CN;
	      pkt->cabecera.puerto_destino = tsap_origen.puerto; // el puerto destino a donde envio el paquete es el puerto que viene en el tsap_origen
	      enviar_tpdu(ip_remota,pkt,sizeof(cabecera_t));
	      if (cx->estado == ZZ) {
		cx->estado = CN;
		despierta_conexion(&(cx->barC));
	      }
	      break;
	    }
	    else if (cx->puerto_local == 0) cx->puerto_local = tsap_destino.puerto;  // cx->puerto_local es 0 o coincide el cx->puero_local con el tsap_destino   
       
	  }
	  else {  // no hay una conexion "::" en la lista
	    //  fprintf(stderr,"No hay una IP :: en la lista\n");  
	    cx = comprobar_conexion_listen(KERNEL,&tsap_destino,&tsap_origen);
	    //	    fprintf(stderr,"Ha encontrado la conexion con el puerto_local %i\n", cx->puerto_local);  
	    if ( cx != NULL) {
	      
	      if (cx->puerto_local != tsap_destino.puerto) { // no coinciden IP y puertos, se rechaza ...
		   
		pkt->cabecera.tipo_pkt = CN;
		pkt->cabecera.puerto_destino = tsap_origen.puerto; // el puerto destino a donde envio el paquete es el puerto que viene en el tsap_origen
		enviar_tpdu(ip_remota,pkt,sizeof(cabecera_t));
		break;
	      }
	    }
	    else {  // cx = NULL
	      pkt->cabecera.tipo_pkt = CN;
	      pkt->cabecera.puerto_destino = tsap_origen.puerto; // el puerto destino a donde envio el paquete es el puerto que viene en el tsap_origen
	      enviar_tpdu(ip_remota,pkt,sizeof(cabecera_t));
	      break;
	    }
	  }

	  cx->ip_remota = ip_remota;
	  cx->ip_local = ip_local;
	  cx->puerto_remoto = tsap_origen.puerto;
	  cx->puerto_local = tsap_destino.puerto;

	  // printf("La conexion encontrada es: ip_local y puerto %s %d ",inet_ntop(AF_INET6,&cx->ip_local,ip6charbuf,INET6_ADDRSTRLEN),cx->puerto_local);
	  //printf("e ip_remota y puerto %s %d\n",inet_ntop(AF_INET6,&cx->ip_remota,ip6charbuf2,INET6_ADDRSTRLEN),cx->puerto_remoto);

	  pkt->cabecera.tipo_pkt = CC;
	  pkt->cabecera.puerto_destino = tsap_origen.puerto;  // el puerto destino a donde envio el paquete es el puerto que viene en el tsap_origen
	  pkt->cabecera.puerto_origen = tsap_destino.puerto;
	  enviar_tpdu(ip_remota,pkt,sizeof(cabecera_t));
	  if (cx->estado == ZZ) {
	    cx->estado = CC;
	    despierta_conexion(&(cx->barC));
          
	  }
  	  break;
	  
	  
	  
	case CC:
	  // printf("Ha llegado un CC desde %s!\n",inet_ntop(AF_INET6,&ip_remota,ip6charbuf,INET6_ADDRSTRLEN));
	  // printf("tsap_destino= %s %i y tsap_origen %s %i\n",inet_ntop(AF_INET6,&tsap_destino.ip,ip6charbuf,INET6_ADDRSTRLEN),tsap_destino.puerto,inet_ntop(AF_INET6,&tsap_origen.ip,ip6charbuf2,INET6_ADDRSTRLEN),tsap_origen.puerto);
	  cx = comprobar_conexion_bprincipal(KERNEL,&tsap_destino,&tsap_origen); 
	  if (cx == NULL) {
	    // fprintf(stderr,"No ha encontrado la conexion en la lista cuando recibimos un CC\n");   
	    break;   
	  }       
	  if (cx->puerto_remoto == 0) cx->puerto_remoto = tsap_origen.puerto;      
	  //printf("En el CC se va a eliminar un evento con la secuencia: %i\n",pkt->cabecera.num_secuencia);
	  elimina_evento(KERNEL,cx,pkt->cabecera.num_secuencia);
	  if (cx->estado == ZZ) {
	    cx->estado = CC;
	    despierta_conexion(&(cx->barC));
	    
	  }
	  break;
        	  
	  
	case CN:
	  peventos = busca_evento(KERNEL->peventos,CR);
	  //  fprintf(stderr,"La conexion ha sido rechazada\n");
	  if (peventos == NULL) {
	    // fprintf(stderr,"No ha encontrado la conexion en la lista cuando recibimos un CN\n");   
	    break;   
	  } 
	  cx = peventos->cx;     
	  //fprintf(stderr,"En el CN se va a eliminar un evento con la secuencia: %i\n",pkt->cabecera.num_secuencia);
	  
	  elimina_evento(KERNEL,cx,pkt->cabecera.num_secuencia);
	  if (cx->estado == ZZ) {
	    cx->estado = CN;
	    despierta_conexion(&(cx->barC));
	    
	  }
	  
	  break;
	  
	case DT:
	  // fprintf(stderr,"El num_secuencia que llega de DT es %i y su cantidad_datos %i \n ", pkt->cabecera.num_secuencia,pkt->cabecera.cantidad_datos);
	  cx = comprobar_conexion_listen(KERNEL,&tsap_destino,&tsap_origen);
	  if (cx == NULL) {
	    // fprintf(stderr,"No ha encontrado la conexion en la lista cuando recibimos un DT\n");   
	    break;   
	  }   
             
	 
	  //if (KERNEL->num_pkts_buf_rx <= 0) fprintf(stderr,"no hay sitio en el buffer RX!!!!\n");
	  if (KERNEL->num_pkts_buf_rx > 0) {
	    if (KERNEL->num_pkts_buf_rx == 1) {
	      //fprintf(stderr,"ES 1 el num_pkts_buf_rx %i %i!!!!!!\n",cx->posic_ventana_rx,pkt->cabecera.num_secuencia);
	      if (cx->posic_ventana_rx != pkt->cabecera.num_secuencia) break; // nos piramos porque esperamos el 1er
	    }                                                             // de la ventana
	    KERNEL->num_pkts_buf_rx--;
	    flag_procesa_pkt = procesar_paquetes_rx(pkt,cx);
	    while(cx->ventana_rx[cx->posic_ventana_rx % TAMANHO_VENTANA].pkt != NULL){
	      cx->ventana_rx[cx->posic_ventana_rx % TAMANHO_VENTANA].pkt = NULL;
	      cx->posic_ventana_rx++;
	    }
	    if (flag_procesa_pkt == -1) {
	      break;
	    }  // se pira si el paquete no entra en la ventana o esta fuera de la ventana.
	    else if (flag_procesa_pkt == -2) { // es un pkt que ya teniamos
	     
	      pkt->cabecera = construye_ack(cx,pkt->cabecera.num_secuencia);
	    }
	    else {  // llego un dato nuevo
	       
	      //  fprintf(stderr,"Ha llegado un dato nuevo (bprincipal case DT %i\n",cx->posic_ventana_rx);
	      pkt->cabecera = construye_ack(cx,pkt->cabecera.num_secuencia);
	                
	    }
	    //printf("Envio un ACK TAL QUE posic_ventana: %i\n", pkt->cabecera.posic_ventana);
	    lista_pkts = cx->buffer_rx;
	   
	    enviar_tpdu(cx->ip_remota,pkt,sizeof(cabecera_t));  // enviamos ACK
	 
	    if (cx->estado == ZZ) {
	      
	      cx->estado = CC; // si estamos transfiriendo datos es que estamos en CC
	      //fprintf(stderr,"cx->flujo_entrada en case DT bprincipal antes de despertar %i\n", cx->flujo_entrada);  
	      despierta_conexion(&(cx->barC));
	    }
	  }
	  break;
	case ACK:    
	  // fprintf(stderr,"Ha llegado un ACK!\n");
	  //printf("mapa_bits: %i y posic_ventana: %i\n",pkt->cabecera.mapa_bits, pkt->cabecera.posic_ventana);
	  
	  cx = comprobar_conexion_listen(KERNEL,&tsap_destino,&tsap_origen);
	  if (cx == NULL) {
	    //   fprintf(stderr,"No ha encontrado la conexion en la lista cuando recibimos un ACK\n");   
	    break;   
	  }  
  
	  //printf("cx->posic_ventana_tx %i pkt->posic_ventana %i\n", cx->posic_ventana_tx, pkt->cabecera.posic_ventana);
	  if (cx->posic_ventana_tx < pkt->cabecera.posic_ventana) {  // ventana_tx esta retrasada respecto rx
	    aNULLear_ventana(cx,pkt->cabecera.posic_ventana);
	    cx->posic_ventana_tx = pkt->cabecera.posic_ventana;
	    comprobar_mapa_bits(cx,pkt->cabecera.mapa_bits);
	  }
	  else if (cx->posic_ventana_tx == pkt->cabecera.posic_ventana) {
	    comprobar_mapa_bits(cx,pkt->cabecera.mapa_bits);
	  }
	  //fprintf(stderr,"cx->estado (ACK) CC = 101: %i\n",cx->estado);
	  if (cx->estado == ZZ) {
	    cx->estado = CC;
	    //fprintf(stderr,"Despertamos la conexion desde el ACK!!\n");
	    despierta_conexion(&(cx->barC));
	    //fprintf(stderr,"HEMOS DESPERTADO!!!\n");
	  }
	   
	  break;
	

	case DR:
	   
	  gettimeofday(&quehoraes,NULL);       
	  //  fprintf(stderr,"Ha llegado un DR a las ms:%u us:%u !!!\n", (uint)quehoraes.tv_sec,(uint)quehoraes.tv_usec);
	  cx = comprobar_conexion_bprincipal(KERNEL,&tsap_destino,&tsap_origen);
	  if (cx == NULL) {
	    //fprintf(stderr,"No ha encontrado la conexion en la lista cuando recibimos un DR\n");   
	    break;   
	  }
	  //fprintf(stderr,"La conexion encontrada tiene puerto_local %i\n",cx->puerto_local);       
	  //media_num_retx_pkt = num_retx_pkt/(float)cx->num_secuencia;
	  //fprintf(stderr,"El nº de RETX fue %i y LA MEDIA DE RETX POR PKT ES: %f (cx->num_sec %i)\n", num_retx_pkt,media_num_retx_pkt,cx->num_secuencia);
	  cx->flujo_salida = CLOSE_SEND;
	  cx->flujo_entrada = CLOSE_SEND;
	  pkt->cabecera.tipo_pkt = DC;
	  pkt->cabecera.puerto_origen = cx->puerto_local;
	  pkt->cabecera.puerto_destino = cx->puerto_remoto;
	  pkt->cabecera.num_secuencia = cx->num_secuencia;
	  pkt->cabecera.cantidad_datos = 0;
	  pkt->cabecera.mapa_bits = 0;
	  pkt->cabecera.posic_ventana = 0;
	  pkt->cabecera.ultimo = CLOSE_SEND;
	  enviar_tpdu(ip_remota,pkt,sizeof(cabecera_t));
	  if (cx->estado == ZZ) {
	    cx->estado = DR;
	    //fprintf(stderr,"Despertamos la conexion desde el DR!!\n");
	    despierta_conexion(&(cx->barC));
	    //fprintf(stderr,"HEMOS DESPERTADO!!!\n");
	  }
	  else {
	    cx->estado = DR;
	  }
	  break;
	  
	case DC:
	  //fprintf(stderr,"Ha llegado un DC!\n");
	  cx = comprobar_conexion_bprincipal(KERNEL,&tsap_destino,&tsap_origen);
	  if (cx == NULL) {
	    //fprintf(stderr,"No ha encontrado la conexion en la lista cuando recibimos un DC\n");   
	    break;   
	  }       
	  //media_num_retx_pkt = num_retx_pkt/(float)cx->num_secuencia;
	  //fprintf(stderr,"El nº de RETX fue %i y LA MEDIA DE RETX POR PKT ES: %f (cx->num_sec %i)\n", num_retx_pkt,media_num_retx_pkt,cx->num_secuencia);       
	  //if (cx == NULL) fprintf(stderr,"No falla el comprobar conex\n");
	  elimina_evento(KERNEL,cx,cx->num_secuencia);
	  if (cx->estado == ZZ) {
	    cx->estado = DC;
	    //fprintf(stderr,"Despertamos la conexion desde el DC!!\n");
	    despierta_conexion(&(cx->barC));
	    //fprintf(stderr,"HEMOS DESPERTADO!!!\n");
	  }
	  break;

	}
      
      }
      //despierta_conexion(&(cx->barC));
      
      break;
    }
    
    
  } while (daemon_stop != true);
}

extern char dir_proto[]; // "s.ltmdg" por defecto
extern char interfaz[]; // "eth0" por defecto
extern int LTM_PROTOCOL;
extern int pe;
extern int NUM_BUF_PKTS; // 100 por defecto

int procesa_argumentos(int argc, char ** argv) {
  
  //////////////////////////////////////////////////////////////
  // ltmd [-s dir_proto]   [-e perror]   [-p ltm_protocol]    //
  //    [-i interfaz] [-n NUM_BUF_PKTS]                       //
  //                                                          //
  //  perror debe ser un entero entre 0 y 100.                //
  //                                                          //
  //                                                          //
  //                                                          //
  //  char dir_proto[64]                                      //
  //  int pe                                                  //
  //  int LTM_PROTOCOL                                        //    
  //  char interfaz[8]                                        //
  //  int NUM_BUF_PKTS                                        //
  //////////////////////////////////////////////////////////////
  
  int opt;
  
  // valores por defecto
  LTM_PROTOCOL = 140+60;
  
  while ((opt = getopt(argc,argv,"s:e:p:i:n:")) != -1) {
    switch(opt) {
    case 's':
      strcpy(dir_proto,optarg);
      break;
    case 'e':
      pe = atoi(optarg);
      if ((pe<0) || (pe>100)) {
	printf("Perror tiene que estar entre 0 y 100.\n");
	exit(EXINVA);
      }
      break;
    case 'p':
      LTM_PROTOCOL = atoi(optarg);
      break;
    case 'i':
      strcpy(interfaz,optarg);
      break;
    case 'n':
      NUM_BUF_PKTS = atoi(optarg);
      break;
    default: 
      printf("Comando no valido.\n");
      exit(EXINVA);
      
    }
    
  }  
  
  printf("Dir_proto: %s\n perror: %i\n LTM_PROTO: %i\n interfaz: %s\n NUM_BUF_PKTS: %i.\n", dir_proto,pe,LTM_PROTOCOL,interfaz,NUM_BUF_PKTS); 
  
  return EXOK;
}


