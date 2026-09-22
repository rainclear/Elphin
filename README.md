# Elphin 🚀

A high-performance, asynchronous, in-memory Key-Value server written in modern **C++20**. 

Elphin features a non-blocking Reactor event loop (`epoll`), a thread-safe task queue, custom memory buffers, and a Redis-compatible protocol parser (RESP). It provides high-concurrency throughput with native support for Key-Value pairs, TTL expiration, and Sorted Sets driven by a SkipList.

---

## Key Features

- **C++20 Architecture**: Built from the ground up using C++20 features (`std::jthread`, `std::stop_token`, `std::from_chars`, `<format>`, smart pointers).
- **Non-blocking Reactor I/O**: High-concurrency event-driven network engine backed by Linux `epoll`.
- **Redis Compatible**: Native implementation of the RESP (Redis Serialization Protocol) with inline fallback support.
- **Sorted Sets via SkipList**: Fast $\mathcal{O}(\log N)$ range queries and ordering using a custom dynamic SkipList.
- **TTL & Expiration Strategy**: Dual-mode expiration system utilizing both lazy eviction on read and active background sampling via a multi-threaded `ThreadPool`.
- **Zero External Dependencies**: Standard C++20 library + POSIX socket APIs.

---

## Architecture Overview

```text
src/
├── common/       # Infrastructure (ThreadPool, C++20 Logger)
├── net/          # Network layer (Reactor, Acceptor, Connection, Buffer, SocketUtils)
├── protocol/     # Protocol layer (RESP Parser & RESP Builder)
├── storage/      # In-Memory storage engine (Database, SkipList)
├── server/       # High-level glue controller (Server, Config)
└── main.cpp      # Server execution entry point