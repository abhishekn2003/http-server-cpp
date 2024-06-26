#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }

    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

// Function to parse the HTTP request
HttpRequest parseHttpRequest(const std::string& request) {
    HttpRequest httpRequest;
    std::istringstream stream(request);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream requestLineStream(line);
        requestLineStream >> httpRequest.method >> httpRequest.uri >>
            httpRequest.version;
        httpRequest.method = trim(httpRequest.method);
        httpRequest.uri = trim(httpRequest.uri);
        httpRequest.version = trim(httpRequest.version);
    }

    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string headerName = trim(line.substr(0, colon));
            std::string headerValue = trim(line.substr(
                colon + 1));  // Skip ":" (no space to accommodate trim)
            httpRequest.headers[headerName] = headerValue;
        }
    }

    // Parse body (if any)
    std::string body;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            body += line + "\n";
        }
    }
    httpRequest.body = trim(body);

    return httpRequest;
}

int main(int argc, char** argv) {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible
    // when running tests.
    std::cout << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
        0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) !=
        0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";

    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr,
                           (socklen_t*)&client_addr_len);
    std::cout << "Client connected\n";

    // Receive data from the client
    std::string client_req(1024, '\0');
    ssize_t bytesReceived = recv(client_fd, &client_req[0], 1024, 0);
    if (bytesReceived == -1) {
        std::cerr << "Failed to receive data." << std::endl;
        close(client_fd);
        close(server_fd);
        return 1;
    }

    HttpRequest httpRequest = parseHttpRequest(client_req);
    std::string response;

    if (httpRequest.method == "GET" && httpRequest.uri == "/") {
        response = "HTTP/1.1 200 OK\r\n\r\n";
    } else if (httpRequest.method == "GET" &&
               httpRequest.uri.find("/echo/") == 0) {
        std::string resBody = httpRequest.uri.substr(6);
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(resBody.length());
        response += "\r\n\r\n";
        response += resBody;
    } else if (httpRequest.method == "GET" &&
               httpRequest.uri == "/user-agent") {
        std::string resBody = httpRequest.headers["User-Agent"];

        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(resBody.length());
        response += "\r\n\r\n";
        response += resBody;
    } else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    send(client_fd, response.c_str(), response.length(), 0);
    close(server_fd);

    return 0;
}
