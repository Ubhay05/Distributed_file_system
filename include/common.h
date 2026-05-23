#pragma once

#include <string>

// Server ports
const int STORAGE_NODE_1_PORT = 8001;
const int STORAGE_NODE_2_PORT = 8002;
const int STORAGE_NODE_3_PORT = 8003;

// Buffer size for sending/receiving data
const int BUFFER_SIZE = 4096;

// Max clients waiting to connect
const int BACKLOG = 10;

// Storage directories for each node
const std::string STORAGE_DIR_1 = "storage/node1/";
const std::string STORAGE_DIR_2 = "storage/node2/";
const std::string STORAGE_DIR_3 = "storage/node3/";