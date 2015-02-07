#ifndef _CONFP_H
#define _CONFP_H

#include "ltmtypes.h"
#include "comunicacion.h"
#include "ltmipcs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <stdbool.h>

#define CR 100  // conexion request
#define CC 101  // conexion confirmed
#define CN 102 // conexion negada
#define DT 103  // tx datos
#define ACK 104  // ack
#define DR 105  // disconnection request
#define DC 106 // disconnection confirmed
#define TX 1   // indica transmision
#define RX 2  // indica recepcion
#define ZZ 99 // indica dormido
#define MAX_CONEX 5 // numero maximo de conexiones
#define LISTEN 25
#define CONNECT 26
#define NUM_RTX 40 // numero maximo de retransmisiones
#define TAMANHO_VENTANA 15
#define TAM_DATOS 1024 // cantidad de datos en el paquete
#define TAM_CAB sizeof(struct _cabecera_pkt) // tamaño de la cabecera del paquete
#define TAM_PKT TAM_DATOS+TAM_CAB

#ifdef	__cplusplus
extern "C" {
#endif

 
  extern const struct timeval timeout;
  //extern int32_t num_retx_pkt;
  //extern float media_num_retx_pkt; 
  extern int32_t num_sec_inicial;
  typedef struct _conexion {
    pid_t ap_pid;
    barrera_t barC;
    int32_t posic_ventana_tx, posic_ventana_rx;
    int8_t estado; // CR=no conectado, CC=conectado, CN=conexion rechazada, ZZ = dormida, DR = disconection request, DC = disconection confirmed
    struct in6_addr ip_local, ip_remota;
    int16_t puerto_local,puerto_remoto;
    int num_secuencia;  
    struct _ventana_rtx *ventana_tx;  // array de punteros a la estructura ventana_rtx 
    struct _ventana_rtx *ventana_rx;  
    struct _lista_paquetes *buffer_tx; // contendra una lista con los paquetes que habrá que ir metiendo en la ventana de tx. (ventana_rtx)
    struct _lista_paquetes *buffer_rx;
    int8_t flujo_salida,flujo_entrada;
    struct _conexion *sig;
  } conexion_t;
  
  typedef struct _evento {
    struct timeval vence;
    int rtx;
    int32_t num_secuencia;
    conexion_t *cx;
    int tipo_evento;
    struct _evento *sig;
  } eventos;
  
  typedef struct _cabecera_pkt {
    char tipo_pkt;   // CC, CR, ACK, DR, CN, DATOS...
    int16_t puerto_origen;
    int16_t puerto_destino;
    int32_t num_secuencia;
    int16_t cantidad_datos;
    uint32_t mapa_bits;
    int32_t posic_ventana;
    int8_t ultimo;
  } cabecera_t;
  
  typedef struct _paquete_tcp {
    cabecera_t cabecera; 
    char datos[TAM_DATOS];
  } paquete_tcp;
    
  typedef struct _ventana_rtx {
    paquete_tcp *pkt; // puntero al pakete pendiente de asentimiento
    eventos *pevento; // puntero al evento de la rtx de ese pakete.
  } ventana_rtx;  // cuando lo definamos como lista_paquetes usamos el campo sig, y cuando lo usamos como ventana_rtx solo lo usamos como punteros a los pkts
  

  
  typedef struct _kernel_shm {
    interfaz_red_t i_red; // este campo debe ser el primero de esta estructura
    
    pid_t kernel_pid;
    int num_conexiones;
    semaforo_t semaforo;
    int8_t num_pkts_buf_tx;
    int8_t num_pkts_buf_rx;
    conexion_t *cx;
    eventos *peventos;
  } kernel_shm_t;
  
  typedef struct  _lista_paquetes {
    paquete_tcp pkt;
    struct _lista_paquetes *sig;
  } lista_paquetes;
  
 
  
  /////////////////////////////////////////////////////////////////////
  //  A partir de aquí podéis poner los prototipos de las funciones  //
  //      que después implementaréis en el FUNCP.C                   //
  /////////////////////////////////////////////////////////////////////
  
  
  
  conexion_t *comprobar_conexion_connect(kernel_shm_t *KERNEL,const t_direccion *tsap_destino,const t_direccion *tsap_origen);
  conexion_t *comprobar_conexion_listen(kernel_shm_t *KERNEL,const t_direccion *tsap_escucha,const t_direccion *tsap_remota);
  conexion_t *comprobar_conexion_bprincipal(kernel_shm_t *KERNEL,const t_direccion *tsap_destino,const t_direccion *tsap_origen);
  conexion_t *crea_conexion(kernel_shm_t *KERNEL,const t_direccion *tsap_destino,const t_direccion *tsap_origen); 
  conexion_t *busca_conex(int id);  
  conexion_t *busca_conex_zero(kernel_shm_t *KERNEL);
  conexion_t *buscar_conexion(kernel_shm_t *KERNEL);
  int is_address_cero(const struct in6_addr *addr_to_test);
  int calcula_shortest(eventos *peventos);
  eventos *crea_evento(kernel_shm_t *KERNEL, conexion_t *cx, int32_t num_secuencia,int tipo_evento, int rtx);
  void elimina_evento(kernel_shm_t *KERNEL,conexion_t *cx,int32_t num_secuencia);
  conexion_t *rtx_CR();
  int rtx_DT();
  int rtx_DR();
  int rtx_CC();
  int busca_puerto(int16_t puerto);
  int busca_puerto_remoto(conexion_t *cx, int16_t puerto); 
  void elimina_conexion(conexion_t *cx);
  eventos *busca_evento(eventos *peventos, int tipo_evento);
  void anhadir_pkt(int tipo_pkt,conexion_t *cx,int num_secuencia,const void *datos, size_t tamanho_datos,int8_t RXTX); 
  int8_t comprueba_ventana(conexion_t *cx,int32_t num_secuencia, int8_t RXTX);
  uint32_t mapea_ventana_rx(conexion_t *cx);
  // int8_t procesar_paquetes(size_t longitud, conexion_t *cx, const void *datos, lista_paquetes *lista_pkts, size_t tamanho_datos);
  int8_t procesar_paquetes_rx(paquete_tcp *pkt, conexion_t *cx);
  cabecera_t construye_ack(conexion_t *cx, int num_secuencia);
  //void anhade_ack(cabecera_t cabecera, conexion_t *cx);
  int cuenta_datos(lista_paquetes *lista_pkts, int longitud,conexion_t *cx);
  void anhade_buffer_rx(conexion_t *cx);
  lista_paquetes *copia_datos(conexion_t *cx,void **datos, int *cantidad_recibida, int *cantidad_temporal, int *long_aux);
  void enventana_pkt(conexion_t *cx,lista_paquetes *lista_pkts,size_t *longitud);
  void anhade_buffer_tx(conexion_t *cx, const void *datos, size_t longitud,size_t *datos_pal_buffer, int8_t *flags);
  void aNULLear_ventana(conexion_t *cx,int32_t posic_ventana);
  void comprobar_mapa_bits(conexion_t *cx,uint32_t mapa_bits);
  //  int termineitor(lista_paquetes *lista_pkts);
  // int termineitor2(conexion_t *cx);  
  lista_paquetes *elimina_buffer(lista_paquetes *lista_pkts, int8_t RXTX);
  void ordena_buffer(conexion_t *cx, int RXTX);
  lista_paquetes *elimina_buffer_tx(lista_paquetes *lista_pkts, int32_t num_secuencia);
  lista_paquetes *busca_pkt_buf(lista_paquetes *buffer_tx, int32_t num_secuencia);
  int8_t comprueba_toda_ventana(conexion_t *cx);
#ifdef	__cplusplus
}
#endif

#endif /* _CONFP_H */
