#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comunicacion.h"

#include "interfaz.h"
#include "ltmtypes.h"
#include "CONFP.h"
#define TROZO 1024

unsigned long int tamanho_fichero(FILE *fich){
	fseek(fich,0,SEEK_END);
	return ftell(fich);
}

int main(int argc, char **argv) {
    t_direccion tsap_destino, tsap_origen;
    size_t datos_encolados;
    int cx,i=0,datos_recibidos=0,datos_enviados=0,recibidos_aux;
    int cx2,cx3;
    int8_t finlectura=0,finescritura=0;
    char *datos,*datos2;
    int8_t flags=0,flags2=0;
    FILE *fich=NULL,*fich2=NULL;
    unsigned long int tamanho_fich;
    
    memset(&tsap_origen, 0, sizeof (tsap_origen));
    memset(&tsap_destino, 0, sizeof (tsap_destino));


    if (strcmp(argv[1],"-cl") == 0) {  // cl = connectlisten
           
      inet_pton(AF_INET6, argv[2], &tsap_destino.ip);
      tsap_destino.puerto = 0;  
      cx = t_connect(&tsap_destino, &tsap_origen);
        
      //cx2 = t_listen(&tsap_origen,&tsap_destino)  
      printf("El id de la cx es: %i\n", cx);
      
      
      if ((fich = fopen("kurose.pdf","rb")) == NULL) {
          fprintf(stderr,"Error al abrir el fichero\n");
          exit(-1);
      }  
        
      if ((fich2 = fopen("kuroserecv.pdf","w+")) == NULL) {
          fprintf(stderr,"Error al abrir el fichero\n");
          exit(-1);
      }  
        
        
      tamanho_fich = tamanho_fichero(fich);  
      rewind(fich);
      fprintf(stderr,"El tama–o del fichero es: %i bytes.\n",tamanho_fich);  
      while ((finlectura == 0) || (finescritura == 0)) {
          if (!finlectura) {  // si no ha terminado de leer entonces continua leyendo
              datos = malloc(TROZO*50);
              fread(datos,TROZO*50,1,fich);
              tamanho_fich -= TROZO*50;
              datos_encolados = t_send(cx,datos,TROZO*50,&flags);
              datos_enviados += (int)datos_encolados;
              if (tamanho_fich < TROZO*50) {
                  datos = malloc(tamanho_fich);
                  flags |= CLOSE; // indicamos que no vamos enviar mas
                  datos_encolados = t_send(cx,datos,tamanho_fich,&flags);
                  datos_enviados += (int)datos_encolados;
                  finlectura = 1;
                  cx2 = t_flush(cx);
                
                    if (cx2 == EXBADLOC)
                          fprintf(stderr,"No se ha encontrado la conexion referida en t_flush. (EXBADLOC)\n");
             
                    if (cx2 == EXDISC)
                          fprintf(stderr,"Ha llegado un disconnect.(EXDISC)\n");
             
                    if (cx2 == EXNET)
                          fprintf(stderr,"EXNEEET!!. (EXNET)\n");
             
                    if (cx2 == EXOK)
                          fprintf(stderr,"Flusheado correctamente! :). (EXOK)\n");
             
                  
              }
          }
          if (!finescritura) { // si no ha terminado de escribir pues sigue haciendolo
              datos2 = malloc(TROZO*50);
              //fprintf(stderr,"AYQJODERSE\n");
              recibidos_aux = t_receive(cx,datos,TROZO*50,&flags2);
              //fprintf(stderr,"AYQJODERSE %i\n", recibidos_aux);
              fwrite(datos,recibidos_aux,1,fich2);
              //fprintf(stderr,"AYQJODERSE3\n");
              datos_recibidos += recibidos_aux;
              i++;
              if ((i % 10) == 1) fprintf(stderr,"Recibiendo.... %i bytes\n", datos_recibidos);
              if ((flags2 &= CLOSE) == 1) {
                  finescritura = 1;
                  cx3 = t_disconnect(cx);
                 
                      if (cx3 == EXBADLOC)
                          fprintf(stderr,"No se ha encontrado la conexion referida en t_disconnect. (EXBADLOC)\n");
                  
                      if (cx3 == EXNET)
                          fprintf(stderr,"EXNEEET!!. (EXNET)\n");
                  
                      if (cx3 == EXOK)
                          fprintf(stderr,"Desconectado correctamente :). (EXOK)\n");
                  
                 
         
              }
          }
      }
      
      fprintf(stderr,"Se han enviado %i bytes.\n",datos_enviados);
      fprintf(stderr,"Se han recibido %i bytes.\n", datos_recibidos); 
      fprintf(stderr,"Bueno, si todo ha salido como lo esperado, deberiamos estar aprobados :)\n");
  
      //tsap_destino.puerto = 22;
      //t_connect(&tsap_destino, &tsap_origen);
      //tsap_destino.puerto = 20;
      //t_connect(&tsap_destino, &tsap_origen);
    } else if (strcmp(argv[1],"-lc") == 0) { // lc = listenconnect

      tsap_origen.puerto = 34;     
      inet_pton(AF_INET6, "::", &tsap_destino.ip);
      cx = t_listen(&tsap_origen, &tsap_destino);
      
        if ((fich = fopen("kurose.pdf","rb")) == NULL) {
            fprintf(stderr,"Error al abrir el fichero\n");
            exit(-1);
        }  
        
        if ((fich2 = fopen("kuroserecv.pdf","w+")) == NULL) {
            fprintf(stderr,"Error al abrir el fichero\n");
            exit(-1);
        }  
        
        tamanho_fich = tamanho_fichero(fich);  
        rewind(fich);
        fprintf(stderr,"El tama–o del fichero es: %i bytes.\n",tamanho_fich);
        while ((finlectura == 0) || (finescritura == 0)) {
            if (!finlectura) {  // si no ha terminado de leer entonces continua leyendo
                datos = malloc(TROZO*50);
                fread(datos,TROZO*50,1,fich);
                tamanho_fich -= TROZO*50;
                datos_encolados = t_send(cx,datos,TROZO*50,&flags);
                datos_enviados += (int)datos_encolados;
                if (tamanho_fich < TROZO*50) {
                    datos = malloc(tamanho_fich);
                    flags |= CLOSE; // indicamos que no vamos enviar mas
                    datos_encolados = t_send(cx,datos,tamanho_fich,&flags);
                    datos_enviados += (int)datos_encolados;
                    finlectura = 1;
                    cx2 = t_flush(cx);
                    if (cx2 == EXBADLOC)
                        fprintf(stderr,"No se ha encontrado la conexion referida en t_flush. (EXBADLOC)\n");
                    
                    if (cx2 == EXDISC)
                        fprintf(stderr,"Ha llegado un disconnect.(EXDISC)\n");
                    
                    if (cx2 == EXNET)
                        fprintf(stderr,"EXNEEET!!. (EXNET)\n");
                    
                    if (cx2 == EXOK)
                        fprintf(stderr,"Flusheado correctamente! :). (EXOK)\n");
                }
            }
            if (!finescritura) { // si no ha terminado de escribir pues sigue haciendolo
                datos2 = malloc(TROZO*50);
                //fprintf(stderr,"AYQJODERSE\n");
                recibidos_aux = t_receive(cx,datos,TROZO*50,&flags2);
                //fprintf(stderr,"AYQJODERSE %i\n", recibidos_aux);
                fwrite(datos,recibidos_aux,1,fich2);
                //fprintf(stderr,"AYQJODERSE3\n");
                datos_recibidos += recibidos_aux;
                i++;
                if ((i % 10) == 1) fprintf(stderr,"Recibiendo.... %i bytes\n", datos_recibidos);
                if ((flags2 &= CLOSE) == 1) {
                    finescritura = 1;
                    cx3 = t_disconnect(cx);
                    if (cx3 == EXBADLOC)
                        fprintf(stderr,"No se ha encontrado la conexion referida en t_disconnect. (EXBADLOC)\n");
                    
                    if (cx3 == EXNET)
                        fprintf(stderr,"EXNEEET!!. (EXNET)\n");
                    
                    if (cx3 == EXOK)
                        fprintf(stderr,"Desconectado correctamente :). (EXOK)\n");
                    
                }
            }
        }  
        fprintf(stderr,"Se han enviado %i bytes.\n",datos_enviados);
        fprintf(stderr,"Se han recibido %i bytes.\n", datos_recibidos);
        fprintf(stderr,"Bueno, si todo ha salido como lo esperado, deberiamos estar aprobados :)\n");
    }
    else fprintf(stderr,"Error en los par‡metros\n");

    return 0;
}
