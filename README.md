# C Socket Web Server with pthreads

A lightweight, multi-threaded HTTP web server implemented in C using sockets and POSIX threads (pthreads).

Created as a practical project for socket programming and multi-threading concepts.

## Overview

This is a simple web server that listens for HTTP client requests, parses them, and responds with a dynamically generated HTML confirmation page. The server handles multiple concurrent connections using thread-based concurrency.

## Features

- **Multi-threaded Architecture**: Uses pthreads to handle multiple client connections simultaneously
- **HTTP Request Parsing**: Extracts HTTP method, requested path, and protocol version from incoming requests
- **Thread Pool Management**: Limits concurrent threads to prevent resource exhaustion (max 100 threads)
- **Signal Handling**: Gracefully shuts down on SIGINT/SIGTERM signals
- **Socket-based Communication**: Uses Berkeley sockets (BSD sockets) for network communication
- **HTML Response**: Returns formatted HTML responses with client request details

## Technologies Used

- **Language**: C
- **Networking**: POSIX sockets (AF_INET, SOCK_STREAM)
- **Concurrency**: POSIX threads (pthreads)
- **Synchronization**: Mutex locks for thread-safe operations
- **OS**: Unix/Linux compatible
- **HTTP Protocol**: HTTP/1.0 and HTTP/1.1 support

## Configuration

Edit the following constants in `server.c` to customize:
- `PORT`: Server listening port (default: 8080)
- `BACKLOG`: Connection queue size (default: 10)
- `MAX_THREADS`: Maximum concurrent threads (default: 100)