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
    int cx,recibidos_aux=0,i,recibidos=0;
    unsigned long int tamanho_fich,datos_enviados=0;
    char *datos,*datos_aux;
    FILE *fich=NULL;
    int8_t flags=0;
    
    memset(&tsap_origen, 0, sizeof (tsap_origen));
    memset(&tsap_destino, 0, sizeof (tsap_destino));
    
    
    if (argc == 2) {
        
        inet_pton(AF_INET6, argv[1], &tsap_destino.ip);
        tsap_destino.puerto = 34;
        cx = t_connect(&tsap_destino, &tsap_origen);
        printf("El id de la cx es: %i\n", cx);
        if ((fich = fopen("test_v1.c","rb")) == NULL) {
            fprintf(stderr,"Error al abrir el fichero\n");
            exit(-1);
        }
	
        tamanho_fich = tamanho_fichero(fich);
        rewind(fich);
        datos = malloc(TROZO*50); // leemos TROZOs de 10240 bytes del fichero
        while (fread(datos,TROZO*50,1,fich) != 0) {
	  tamanho_fich -= TROZO*50;
	  datos_encolados = t_send(cx,datos,TROZO*50,&flags);
	  datos_enviados += (uint)datos_encolados;
	  datos = malloc(TROZO*50);
	  if (tamanho_fich < TROZO*50) break; // salimos del while si lo que falta por leer es menor que TROZO
        }
        datos = malloc(tamanho_fich);
        fread(datos,tamanho_fich,1,fich);
        datos_encolados = t_send(cx,datos,tamanho_fich,&flags);
        datos_enviados += (int)datos_encolados;
       
	
        fclose(fich);
        fprintf(stderr,"Se han enviado %li datos.\n", datos_enviados);
    } else {
        
        tsap_origen.puerto = 34;     
        inet_pton(AF_INET6, "::", &tsap_destino.ip);
        cx = t_listen(&tsap_origen, &tsap_destino);
        if ((fich = fopen("testv1recv.jpg","w+")) == NULL) {
            fprintf(stderr,"Error al abrir el fichero\n");
            exit(-1);
        }
        datos = malloc(170771);
	datos_aux = datos;
        for (i=0;i<5;i++) { 
	  recibidos_aux = t_receive(cx,datos,1024,&flags);
	  recibidos += recibidos_aux;
        }
	recibidos_aux = t_receive(cx,datos,257,&flags);
	recibidos += recibidos_aux;
        printf("En aplic se han recibido %u bytes\n", recibidos);
        printf("Procedemos a crear el fichero...\n");
        fwrite(datos_aux,5120+257,1,fich);
        fclose(fich);
        printf("Hecho!\n");
        
    }
    
    return 0;
}
