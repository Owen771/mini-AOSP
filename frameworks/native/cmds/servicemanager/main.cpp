// mini-AOSP servicemanager — Unix socket service registry
// Protocol: ADD_SERVICE <name> <socket_path>\n → OK\n
//           GET_SERVICE <name>\n → <socket_path>\n or NOT_FOUND\n
//           LIST_SERVICES\n → <name1> <name2> ...\n
#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <filesystem>
#include "../../../../system/core/liblog/log.h"

static const std::string TAG = "servicemanager";
static const std::string SOCKET_PATH = "/tmp/mini-aosp/servicemanager.sock";

static std::unordered_map<std::string, std::string> g_services;

volatile sig_atomic_t g_shutdown = 0;
void handle_signal(int) { g_shutdown = 1; }

// Handle a single client connection
void handle_client(int client_fd) {
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    // Strip trailing newline
    std::string request(buf);
    while (!request.empty() && (request.back() == '\n' || request.back() == '\r')) {
        request.pop_back();
    }

    std::istringstream iss(request);
    std::string command;
    iss >> command;

    std::string response;

    if (command == "ADD_SERVICE") {
        std::string name, path;
        iss >> name >> path;
        if (!name.empty() && !path.empty()) {
            g_services[name] = path;
            miniaosp::log(TAG, "Registered service: " + name + " → " + path);
            response = "OK\n";
        } else {
            response = "ERROR: usage: ADD_SERVICE <name> <socket_path>\n";
        }
    } else if (command == "GET_SERVICE") {
        std::string name;
        iss >> name;
        auto it = g_services.find(name);
        if (it != g_services.end()) {
            response = it->second + "\n";
        } else {
            response = "NOT_FOUND\n";
        }
    } else if (command == "LIST_SERVICES") {
        for (auto& [name, path] : g_services) {
            response += name + " ";
        }
        if (response.empty()) response = "(none)";
        response += "\n";
    } else {
        response = "ERROR: unknown command\n";
    }

    write(client_fd, response.c_str(), response.size());
}

int main() {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    std::filesystem::create_directories("/tmp/mini-aosp");

    // Clean up stale socket
    unlink(SOCKET_PATH.c_str());

    // Create Unix domain socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        miniaosp::log(TAG, "ERROR: socket() failed: " + std::string(strerror(errno)));
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        miniaosp::log(TAG, "ERROR: bind() failed: " + std::string(strerror(errno)));
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        miniaosp::log(TAG, "ERROR: listen() failed: " + std::string(strerror(errno)));
        close(server_fd);
        return 1;
    }

    miniaosp::log(TAG, "Listening on " + SOCKET_PATH);

    // Accept loop
    while (!g_shutdown) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(server_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ready == 0) continue; // timeout, check g_shutdown

        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            miniaosp::log(TAG, "ERROR: accept() failed: " + std::string(strerror(errno)));
            continue;
        }

        handle_client(client_fd);
        close(client_fd);
    }

    miniaosp::log(TAG, "Shutting down.");
    close(server_fd);
    unlink(SOCKET_PATH.c_str());
    return 0;
}
