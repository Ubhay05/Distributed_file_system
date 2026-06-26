#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <cstring>
#include "../include/common.h"
#include "../include/socket_utils.h"
#include "../include/file_utils.h"
#include <sstream>

// Send one chunk to a storage node
void put_chunk(const FileChunk& chunk, const std::string& remote_name,
               const std::string& host, int port) {
    int sock = connect_to_server(host, port);
    if (sock < 0) return;

    // Send command
    std::string cmd = "PUT";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);
    usleep(10000);

    // Send filename with chunk index eg. myfile.cpp_chunk0
    std::string chunk_name = remote_name + "_chunk" + std::to_string(chunk.index);
    send(sock, chunk_name.c_str(), chunk_name.size() + 1, 0);
    usleep(10000);

    // Send chunk size
    long chunk_size = chunk.data.size();
    send(sock, &chunk_size, sizeof(long), 0);

    // Send chunk data
    send(sock, chunk.data.data(), chunk.data.size(), 0);

    // Wait for ack
    char ack[10] = {0};
    recv(sock, ack, sizeof(ack), 0);
    if (std::string(ack) == "OK") {
        std::cout << "  chunk" << chunk.index << " -> node " << port << " ✓\n";
    }

    close(sock);
}

// Get one chunk from a storage node
FileChunk get_chunk(const std::string& remote_name, int chunk_index,
                    const std::string& host, int port) {
    FileChunk chunk;
    chunk.index = chunk_index;

    int sock = connect_to_server(host, port);
    if (sock < 0) return chunk;

    // Send command
    std::string cmd = "GET";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);
    usleep(10000);

    // Send chunk filename
    std::string chunk_name = remote_name + "_chunk" + std::to_string(chunk_index);
    send(sock, chunk_name.c_str(), chunk_name.size() + 1, 0);

    // Check response
    char response[10] = {0};
    recv(sock, response, sizeof(response), 0);
    if (std::string(response) == "ERROR") {
        std::cerr << "  Error: chunk" << chunk_index << " not found on node " << port << "\n";
        close(sock);
        return chunk;
    }

    // Read chunk size
    long chunk_size = 0;
    memcpy(&chunk_size, response, sizeof(long));

    // Receive chunk data
    chunk.data.resize(chunk_size);
    long received = 0;
    while (received < chunk_size) {
        int bytes = recv(sock, chunk.data.data() + received, 
                        chunk_size - received, 0);
        received += bytes;
    }

    std::cout << "  chunk" << chunk_index << " <- node " << port << " ✓\n";
    close(sock);
    return chunk;
}

// List files on a storage node
void list_files(const std::string& host, int port) {
    int sock = connect_to_server(host, port);
    if (sock < 0) return;

    std::string cmd = "LIST";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);

    char buffer[BUFFER_SIZE] = {0};
    recv(sock, buffer, BUFFER_SIZE, 0);
    std::cout << "Node " << port << ":\n" << buffer << "\n";

    close(sock);
}

std::vector<std::pair<std::string, int>> get_nodes_from_master() {
    std::vector<std::pair<std::string, int>> nodes;

    int sock = connect_to_server("127.0.0.1", MASTER_PORT);
    if (sock < 0) {
        std::cerr << "Error: could not reach master node\n";
        return nodes;
    }

    std::string cmd = "GET_NODES";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);

    char buffer[BUFFER_SIZE] = {0};
    recv(sock, buffer, BUFFER_SIZE, 0);
    std::string response(buffer);
    close(sock);

    if (response == "EMPTY") return nodes;

    std::istringstream iss(response);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        size_t colon = line.find(':');
        std::string host = line.substr(0, colon);
        int port = std::stoi(line.substr(colon + 1));
        nodes.push_back({host, port});
    }

    return nodes;
}

int main() {
    std::string command, arg1, arg2;

    std::cout << "=== DFS Client (with master coordination) ===\n";
    std::cout << "Commands: PUT <local_file> <remote_name>\n";
    std::cout << "          GET <remote_name> <local_file>\n";
    std::cout << "          LIST\n";
    std::cout << "          exit\n\n";

    while (true) {
        std::cout << ">>> ";
        std::cin >> command;

        if (command == "exit") break;

        else if (command == "PUT") {
            std::cin >> arg1 >> arg2;

            // Refresh node list right before this operation
            auto nodes = get_nodes_from_master();
            if (nodes.empty()) {
                std::cerr << "Error: no storage nodes available right now\n";
                continue;
            }
            std::cout << "Using " << nodes.size() << " active nodes\n";

            int num_nodes = nodes.size();
            auto chunks = split_file(arg1, num_nodes);
            if (chunks.empty()) {
                std::cerr << "Error splitting file\n";
                continue;
            }

            for (int i = 0; i < (int)chunks.size(); i++) {
                put_chunk(chunks[i], arg2, nodes[i].first, nodes[i].second);
            }
            std::cout << "PUT complete: " << arg2 
                      << " stored in " << chunks.size() << " chunks\n";

        } else if (command == "GET") {
            std::cin >> arg1 >> arg2;

            // Refresh node list right before this operation
            auto nodes = get_nodes_from_master();
            if (nodes.empty()) {
                std::cerr << "Error: no storage nodes available right now\n";
                continue;
            }
            std::cout << "Using " << nodes.size() << " active nodes\n";

            int num_nodes = nodes.size();
            std::vector<FileChunk> chunks;

            for (int i = 0; i < num_nodes; i++) {
                FileChunk chunk = get_chunk(arg1, i, nodes[i].first, nodes[i].second);
                if (!chunk.data.empty()) {
                    chunks.push_back(chunk);
                }
            }

            if (chunks.empty()) {
                std::cerr << "Error: could not retrieve any chunks\n";
                continue;
            }

            reassemble_file(arg2, chunks);
            std::cout << "GET complete: " << arg2 << "\n";

        } else if (command == "LIST") {
            auto nodes = get_nodes_from_master();
            if (nodes.empty()) {
                std::cerr << "Error: no storage nodes available right now\n";
                continue;
            }

            std::cout << "Files across all nodes:\n";
            for (auto& node : nodes) {
                list_files(node.first, node.second);
            }

        } else {
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}