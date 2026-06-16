/*
 * ============================================================================
 *  HttpRequest.cpp — Parses raw HTTP text into a clean HttpRequest struct
 * ============================================================================
 *
 *  This file contains the actual logic that reads a raw string like:
 *      "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n"
 *  and breaks it into method, path, version, headers, and body.
 *
 *  Think of it like opening a letter:
 *      1. Read the first line to know what the sender wants (method + path)
 *      2. Read the header lines to get extra info (like "who sent this?")
 *      3. After the blank line, read the body (the actual message content)
 *
 * ============================================================================
 */

#include "HttpRequest.h"
#include <sstream>

bool HttpRequest::parse(const std::string &raw, HttpRequest &out) {

    // Wrap the raw text in a stream so we can read it line by line
    std::istringstream stream(raw);
    std::string line;

    // ── Step 1: Read the FIRST line (the "request line") ──
    // Example: "GET /hello HTTP/1.1\r\n"
    if (!std::getline(stream,line))return false;

    // HTTP uses "\r\n" as line endings, but getline() only strips "\n".
    // So we need to manually remove the trailing '\r' if it's there.
    if (!line.empty() && line.back()=='\r')line.pop_back();

    // Split the first line into three parts: method, path, version
    // "GET /hello HTTP/1.1" → method="GET", path="/hello", version="HTTP/1.1"
    std::istringstream firstLine(line);
    firstLine >> out.method >> out.path >> out.version;

    // A valid request MUST have at least a method and a path
    if (out.method.empty() || out.path.empty()) return false;

    // ── Step 2: Read HEADER lines until we hit an empty line ──
    // Each header looks like: "Key: Value"
    // The empty line ("\r\n" alone) signals the end of headers
    while (std::getline(stream,line)) {
        // Strip trailing '\r' (same reason as above)
        if (!line.empty() && line.back() == '\r')line.pop_back();

        // Empty line = we've reached the end of headers
        if (line.empty())break;

        // Find the colon that separates the key from the value
        // "Content-Type: text/html" → colon is at position 12
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);        // Everything before ':'
            std::string val = line.substr(colon+2);          // Everything after ': ' (skip colon and space)
            out.headers[key] = val;                          // Store in our map
        }
    }

    // ── Step 3: Read the BODY (everything after the blank line) ──
    // GET requests usually have no body, but POST/PUT requests do
    std::string bodyLine;
    while (std::getline(stream,bodyLine)) {
        out.body += bodyLine+"\n";
    }

    // If we got here, parsing was successful!
    return true;
}