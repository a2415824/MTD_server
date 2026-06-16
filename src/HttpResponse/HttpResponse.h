/*
 * ============================================================================
 *  HttpResponse.h — Represents the reply we send back to the client
 * ============================================================================
 *
 *  After our server receives a request and figures out what to do with it,
 *  we need to send a reply. This class builds that reply.
 *
 *  A typical HTTP response looks like this:
 *
 *      HTTP/1.1 200 OK\r\n                     ← status line
 *      Content-Type: text/html\r\n              ← header
 *      Content-Length: 45\r\n                   ← header
 *      Connection: close\r\n                    ← header
 *      \r\n                                     ← blank line (end of headers)
 *      <h1>Hello from C++ Web Server!</h1>      ← body
 *
 *  This class stores:
 *      - statusCode  → 200, 404, 500, etc.
 *      - statusText  → "OK", "Not Found", "Internal Server Error"
 *      - headers     → key-value pairs like Content-Type
 *      - body        → the actual HTML/JSON content
 *
 *  It also has helper methods to quickly build common responses:
 *      - makeOk()    → 200 OK with your content
 *      - makeError() → 404/500/etc. with an error message
 *
 * ============================================================================
 */

#pragma once

#include <string>
#include <unordered_map>

class HttpResponse {
public:
    // --- The components of our HTTP response ---
    int statusCode = 200;                                       // HTTP status code (200 = success)
    std::string statusText = "OK";                              // Human-readable status ("OK", "Not Found")
    std::unordered_map<std::string, std::string> headers;       // Response headers (Content-Type, etc.)
    std::string body;                                           // The actual content (HTML, JSON, etc.)

    /*
     * toString() — Converts this response object into a raw HTTP string
     *              that we can send over the network.
     *
     *  Combines status line + headers + blank line + body into one string.
     */
    std::string toString() const;

    /*
     * makeOk() — Quick helper to create a "200 OK" response.
     *
     *   body        : the content to send (HTML, JSON, plain text, etc.)
     *   contentType : defaults to "text/html" — change to "application/json" for APIs
     */
    static HttpResponse makeOk(const std::string& body,const std::string& contentType = "text/html");

    /*
     * makeError() — Quick helper to create an error response (404, 500, etc.)
     *
     *   code : HTTP status code (e.g., 404)
     *   msg  : human-readable message (e.g., "Not Found")
     */
    static HttpResponse makeError(int code,const std::string& msg);
};