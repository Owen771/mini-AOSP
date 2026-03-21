// mini-AOSP init — parses init.rc, fork+exec services in order
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <filesystem>
#include "../liblog/log.h"

static const std::string TAG = "init";
static const std::string RUNTIME_DIR = "/tmp/mini-aosp";
static const std::string PID_FILE = RUNTIME_DIR + "/init.pid";

struct Service {
    std::string name;
    std::vector<std::string> args; // command + arguments
    pid_t pid = -1;
};

// Parse init.rc: "service <name> <cmd> [args...]"
std::vector<Service> parse_init_rc(const std::string& path) {
    std::vector<Service> services;
    std::ifstream file(path);
    if (!file.is_open()) {
        miniaosp::log(TAG, "ERROR: Cannot open " + path);
        return services;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;
        if (keyword != "service") continue;

        Service svc;
        iss >> svc.name;
        std::string arg;
        while (iss >> arg) {
            svc.args.push_back(arg);
        }
        if (!svc.args.empty()) {
            services.push_back(svc);
        }
    }
    return services;
}

// Fork and exec a service, return child PID
pid_t launch_service(const Service& svc) {
    pid_t pid = fork();
    if (pid < 0) {
        miniaosp::log(TAG, "ERROR: fork() failed for " + svc.name);
        return -1;
    }
    if (pid == 0) {
        // Child process — exec the command
        std::vector<char*> argv;
        for (auto& a : svc.args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        // If exec fails
        std::cerr << "[init] exec failed for " << svc.name << ": " << strerror(errno) << std::endl;
        _exit(127);
    }
    return pid;
}

volatile sig_atomic_t g_shutdown = 0;
void handle_signal(int) { g_shutdown = 1; }

int main(int argc, char* argv[]) {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Create runtime directory
    std::filesystem::create_directories(RUNTIME_DIR);

    // Write our PID
    {
        std::ofstream pf(PID_FILE);
        pf << getpid();
    }

    // Find init.rc — check arg, then default path relative to CWD
    std::string rc_path;
    if (argc > 1) {
        rc_path = argv[1];
    } else {
        rc_path = "system/core/rootdir/init.rc";
    }

    miniaosp::log(TAG, "Parsing " + rc_path + "...");
    auto services = parse_init_rc(rc_path);

    if (services.empty()) {
        miniaosp::log(TAG, "ERROR: No services found in " + rc_path);
        return 1;
    }

    // Launch services in order with a small delay between each
    std::vector<pid_t> child_pids;
    for (auto& svc : services) {
        pid_t pid = launch_service(svc);
        if (pid > 0) {
            svc.pid = pid;
            child_pids.push_back(pid);
            miniaosp::log(TAG, "Starting " + svc.name + " (PID " + std::to_string(pid) + ")...");

            // Write child PID file
            std::ofstream pf(RUNTIME_DIR + "/" + svc.name + ".pid");
            pf << pid;

            // Brief pause to let the service initialize
            usleep(500000); // 500ms
        }
    }

    miniaosp::log(TAG, "All services started. Waiting...");

    // Wait for children or shutdown signal
    while (!g_shutdown) {
        int status;
        pid_t exited = waitpid(-1, &status, WNOHANG);
        if (exited > 0) {
            for (auto& svc : services) {
                if (svc.pid == exited) {
                    if (WIFEXITED(status)) {
                        miniaosp::log(TAG, svc.name + " exited with code " +
                            std::to_string(WEXITSTATUS(status)));
                    } else if (WIFSIGNALED(status)) {
                        miniaosp::log(TAG, svc.name + " killed by signal " +
                            std::to_string(WTERMSIG(status)));
                    }
                    svc.pid = -1;
                    break;
                }
            }

            // Check if all services exited
            bool any_alive = false;
            for (auto& svc : services) {
                if (svc.pid > 0) { any_alive = true; break; }
            }
            if (!any_alive) {
                miniaosp::log(TAG, "All services exited. Shutting down.");
                break;
            }
        }
        usleep(100000); // 100ms poll
    }

    // On shutdown, kill remaining children
    if (g_shutdown) {
        miniaosp::log(TAG, "Shutdown signal received. Stopping services...");
        for (auto& svc : services) {
            if (svc.pid > 0) {
                kill(svc.pid, SIGTERM);
                miniaosp::log(TAG, "Sent SIGTERM to " + svc.name + " (PID " + std::to_string(svc.pid) + ")");
            }
        }
        // Wait briefly for graceful shutdown, then force
        usleep(2000000); // 2s
        for (auto& svc : services) {
            if (svc.pid > 0) {
                int status;
                pid_t result = waitpid(svc.pid, &status, WNOHANG);
                if (result == 0) {
                    kill(svc.pid, SIGKILL);
                }
            }
        }
    }

    // Cleanup PID files
    for (auto& svc : services) {
        std::filesystem::remove(RUNTIME_DIR + "/" + svc.name + ".pid");
    }
    std::filesystem::remove(PID_FILE);
    miniaosp::log(TAG, "Shutdown complete.");
    return 0;
}
