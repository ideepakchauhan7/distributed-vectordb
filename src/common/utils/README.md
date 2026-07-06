# Utilities Subsystem (`common/utils`)

## 📐 System Design Philosophy

Every large-scale C++ project needs a centralized location for pure, stateless helper functions. Instead of copying and pasting string parsing logic or hashing algorithms across the Storage Engine, RAFT, and the gRPC API, we centralize them here to ensure heavily tested, optimized, and zero-dependency functions.

## ⚙️ How It Works

### 1. Cryptographic vs. Non-Cryptographic Hashing (`Hash`)
In a distributed database, vectors must be **sharded** (partitioned) across multiple physical servers. We route a vector to a shard by hashing its unique identifier (e.g., `"doc_7482"`).

Using cryptographic hashes like SHA-256 or MD5 is computationally expensive and completely unnecessary, as we are not defending against malicious collision attacks. Instead, we implemented **FNV-1a**, an incredibly fast, non-cryptographic hash function that processes bytes sequentially using a prime multiplication and bitwise XOR. It provides excellent distribution (avalanche effect) across our cluster nodes in just a few CPU cycles.

### 2. String Manipulation (`StringUtils`)
Standard C++ (`std::string`) natively lacks basic utilities like `split` and `trim`. 
- **`Split`**: Critical for parsing configuration files and translating peer address strings (`"192.168.1.10:8080"`) into IPs and Ports for the networking layer.
- **`Trim`**: Critical for sanitizing user inputs from the REST API or removing accidental trailing whitespace from YAML configuration files.
