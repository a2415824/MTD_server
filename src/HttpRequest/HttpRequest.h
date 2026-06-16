/*
 * ============================================================================
 *  HttpRequest.h — Represents an incoming HTTP request from a client
 * ============================================================================
 *
 *  When a browser (or curl, Postman, etc.) sends a request to our server,
 *  it arrives as a big blob of raw text. This struct is the "clean version"
 *  of that text — we parse the raw text and store each piece separately
 *  so the rest of our code can easily work with it.
 *
 *  A typical raw HTTP request looks like this:
 *
 *      GET /hello HTTP/1.1\r\n          ← request line (method, path, version)
 *      Host: localhost:8080\r\n         ← header
 *      Accept: text/html\r\n            ← header
 *      \r\n                              ← empty line = end of headers
 *      (optional body goes here)        ← body (used in POST/PUT requests)
 *
 *  This struct breaks it down into:
 *      - method   → "GET"
 *      - path     → "/hello"
 *      - version  → "HTTP/1.1"
 *      - headers  → { "Host": "localhost:8080", "Accept": "text/html" }
 *      - body     → "" (empty for GET, filled for POST)
 *
 * ============================================================================
 */

#pragma once
#include <string>
#include <unordered_map>

struct HttpRequest {

    // --- The pieces we extract from the first line of the request ---
    std::string method;   // "GET", "POST", "PUT", "DELETE", etc.
    std::string path;     // "/", "/hello", "/api/users", etc.
    std::string version;  // "HTTP/1.1" (almost always this)

    // --- Key-value pairs from the header lines ---
    // Example: "Content-Type" → "application/json"
    std::unordered_map<std::string, std::string> headers;

    // --- The message body (usually empty for GET requests) ---
    std::string body;

    /*
     * parse() — takes raw HTTP text and fills an HttpRequest struct.
     *
     *   raw : the raw string we received from the client socket
     *   out : the HttpRequest struct we'll fill with parsed data
     *
     *   Returns true if parsing succeeded, false if the request is malformed.
     *   This is a static method — you call it as HttpRequest::parse(raw, req).
     */
    static bool parse(const std::string& raw , HttpRequest& out);
};