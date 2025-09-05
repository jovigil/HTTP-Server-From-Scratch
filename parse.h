/*

joey vigil
jovigil
cse130
parse.h
~header file for http server's
message parsing module~

*/

#ifndef PARSE_H_INCLUDE_
#define PARSE_H_INCLUDE_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#define BUF_SIZE  8192
#define HEAD_SIZE 2048
#define RNRN      "\r\n\r\n"

// exported types

typedef struct RequestObj *Request;

enum StatusCode {
    OK = 200,
    CREATED = 201,
    BAD_REQ = 400,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    SERV_ERR = 500,
    NOT_IMPD = 501,
    VRSN_NSPD = 505
};

// exported functs

// creation/destruction

// newRequest()
// allocates a Request and initializes its
// fields. '\0' is placed at the ends of all
// character buffers and ints are initialized
// to zero.
Request newRequest();

// freeRequest()
// frees the dynamically allocated memory
// associated with the Request pointed to
// by pReq.
void freeRequest(Request *pReq);

// access functions

char *getHeadBuf(Request R);

// manipulation functions

// this will set the byte offset in R's
// hd_raw buffer to \0 for string manipulation
// purposes.
void stringify_hd(Request R, size_t offset);

// sets connection file desc for R
void setCFD(Request R, int fd);

void setHeadLen(Request R, int hl);

// parse_request()
// will attempt to parse a valid HTTP 1.1
// request header from the hd_buf field of
// R. if a valid header is present in R's
// hd_buf, the command, fname, and con_len
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
void parse_request(Request R);

// echo()
// will write n bytes buf->fd (or try to).
// returns number of bytes written or -1
// upon error.
int echo(int fd, char *buf, size_t n);

// make_response()
// puts an HTTP 1.1-formatted response with
// respect to status code stat in the char
// array pointed to by str. returns strlen
// of response.
int make_response(Request R);

// handle_request()
// prepares Request for being executed on
// GET or PUT methods, or prepares them to
// return an appropriate error response.
// Executes the method if all goes well
// and produces and sends a response to
// the socket in all cases.
void handle_request(Request R);

#endif
