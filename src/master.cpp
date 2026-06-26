#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include "../include/common.h"
#include "../include/socket_utils.h"

// Info about each registered storage node
struct NodeInfo {
    std::string host;
    int port;
    std::chrono::steady_clock::time_point last_heartbeat;
};

// Shared registry of all known nodes - protected by mutex since
// multiple threads (one per connection) will access it
std::map<int, NodeInfo> registry;  // key = port (unique id)
std::mutex registry_mutex;

// Remove nodes that haven't sent a heartbeat recently
void cleanup_dead_nodes() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::lock_guard<std::mutex> lock(registry_mutex);
        auto now = std::chrono::steady_clock::now();

        for (auto it = registry.begin(); it != registry.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_heartbeat).count();

            if (elapsed > NODE_TIMEOUT) {
                std::cout << "DEBUG: Node " << it->second.port 
                         << " timed out, removing\n";
                it = registry.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// Handle one connection - either a node registering/heartbeating,
// or a client asking for the node list
void handle_connection(int client_fd) {
    char buffer[BUFFER_SIZE] = {0};
    recv(client_fd, buffer, BUFFER_SIZE, 0);
    std::string command(buffer);

    if (command == "REGISTER") {
        // Storage node is registering itself
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_fd, buffer, BUFFER_SIZE, 0);
        std::string info(buffer); // format: "host:port"

        size_t colon = info.find(':');
        std::string host = info.substr(0, colon);
        int port = std::stoi(info.substr(colon + 1));

        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            registry[port] = {host, port, std::chrono::steady_clock::now()};
        }

        std::cout << "DEBUG: Node registered: " << host << ":" << port 
                 << " (total nodes: " << registry.size() << ")\n";

        std::string ack = "OK";
        send(client_fd, ack.c_str(), ack.size(), 0);

    } else if (command == "HEARTBEAT") {
        // Storage node is sending a heartbeat to stay alive in registry
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_fd, buffer, BUFFER_SIZE, 0);
        std::string info(buffer);

        size_t colon = info.find(':');
        int port = std::stoi(info.substr(colon + 1));

        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            if (registry.count(port)) {
                registry[port].last_heartbeat = std::chrono::steady_clock::now();
            }
        }

        std::string ack = "OK";
        send(client_fd, ack.c_str(), ack.size(), 0);

    } else if (command == "GET_NODES") {
        // Client is asking for the current list of live nodes
        std::ostringstream oss;
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            for (auto& [port, info] : registry) {
                oss << info.host << ":" << info.port << "\n";
            }
        }
        std::string node_list = oss.str();
        if (node_list.empty()) node_list = "EMPTY";

        send(client_fd, node_list.c_str(), node_list.size(), 0);
        std::cout << "DEBUG: Sent node list to client (" 
                 << registry.size() << " nodes)\n";
    }

    close(client_fd);
}

int main() {
    int server_fd = create_server_socket(MASTER_PORT);
    if (server_fd < 0) return 1;

    std::cout << "Master node starting on port " << MASTER_PORT << "\n";
    std::cout << "Waiting for storage nodes to register...\n";

    // Background thread that removes dead nodes periodically
    std::thread cleanup_thread(cleanup_dead_nodes);
    cleanup_thread.detach();

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;

        // Handle each connection in its own thread so multiple
        // nodes/clients can talk to master at the same time
        std::thread t(handle_connection, client_fd);
        t.detach();
    }

    close(server_fd);
    return 0;
}