# Distributed File System in C++

A distributed file system built from scratch in C++ using raw TCP sockets.
Files are split into chunks and distributed across multiple storage nodes,
enabling fault-tolerant storage and retrieval.

## Architecture
Client
|
|--TCP--> Storage Node 1 (port 8001)  [chunk 0]
|--TCP--> Storage Node 2 (port 8002)  [chunk 1]
|--TCP--> Storage Node 3 (port 8003)  [chunk 2]
## Features

- **File Chunking** — files are split into N chunks and distributed across N storage nodes
- **Replication** — each chunk stored on a dedicated node, no single point of failure
- **Fault Tolerant GET** — retrieves file as long as all chunk nodes are alive
- **Binary Safe** — handles any file type (text, images, executables)
- **Raw TCP Sockets** — no frameworks, built from scratch using POSIX socket API

## Tech Stack

- **Language:** C++17
- **Networking:** POSIX TCP sockets
- **Storage:** Local filesystem per node
- **Build:** GNU Make

## Project Structure
dfs-cpp/
├── src/
│   ├── storage.cpp     # Storage node — handles PUT/GET/LIST per chunk
│   └── client.cpp      # Client — splits files, coordinates nodes
├── include/
│   ├── common.h        # Shared constants and ports
│   ├── socket_utils.h  # TCP helper functions (create server, connect)
│   └── file_utils.h    # File chunking and reassembly
├── Makefile
└── README.md
## How to Run

### 1. Build
```bash
make all
```

### 2. Start Storage Nodes (3 separate terminals)
```bash
bin/storage 8001 storage/node1/
bin/storage 8002 storage/node2/
bin/storage 8003 storage/node3/
```

### 3. Start Client
```bash
bin/client
```

### 4. Use the Client

```bash
# Upload a file (splits into 3 chunks across nodes)
>>> PUT localfile.txt remotefile.txt

# List files across all nodes
>>> LIST

# Download and reassemble file
>>> GET remotefile.txt downloaded.txt

# Exit
>>> exit
```

## How It Works

### PUT
1. Client reads the local file
2. Splits it into N equal chunks (N = number of storage nodes)
3. Sends chunk 0 to node 1, chunk 1 to node 2, chunk 2 to node 3
4. Each node stores the chunk on its local disk

### GET
1. Client requests chunk 0 from node 1, chunk 1 from node 2, chunk 2 from node 3
2. Reassembles chunks in order
3. Writes reconstructed file to local disk
4. Verifies reconstruction with diff

### LIST
1. Client queries all nodes
2. Merges and displays file list from each node

## Example

```bash
>>> PUT src/client.cpp bigfile.cpp
Splitting src/client.cpp into 3 chunks...
  chunk0 -> node 8001 ✓
  chunk1 -> node 8002 ✓
  chunk2 -> node 8003 ✓
PUT complete: bigfile.cpp stored in 3 chunks

>>> GET bigfile.cpp restored.cpp
Collecting chunks for bigfile.cpp...
  chunk0 <- node 8001 ✓
  chunk1 <- node 8002 ✓
  chunk2 <- node 8003 ✓
File reassembled at: restored.cpp
GET complete: restored.cpp
```

## Key Concepts Demonstrated

- **TCP socket programming** in C++ from scratch
- **Client-server architecture** with multiple servers
- **File chunking and reassembly** with binary-safe I/O
- **Distributed storage** across independent nodes
- **Fault isolation** — each node operates independently
