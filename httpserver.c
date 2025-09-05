/*

joey vigil
jovigil
cse130
echo server
~scaffolding for asgn2~


*/

#include "asgn2_helper_funcs.h"
#include "parse.h"
#include <unistd.h>
#include <fcntl.h>

#define USAGE "Usage:\n./echoserver <port>"

int main(int argc, char *argv[]) {

    // allocate Listener_Socket struct
    Listener_Socket *sock = (Listener_Socket *) malloc(sizeof(Listener_Socket));

    // check for usage error and invalid port number
    if (argc != 2) {
        warnx(USAGE);
        exit(EXIT_FAILURE);
    }
    int p = atoi(argv[1]);
    if (p < 1 || p > 65535) {
        warnx("Invalid port number");
        exit(EXIT_FAILURE);
    }

    // initialize socket
    int sock_init = listener_init(sock, p);
    if (sock_init != 0) {
        warnx("Cannot initialize socket on port %d", p);
        exit(EXIT_FAILURE);
    }

    // read/write loop
    while (1) {
        int read_bytes;

        // accept connection
        int cfd = listener_accept(sock); // connection file descriptor
        Request Req = newRequest();
        setCFD(Req, cfd);
        if (cfd == 0) {
            warnx("Could not bind on port %d", p);
            goto done;
        }

        // get pointer to header buffer of Request obect
        char *hd_buf = getHeadBuf(Req);

        // read from socket into buffer and write from buf to sock
        read_bytes = read_until(cfd, hd_buf, BUF_SIZE, RNRN);
        setHeadLen(Req, read_bytes);
        if (read_bytes == -1) {
            warnx("BAD READ");
            goto done;
        }
        stringify_hd(Req, read_bytes); // put nul char at end of read material

        // send header to parser
        parse_request(Req);

        // send request to handler
        handle_request(Req);

    done:
        // close connection and free memory
        close(cfd);
        // printf("connection closed\n");
        freeRequest(&Req);
    }
    free(sock);
    exit(EXIT_SUCCESS);
}
