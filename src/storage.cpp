#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <filesystem>
#include "../include/common.h"
#include "../include/socket_utils.h"
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// Handle one client connection
void handle_client(int client_fd, const std::string& storage_dir) {
    char buffer[BUFFER_SIZE] = {0};

    // Step 1: Read the command (PUT or GET or LIST)
    recv(client_fd, buffer, BUFFER_SIZE, 0);
    std::string command(buffer);
    std::cout << "DEBUG: Command received: " << command << "\n";

    if (command == "PUT") {
        // Step 2: Read filename
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_fd, buffer, BUFFER_SIZE, 0);
        std::string filename(buffer);
        std::string filepath = storage_dir + filename;
        std::cout << "DEBUG: Storing file at: " << filepath << "\n";

        // Step 3: Read file size
        long file_size = 0;
        recv(client_fd, &file_size, sizeof(long), 0);

        // Step 4: Read file data and write to disk
        std::ofstream outfile(filepath, std::ios::binary);
        long received = 0;
        while (received < file_size) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
            outfile.write(buffer, bytes);
            received += bytes;
        }
        outfile.close();

        // Step 5: Send confirmation
        std::string ack = "OK";
        send(client_fd, ack.c_str(), ack.size(), 0);
        std::cout << "DEBUG: File stored successfully\n";

    } else if (command == "GET") {
        // Step 2: Read filename
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_fd, buffer, BUFFER_SIZE, 0);
        std::string filename(buffer);
        std::string filepath = storage_dir + filename;

        // Step 3: Check if file exists
        if (!fs::exists(filepath)) {
            std::string err = "ERROR";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "DEBUG: File not found: " << filepath << "\n";
            close(client_fd);
            return;
        }

        // Step 4: Send file size
        long file_size = fs::file_size(filepath);
        send(client_fd, &file_size, sizeof(long), 0);

        // Step 5: Send file data
        std::ifstream infile(filepath, std::ios::binary);
        while (infile.read(buffer, BUFFER_SIZE) || infile.gcount() > 0) {
            send(client_fd, buffer, infile.gcount(), 0);
        }
        infile.close();
        std::cout << "DEBUG: File sent successfully\n";

    } else if (command == "LIST") {
        // Send all filenames in storage dir
        std::string filelist = "";
        for (const auto& entry : fs::directory_iterator(storage_dir)) {
            filelist += entry.path().filename().string() + "\n";
        }
        if (filelist.empty()) filelist = "EMPTY";
        send(client_fd, filelist.c_str(), filelist.size(), 0);
    }

    close(client_fd);
}

// Register this node with the master, then send periodic heartbeats
void register_with_master(int my_port) {
    std::string my_info = "127.0.0.1:" + std::to_string(my_port);

    // Initial registration
    int sock = connect_to_server("127.0.0.1", MASTER_PORT);
    if (sock < 0) {
        std::cerr << "Warning: could not reach master to register\n";
        return;
    }

    std::string cmd = "REGISTER";
    send(sock, cmd.c_str(), cmd.size() + 1, 0);
    usleep(10000);
    send(sock, my_info.c_str(), my_info.size() + 1, 0);

    char ack[10] = {0};
    recv(sock, ack, sizeof(ack), 0);
    close(sock);

    std::cout << "DEBUG: Registered with master as " << my_info << "\n";

    // Heartbeat loop - runs forever in background thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));

        int hb_sock = connect_to_server("127.0.0.1", MASTER_PORT);
        if (hb_sock < 0) continue; // master might be temporarily down

        std::string hb_cmd = "HEARTBEAT";
        send(hb_sock, hb_cmd.c_str(), hb_cmd.size() + 1, 0);
        usleep(10000);
        send(hb_sock, my_info.c_str(), my_info.size() + 1, 0);

        char hb_ack[10] = {0};
        recv(hb_sock, hb_ack, sizeof(hb_ack), 0);
        close(hb_sock);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./storage <port> <storage_dir>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string storage_dir = argv[2];

    // Create storage directory if it doesn't exist
    fs::create_directories(storage_dir);
    std::cout << "Storage node starting on port " << port << "\n";
    std::cout << "Storing files in: " << storage_dir << "\n";

    int server_fd = create_server_socket(port);
    if (server_fd < 0) return 1;

    // Start heartbeat thread - registers with master and keeps pinging it
    std::thread registration_thread(register_with_master, port);
    registration_thread.detach();

    std::cout << "Waiting for connections...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            std::cerr << "Error accepting connection\n";
            continue;
        }
        std::cout << "DEBUG: Client connected\n";
        handle_client(client_fd, storage_dir);
    }

    close(server_fd);
    return 0;
}