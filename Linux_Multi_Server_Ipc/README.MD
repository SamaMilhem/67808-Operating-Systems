
# Multi-Server Communication System

## Overview

This project is a **Linux-based multi-server communication system** that facilitates interaction between clients and multiple servers using **shared memory** and **socket communication**. It demonstrates the use of inter-process communication (IPC) mechanisms in Linux, enabling efficient resource management and categorized server operations.

The system includes server-side and client-side implementations designed to interact seamlessly, handle concurrent operations, and ensure resource cleanup.

---

## Features

### Server
- **Socket Communication**:
    - Listens for incoming client connections.
    - Sends messages to clients over TCP sockets.

- **Shared Memory**:
    - Creates and manages shared memory segments for inter-process communication (IPC).

- **Error Handling**:
    - Ensures proper cleanup of shared memory, sockets, and temporary files during errors or shutdown.

### Client
- **Server Discovery**:
    - Reads server information from designated directory files to identify available servers.

- **Message Retrieval**:
    - Retrieves messages from servers using shared memory and sockets.

- **Categorization**:
    - Categorizes servers as:
        - **Host**: Supports both shared memory and socket communication.
        - **Container**: Supports shared memory only.
        - **VM**: Does not support either shared memory or socket communication.

- **Statistics Reporting**:
    - Displays a detailed summary of connected servers and retrieved messages.

---

## System Requirements

- **Operating System**: Linux (mandatory due to reliance on Linux-specific APIs).
- **Compiler**: GCC or any C++ compiler that supports C++11 or higher.
- **Required Libraries**:
    - System libraries for IPC and networking:
        - `<sys/shm.h>`, `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`.
    - Standard C++ libraries for file I/O, string manipulation, and logging:
        - `<iostream>`, `<fstream>`, `<sstream>`.

---

## Project Structure

### Directories and Files
- **`server.cpp`**:
    - Implements server functionalities like shared memory creation, socket setup, and message handling.

- **`client.cpp`**:
    - Implements client functionalities such as server discovery, message retrieval, and statistics reporting.

- **`globals.h`**:
    - Contains shared constants and data structures, including `live_server_info` for managing server state.

- **`server.h`** and **`client.h`**:
    - Declare all the necessary functions for server and client operations.

- **`Makefile`**:
    - Automates the build process for the server and client executables.

---

## Compilation and Execution

### Compilation
Use the provided `Makefile` to compile the project:

```bash
make
```

This will generate two executables:
- `server`
- `client`

### Running the System

#### 1. Start the Server
Run the server executable to create shared memory and listen for incoming client connections:

```bash
./server
```

#### 2. Run the Client
Run the client and specify the directory containing server info files to interact with:

```bash
./client <path_to_info_directory>
```

Replace `<path_to_info_directory>` with the directory path where the server info files are stored.

---

## How It Works

### Server Workflow
1. Creates a TCP socket and listens for incoming connections.
2. Allocates shared memory for IPC.
3. Sends messages to clients via shared memory or sockets.
4. Ensures proper cleanup of resources on shutdown.

### Client Workflow
1. Reads server info files to discover available servers.
2. Connects to servers via sockets or shared memory.
3. Retrieves messages and categorizes servers based on connection type.
4. Displays a summary of server statistics and messages.

---

## Error Handling

- **Server**:
    - Handles failures in socket setup, shared memory allocation, and file creation.
    - Ensures cleanup of resources like sockets, shared memory, and files on error.

- **Client**:
    - Handles errors in connecting to servers, reading files, and retrieving messages.
    - Logs appropriate messages and skips problematic servers.
