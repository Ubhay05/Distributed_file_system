#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <cstring>
#include "../include/common.h"
#include "../include/socket_utils.h"
#include "../include/file_utils.h"

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

int main() {
    // All 3 storage nodes
    std::vector<std::pair<std::string, int>> nodes = {
        {"127.0.0.1", STORAGE_NODE_1_PORT},
        {"127.0.0.1", STORAGE_NODE_2_PORT},
        {"127.0.0.1", STORAGE_NODE_3_PORT}
    };

    int num_nodes = nodes.size();
    std::string command, arg1, arg2;

    std::cout << "=== DFS Client (with chunking) ===\n";
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

            // Split file into chunks
            std::cout << "Splitting " << arg1 << " into " 
                      << num_nodes << " chunks...\n";
            auto chunks = split_file(arg1, num_nodes);

            if (chunks.empty()) {
                std::cerr << "Error splitting file\n";
                continue;
            }

            // Send each chunk to a different node
            for (int i = 0; i < (int)chunks.size(); i++) {
                put_chunk(chunks[i], arg2, 
                         nodes[i].first, nodes[i].second);
            }
            std::cout << "PUT complete: " << arg2 
                      << " stored in " << chunks.size() << " chunks\n";

        } else if (command == "GET") {
            std::cin >> arg1 >> arg2;

            // Collect all chunks from their respective nodes
            std::cout << "Collecting chunks for " << arg1 << "...\n";
            std::vector<FileChunk> chunks;

            for (int i = 0; i < num_nodes; i++) {
                FileChunk chunk = get_chunk(arg1, i, 
                                           nodes[i].first, nodes[i].second);
                if (!chunk.data.empty()) {
                    chunks.push_back(chunk);
                }
            }

            if (chunks.empty()) {
                std::cerr << "Error: could not retrieve any chunks\n";
                continue;
            }

            // Reassemble
            reassemble_file(arg2, chunks);
            std::cout << "GET complete: " << arg2 << "\n";

        } else if (command == "LIST") {
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