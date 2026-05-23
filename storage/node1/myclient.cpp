#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <cstring>
#include "../include/common.h"
#include "../include/socket_utils.h"

// Upload a file to a storage node
void put_file(const std::string& local_path, const std::string& remote_name, 
              const std::string& host, int port) {
    // Open local file
    std::ifstream infile(local_path, std::ios::binary);
    if (!infile) {
        std::cerr << "Error: cannot open file " << local_path << "\n";
        return;
    }

    // Connect to storage node
    int sock = connect_to_server(host, port);
    if (sock < 0) return;

    // Send command
    std::string cmd = "PUT";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);
    usleep(10000); // small delay so server reads command first

    // Send filename
    send(sock, remote_name.c_str(), remote_name.size() + 1, 0);
    usleep(10000);

    // Get file size and send it
    infile.seekg(0, std::ios::end);
    long file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);
    send(sock, &file_size, sizeof(long), 0);

    // Send file data
    char buffer[BUFFER_SIZE];
    while (infile.read(buffer, BUFFER_SIZE) || infile.gcount() > 0) {
        send(sock, buffer, infile.gcount(), 0);
    }
    infile.close();

    // Wait for confirmation
    char ack[10] = {0};
    recv(sock, ack, sizeof(ack), 0);
    if (std::string(ack) == "OK") {
        std::cout << "PUT successful: " << remote_name << "\n";
    }

    close(sock);
}

// Download a file from a storage node
void get_file(const std::string& remote_name, const std::string& local_path,
              const std::string& host, int port) {
    // Connect to storage node
    int sock = connect_to_server(host, port);
    if (sock < 0) return;

    // Send command
    std::string cmd = "GET";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);
    usleep(10000);

    // Send filename
    send(sock, remote_name.c_str(), remote_name.size() + 1, 0);

    // Check response
    char response[10] = {0};
    recv(sock, response, sizeof(response), 0);
    if (std::string(response) == "ERROR") {
        std::cerr << "Error: file not found on server\n";
        close(sock);
        return;
    }

    // Read file size
    long file_size = 0;
    memcpy(&file_size, response, sizeof(long));

    // Receive file data
    std::ofstream outfile(local_path, std::ios::binary);
    char buffer[BUFFER_SIZE];
    long received = 0;
    while (received < file_size) {
        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        outfile.write(buffer, bytes);
        received += bytes;
    }
    outfile.close();

    std::cout << "GET successful: saved to " << local_path << "\n";
    close(sock);
}

// List files on a storage node
void list_files(const std::string& host, int port) {
    int sock = connect_to_server(host, port);
    if (sock < 0) return;

    std::string cmd = "LIST";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);

    char buffer[BUFFER_SIZE] = {0};
    recv(sock, buffer, BUFFER_SIZE, 0);
    std::cout << "Files on node " << port << ":\n" << buffer << "\n";

    close(sock);
}

int main() {
    // All 3 storage nodes
    std::vector<std::pair<std::string, int>> nodes = {
        {"127.0.0.1", STORAGE_NODE_1_PORT},
        {"127.0.0.1", STORAGE_NODE_2_PORT},
        {"127.0.0.1", STORAGE_NODE_3_PORT}
    };

    std::string command, arg1, arg2;

    std::cout << "=== DFS Client ===\n";
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
            // Send to ALL 3 nodes (replication)
            for (auto& node : nodes) {
                std::cout << "Sending to node " << node.second << "...\n";
                put_file(arg1, arg2, node.first, node.second);
            }

        } else if (command == "GET") {
            std::cin >> arg1 >> arg2;
            // Get from first available node
            for (auto& node : nodes) {
                int sock = connect_to_server(node.first, node.second);
                if (sock < 0) continue;
                close(sock);
                std::cout << "Fetching from node " << node.second << "...\n";
                get_file(arg1, arg2, node.first, node.second);
                break; // got it from first available node
            }

        } else if (command == "LIST") {
            // List from all nodes and merge
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