/*

joey vigil
jovigil
cse130
parse.c
~source file for http
server mssg parser~

*/

#include "parse.h"
#include "asgn2_helper_funcs.h"
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <linux/limits.h>
#include <regex.h>
#include <dirent.h>
#define NUL '\0'

// const strings for messages

const char get[] = "GET";
const char put[] = "PUT";
const char http_vers[] = "HTTP/1.1";
const char ok[] = "OK"; // these are for the status line
const char created[] = "Created";
const char bad_req[] = "Bad Request";
const char forbidden[] = "Forbidden";
const char not_found[] = "Not Found";
const char serv_err[] = "Internal Server Error";
const char not_impd[] = "Not Implemented";
const char vrsn_nspd[] = "Version Not Supported";
const char ok_msg[] = "OK\n";
const char created_msg[] = "Created\n"; // and these are for the mssg body
const char bad_req_msg[] = "Bad Request\n";
const char forbidden_msg[] = "Forbidden\n";
const char not_found_msg[] = "Not Found\n";
const char serv_err_msg[] = "Internal Server Error\n";
const char not_impd_msg[] = "Not Implemented\n";
const char vrsn_nspd_msg[] = "Version Not Supported\n";
const char content_length[] = "Content-Length:";

// regexes
const char rl[] = "([a-zA-Z]{1,8}) (/[a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n";
const char hf[] = "([a-zA-Z0-9.-]{1,128}:) ([ -~]{1,128})\r\n";

// private types

/*
the RequestObj type is the private underlying object for the
public Request type. It holds fields for a header raw buffer,
a message body raw buffer, a response char array for string
making, a filename char array, a command char array, a content
length char array, an integer content_length, an integer
connection file descriptor, an integer offset for the end of 
the header within hd_raw, and an integer status code.

the exported Request type, which is a pointer to a RequestObj,
will be the interface between the main module and this module.
A single Request will be allocated and initialized by the main
module every time a connection is accepted. when the main module
calls parse_request(), a void function, parse_header will populate
the command, con_len, hd_eo,  fields with parsed info from the
header, which was passed to it via hd_raw. the status code will be
filled EITHER when parse_request() encounters in an error OR ELSE
when handle_request() executes.
*/

typedef struct RequestObj {
    char *hd_raw; // header raw buffer data
    char response[HEAD_SIZE + 23]; // response char array
    char command[4]; // "GET," "PUT," or some other 3 letter word
    char fname[PATH_MAX]; // filename given by request
    int hd_read;
    int method; // integer indicator of command
    int con_len; // content length of mssg body
    int fcon_len; // content length of target file
    int hd_eo; // index of byte in hd_raw DIRECTLY AFTER header
    int status; // HTTP status code
    int tfd; // target file descriptor
    int cfd; // connection socket file desc
} RequestObj;

enum methodCodes { NOT_SET, GET, PUT };

// private defs

void get_ex(Request R);
int put_ex(Request R);

// public function defs

// newRequest()
// allocates a Request and initializes its
// fields. '\0' is placed at the ends of all
// character buffers and ints are initialized
// to zero.
Request newRequest() {
    Request R = malloc(sizeof(RequestObj));
    R->hd_raw = (char *) malloc(BUF_SIZE + 1); // allocate buffers
    R->hd_raw[BUF_SIZE] = NUL; // make sure no one can fall off!!
    R->command[3] = NUL; // put nuls in strings
    memset(R->fname, NUL, PATH_MAX);
    memset(R->response, NUL, HEAD_SIZE + 23);
    R->con_len = -1;
    R->hd_eo = 0;
    R->status = 0;
    R->tfd = -1;
    R->cfd = 0;
    R->fcon_len = 0;
    R->method = NOT_SET;
    R->hd_read = 0;
    return R;
}

// freeRequest()
// frees the dynamically allocated memory
// associated with the Request pointed to
// by pReq.
void freeRequest(Request *pReq) {
    if (pReq != NULL && *pReq != NULL) {
        Request R = *pReq;
        free(R->hd_raw);
        free(R);
        close(R->tfd);
        R = NULL;
    }
}

char *getHeadBuf(Request R) {
    return R->hd_raw;
}

char *getResponse(Request R) {
    return R->response;
}

int getStatus(Request R) {
    return R->status;
}

// sets the connection file descriptor
// field, effectively passing it from
// the ListenerStruct that lives in
// httpserver.c to this module.
void setCFD(Request R, int fd) {
    R->cfd = fd;
}

void setHeadLen(Request R, int hl) {
    R->hd_read = hl;
}

// this will set the byte offset in R's
// hd_raw buffer to \0 for string manipulation
// purposes.
void stringify_hd(Request R, size_t offset) {
    R->hd_raw[offset] = '\0';
}

// make_response()
// puts an HTTP 1.1-formatted response with
// respect to status code stat in the char
// array pointed to by str.
int make_response(Request R) {
    char *str = R->response;
    size_t str_len = HEAD_SIZE + 23;
    size_t msg_len = 0;
    char stat_phrase[23];
    char msg[23];
    int ret;
    int stat = R->status;
    switch (stat) { // set pointers to status phrase strings and
    case OK: { // get size of message depending on status code
        strncpy(stat_phrase, ok, sizeof(stat_phrase));
        if (R->method == GET) {
            msg_len = R->fcon_len; // length of target file
        } else {
            msg_len = strlen(ok_msg);
            strncpy(msg, ok_msg, sizeof(msg));
        }
        break;
    }
    case CREATED: {
        strncpy(stat_phrase, created, sizeof(stat_phrase));
        msg_len = strlen(created_msg);
        strncpy(msg, created_msg, sizeof(msg));
        break;
    }
    case BAD_REQ: {
        strncpy(stat_phrase, bad_req, sizeof(stat_phrase));
        msg_len = strlen(bad_req_msg);
        strncpy(msg, bad_req_msg, sizeof(msg));
        break;
    }
    case FORBIDDEN: {
        strncpy(stat_phrase, forbidden, sizeof(stat_phrase));
        msg_len = strlen(forbidden_msg);
        strncpy(msg, forbidden_msg, sizeof(msg));
        break;
    }
    case NOT_FOUND: {
        strncpy(stat_phrase, not_found, sizeof(not_found));
        msg_len = strlen(not_found_msg);
        strncpy(msg, not_found_msg, sizeof(msg));
        break;
    }
    case SERV_ERR: {
        strncpy(stat_phrase, serv_err, sizeof(stat_phrase));
        msg_len = strlen(serv_err_msg);
        strncpy(msg, serv_err_msg, sizeof(msg));
        break;
    }
    case NOT_IMPD: {
        strncpy(stat_phrase, not_impd, sizeof(stat_phrase));
        msg_len = strlen(not_impd_msg);
        strncpy(msg, not_impd_msg, sizeof(msg));
        break;
    }
    case VRSN_NSPD: {
        strncpy(stat_phrase, vrsn_nspd, sizeof(stat_phrase));
        msg_len = strlen(vrsn_nspd_msg);
        strncpy(msg, vrsn_nspd_msg, sizeof(msg));
        break;
    }
    default: {
        warnx("INVALID STATUS CODE");
        break;
    }
    }

    // make the string and return bytes written (not incl. \n)
    if (R->method == GET && stat == OK) {
        ret = snprintf(str, str_len, "%s %d %s\r\n%s %d%s", http_vers, stat, stat_phrase,
            content_length, (int) msg_len, RNRN);
    } else {
        ret = snprintf(str, str_len, "%s %d %s\r\n%s %d%s%s", http_vers, stat, stat_phrase,
            content_length, (int) msg_len, RNRN, msg);
    }
    return ret;
}

// parse_request()
// will attempt to parse a valid HTTP 1.1
// request header from the hd_raw field of
// R. if a valid header is present in R's
// hd_raw, the command, fname, and con_len
// (if applicable) fields will be filled with
// the respective fields. On success, the status
// field of R is NOT touched and will remain at
// its initialzed value. On failure, such as
// Bad Request, Version Not Supported, or Not
// Implemented, the corresponding HTTP status
// code value will be placed in R's status field.
// A failed parse may result in some, but not all,
// of R's fields being set, depending on where the
// error appears in the request.
void parse_request(Request R) {
    char err_buf[100];
    char *buf = R->hd_raw;
    int i = 0;

    // set up the regex machines
    regex_t regRL, regHFL; // two regex machines, one for req. line
    regex_t *pregRL = &regRL, *pregHFL = &regHFL;
    regmatch_t pmRL[4], pmHFL[3]; // and one for header field list

    // compile
    regcomp(pregRL, rl, REG_EXTENDED);
    regcomp(pregHFL, hf, REG_EXTENDED);

    // attempt to match request line
    int rl_match = regexec(pregRL, buf, 4, pmRL, 0);
    if (rl_match != 0) {
        regerror(rl_match, pregRL, err_buf, sizeof(err_buf));
        warnx("%s", err_buf);
        // warnx("bad request");
        R->status = BAD_REQ;
        regfree(pregRL);
        regfree(pregHFL);
        return;
    }

    // make a string of the whole request line
    size_t req_len = pmRL[0].rm_eo - pmRL[0].rm_so;
    char req[req_len + 1];
    strncpy(req, buf + pmRL[0].rm_so, req_len);

    // make a string of the command field
    size_t cmd_len = pmRL[1].rm_eo - pmRL[1].rm_so;
    char cmd[cmd_len + 1];
    strncpy(cmd, req + pmRL[1].rm_so, cmd_len);
    cmd[cmd_len] = '\0';

    // make a string of the filename field
    size_t fname_len = pmRL[2].rm_eo - pmRL[2].rm_so;
    char fname[fname_len]; // won't use slash, so regular len is fine
    strncpy(fname, req + pmRL[2].rm_so + 1, fname_len - 1);
    fname[fname_len - 1] = '\0';

    // make a string of the version field
    size_t vers_len = pmRL[3].rm_eo - pmRL[3].rm_so;
    char vers[vers_len + 1];
    strncpy(vers, buf + pmRL[3].rm_so, vers_len);
    vers[vers_len] = '\0';

    // check to see if command field is valid
    if (strcmp(cmd, put) != 0 && strcmp(cmd, get) != 0) {
        R->status = NOT_IMPD;
        // warnx("not implemented");
        regfree(pregRL);
        regfree(pregHFL);
        return;
    }

    // check to see if version is 1.1
    if (strcmp(vers, http_vers) != 0) {
        R->status = VRSN_NSPD;
        // warnx("version not supported");
        regfree(pregRL);
        regfree(pregHFL);
        return;
    }

    // set cmd and fname fields of Request
    strncpy(R->command, cmd, cmd_len);
    strncpy(R->fname, fname, fname_len);
    if (strcmp(cmd, get) == 0) {
        R->method = GET;
    } else if (strcmp(cmd, put) == 0) {
        R->method = PUT;
    }

    // attempt to match header field "Content-Length: Val"
    // within potentially long header field list
    i = pmRL[0].rm_eo; // store end of req. line char in i
    int hfl_match = 0;
    while (hfl_match == 0) {
        hfl_match = regexec(pregHFL, buf + i, 3, pmHFL, 0);
        if (!hfl_match) { // hfl_match == 0 on success

            // check if match is consecutive to last one,
            // BAD_REQ if not
            // start offset should equal i if it is consec.
            if (pmHFL[0].rm_so != 0) {
                R->status = BAD_REQ;
                goto breakout;
            }

            // make a string of key: value
            size_t key_len = pmHFL[1].rm_eo - pmHFL[1].rm_so;
            char key[key_len + 1];
            strncpy(key, buf + i, req_len);
            key[key_len] = '\0';

            size_t val_len = pmHFL[2].rm_eo - pmHFL[2].rm_so;
            char val[val_len + 1];
            strncpy(val, buf + i + pmHFL[2].rm_so, val_len);
            val[val_len] = '\0';

            // check if field is content length and
            // store info if yes
            if (strcmp(key, content_length) == 0) {
                int cl = atoi(val);
                R->con_len = cl;
            }

            // update i
            i += pmHFL[0].rm_eo;
        }
    }

    // check for empty line at end
    if (buf[i] != '\r' || buf[i + 1] != '\n') {
        R->status = BAD_REQ;
    }

    // check if put request w no content length field
    if (strcmp(cmd, put) == 0 && R->con_len == -1) {
        R->status = BAD_REQ;
    }

    // set end of header offset
    R->hd_eo = i + 2;

breakout:
    // clean up and return
    regfree(pregRL);
    regfree(pregHFL);
    return;
}

// handle_request()
// prepares Request for being executed on
// GET or PUT methods, or prepares them to
// return an appropriate error response.
// Executes the method if all goes well
// and produces and sends a response to
// the socket in all cases.
void handle_request(Request R) {
    bool get = false;
    bool put = false;
    if (R->method == GET) { // find out which method
        get = true;
    }
    if (R->method == PUT && R->con_len != -1) {
        put = true;
    }
    char *fn = R->fname;

    // set up for get
    // try to open file and set status accordingly
    if (get) {
        DIR *d;
        if ((d = opendir(fn)) != NULL) { // check if is dir
            R->status = FORBIDDEN;
        } else if (access(fn, F_OK) != 0) { // check for existence
            R->status = NOT_FOUND;
        } else if (access(fn, R_OK) != 0) { // check for permissions
            R->status = FORBIDDEN;
        } else {
            int fd = open(fn, O_RDONLY);
            if (fd < 0) {
                R->status = SERV_ERR;
            } else {
                R->status = OK;
                R->tfd = fd; // store fd in struct so get() can access
                struct stat st; // find out file size and store in struct
                fstat(fd, &st);
                R->fcon_len = (int) st.st_size;
            }
        }
    }

    // set up for put
    // try to open file, create if need be, and set status
    if (put) {

        int fd;
        if (access(fn, F_OK) == 0) { // file exists case

            DIR *d;
            if ((d = opendir(fn)) != NULL) { // check if is dir
                R->status = FORBIDDEN;
            } else if (access(fn, W_OK) != 0) { // check for permission
                R->status = FORBIDDEN;
            } else {
                fd = open(fn, O_RDWR | O_TRUNC); // truncate and get fd
                if (fd < 0) {
                    R->status = SERV_ERR;
                } else {
                    R->status = OK; // store in struct and set status
                    R->tfd = fd;
                }
            }
        } else { // file dne case
            fd = open(fn, O_RDWR | O_CREAT, 0666); // create it
            if (fd < 0) {
                R->status = SERV_ERR;
            } else {
                R->status = CREATED; // store in struct and set status
                R->tfd = fd;
            }
        }
        if (R->status == OK || R->status == CREATED) {
            put_ex(R);
        }
    }

    // make response and write to sock
    int resp_len = make_response(R);
    write_n_bytes(R->cfd, R->response, resp_len);

    // perform get execution
    if (get) {
        get_ex(R);
    }
}

// private functions

// get()
void get_ex(Request R) {
    pass_n_bytes(R->tfd, R->cfd, R->fcon_len);
}

// put()
int put_ex(Request R) {
    int cl = R->con_len;
    int buf_remainder = R->hd_read - R->hd_eo;
    int transferred = 0, total = 0;
    if (cl > buf_remainder) { // cl large, need to read from buf AND sock case
        transferred = write_n_bytes(R->tfd, R->hd_raw + R->hd_eo, buf_remainder);
        cl -= transferred;
        total += transferred;
        while (cl > 0) {
            transferred = pass_n_bytes(R->cfd, R->tfd, cl);
            cl -= transferred;
            total += transferred;
        }
    } else { // cl small, just need to read from buf case
        transferred = write_n_bytes(R->tfd, R->hd_raw + R->hd_eo, cl);
        total += transferred;
    }
    if (total != R->con_len) {
        warnx("PUT WRONG NUMBER OF BYTES");
    }
    return total;
}
