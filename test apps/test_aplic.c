/*
 * ⓒ Copyright Laboratorio de Redes 2011
 *
 * Programa demo de pre-prueba de LTM. Versión 0.1.
 *
 * El programa crea dos conexiones entre dos máquinas A y B. Los puertos origen
 * y destino son los especificados en la línea de comandos para la primera conexión y
 * los mismos incrementados en una unidad para la segunda. Pueden dejarse sin especificar
 * en las condiciones que establecen las llamadas a t_listen y t_connect.
 *
 * La máquina B se lanza en modo escucha (-l) y la máquina A en modo connect.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "interfaz.h"

static const char *str_errors[] = { "EXOK", "EXBADLOC", "EXCDUP", "EXNET",
		"EXKERNEL", "EXMAXDATA", "EXNOTSAP", "EXNODATA",
		"EXCLOSE", "EXINVA",  "EXMAXC", "EXUNDEF", "EXDISC" };

static const char *error_to_str(int error) {
	int index = -error;

	if (index >= 0 && index < sizeof(str_errors))
		return str_errors[index];
	else
		return "Unknown error";
}

static void usage(const char *progname) {
    fprintf(stderr, "Uso: %s [-l] [-d ip_dst] [-s port_orig] [-r port_dst]\n", progname);

    exit(-1);
}

static void show_endpoints(const t_direccion *origen, const t_direccion *destino) {
	char buf[INET6_ADDRSTRLEN];

    fprintf(stderr, "IP Origen: %s\tPuerto Origen: %d\n", inet_ntop(AF_INET6, &origen->ip, buf, sizeof(buf)), origen->puerto);
    fprintf(stderr, "IP Destino: %s\tPuerto Destino: %d\n", inet_ntop(AF_INET6, &destino->ip, buf, sizeof(buf)), destino->puerto);
}

static int do_receiver_part(t_direccion *dir_local, t_direccion *dir_remota) {
	int cx1, cx2;
	int8_t flags = 0;
	int bs = 200;
	int len1, len2;

	/* Crear dos conexiones, si podemos */
	cx1 = t_listen(dir_local, dir_remota);
	if (cx1 < 0)
		return cx1;

	show_endpoints(dir_local, dir_remota);

	dir_local->puerto += 1;
	dir_remota->puerto += 1;

	cx2 = t_listen(dir_local, dir_remota);
	if (cx2 < 0) {
	  //t_disconnect(cx1);
		return cx2;
	}

	do {
		char buf[8192];

		len1 = t_receive(cx1, buf, bs, &flags);
		if (len1 < 0 && len1 != EXCLOSE && len1 != EXDISC) {
		  //t_disconnect(cx1);
		  //		t_disconnect(cx2);

			fprintf(stderr, "CX1: ");

			return len1;
		}

		len2 = t_send(cx2, buf, len1, &flags);
		if (len2 < 0 && len2 != EXCLOSE && len2 != EXDISC) {
		  //t_disconnect(cx1);
		  //	t_disconnect(cx2);

			fprintf(stderr, "CX2: ");

			return len2;
		}

		bs = (bs + 256)%sizeof(buf);
	} while(len1 != EXCLOSE && len1 != EXDISC);

	return EXOK;
}

static char *init_noise(char *buf, size_t len) {
	int32_t *buf32 = (int32_t *)buf;
	int i;

	for (i = 0; i < len/4; i++) {
		buf32[i] = random();
	}

	return buf;
}

static char *gen_noise(char *buf, size_t len) {
	int32_t noise = random();
	int i;
	int32_t *buf32 = (int32_t *)buf;

	for (i = 0; i < len/4; i++) {
		buf32[i] ^= noise;
	}

	return buf;
}

static int do_sender_part(t_direccion *dir_local, const t_direccion *dir_remota, size_t len) {
	int cx1, cx2;
	int8_t flags = 0;
	int bs = 200;
	int len1, len2;
	char data[8192];
	t_direccion dir_remota2;

	/* Crear dos conexiones, si podemos */
	cx1 = t_connect(dir_remota, dir_local);
	if (cx1 < 0)
		return cx1;

	show_endpoints(dir_local, dir_remota);

	dir_remota2 = *dir_remota;
	dir_local->puerto += 1;
	dir_remota2.puerto += 1;

	/* Para ida+vuelta+ida, DESCOMENTAR:
	 * sleep(1); // Damos tiempo a la confirmación a llegar y ejecutar el 2º listen
	 */

	cx2 = t_connect(dir_local, &dir_remota2);
	if (cx2 < 0) {
	  //t_disconnect(cx1);
		return cx2;
	}

	init_noise(data, sizeof(data));
	do {
		char buf[sizeof(data)];
		gen_noise(data, sizeof(data));

		if (bs >= len) {
			flags |= CLOSE;
			bs = len;
		}
		len1 = t_send(cx1, data, bs, &flags);
		if (len1 < 0) {
		  //	t_disconnect(cx1);
		  //	t_disconnect(cx2);

			fprintf(stderr, "CX1: ");

			return len1;
		}

		len -= bs;

		len2 = t_receive(cx2, buf, bs, &flags);
		if (len2 < 0 && len2 != EXCLOSE && len2 != EXDISC) {
		  //	t_disconnect(cx1);
		  //	t_disconnect(cx2);

			fprintf(stderr, "CX2: ");

			return len2;
		} if (len > 0 && (len2 == EXCLOSE || (flags & CLOSE) == CLOSE)) {
			fprintf(stderr, "Entrante cerrado, len debería ser 0 y es %zd\n", len);
		}

		if (memcmp(data, buf, bs))
			printf("!"); // Error
		else
			printf("."); // Ok

		bs = (bs + 256)%sizeof(buf);
	} while (len > 0);

	printf("\n");

	return EXOK;
}

int main(int argc, char *argv[]) {
    bool listen = false;
    t_direccion dir_origen = {IN6ADDR_ANY_INIT, 0}, dir_destino = {IN6ADDR_LOOPBACK_INIT, 0};
    int error;
    int opt;

    while ((opt = getopt(argc, argv, "ld:s:r:")) != -1) {
        switch (opt) {
            case 'l':
                listen = true;
                break;
            case 'd':
                inet_pton(AF_INET6, optarg, &dir_destino.ip);
                break;
            case 's':
                dir_origen.puerto = atoi(optarg);
                break;
            case 'r':
                dir_destino.puerto = atoi(optarg);
                break;
            default:
                usage(basename(argv[0]));
                return -1;

        }
    }

    if (listen)
    	error = do_receiver_part(&dir_origen, &dir_destino);
    else
    	error = do_sender_part(&dir_origen, &dir_destino, 100000000UL);

    if (error < 0)
    	fprintf(stderr, "Acabamos con error: %s\n", error_to_str(error));

	return 0;
}
