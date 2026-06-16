/*
 * ============================================================================
 *  HttpResponse.cpp — Builds the raw HTTP text that gets sent to the client
 * ============================================================================
 *
 *  This file takes our nice HttpResponse object and converts it into the
 *  exact text format that browsers and HTTP clients expect.
 *
 *  It also provides two convenient factory methods:
 *      - makeOk()    → quickly create a successful response
 *      - makeError() → quickly create an error response (404, 500, etc.)
 *
 * ============================================================================
 */

#include  "HttpResponse.h"
#include <sstream>

/*
 * toString() — Assembles the full HTTP response as a single string.
 *
 *  The format must be EXACTLY:
 *      HTTP/1.1 200 OK\r\n
 *      Header-Name: Header-Value\r\n
 *      \r\n                           ← blank line separates headers from body
 *      <body content here>
 *
 *  Browsers are strict about this format — missing "\r\n" = broken response!
 */
std::string HttpResponse::toString() const {
    std::ostringstream oss;

    // Line 1: Status line — "HTTP/1.1 200 OK"
    oss<<"HTTP/1.1  "<<statusCode<<" "<<statusText<<"\r\n";

    // Lines 2+: Headers — one per line, format "Key: Value"
    for (auto& [key,val] : headers) {
        oss<<key<<": "<<val<<"\r\n";
    }

    // Blank line: signals "headers are done, body starts next"
    oss<<"\r\n";

    // The actual content (HTML, JSON, etc.)
    oss<<body;

    return oss.str();
}

/*
 * makeOk() — Factory method for a standard "200 OK" response.
 *
 *  Sets up all the required headers automatically:
 *      - Content-Length : tells the client how many bytes to expect
 *      - Content-Type   : tells the client what kind of data this is
 *      - Connection     : "close" means we'll close the socket after sending
 *                         (as opposed to keep-alive, which reuses the connection)
 */
HttpResponse HttpResponse::makeOk(const std::string &body, const std::string &contentType) {
    HttpResponse r;
    r.statusCode = 200;
    r.statusText = "OK";
    r.body = body;

    r.headers["Content-Length" ] = std::to_string(body.size());  // How many bytes the body is
    r.headers["Content-Type"] = contentType;                     // "text/html" or "application/json"
    r.headers["Connection"] = "close";                           // Close connection after response
    return r;
}

/*
 * makeError() — Factory method for error responses (404, 500, etc.)
 *
 *  Builds a simple HTML page showing the error code and message.
 *  Example output for makeError(404, "Not Found"):
 *      <h1>404 Not Found</h1>
 */
HttpResponse HttpResponse::makeError(int code, const std::string &msg) {
    HttpResponse r;
    r.statusCode = code;
    r.statusText = msg;
    r.body = "<h1>"+std::to_string(code)+" "+msg+"</h1>";       // Simple HTML error page
    r.headers["Content-Length"] = std::to_string(r.body.size());
    r.headers["Content-Type"] = "text/html";
    r.headers["Connection"] = "close";
    return r;
}