# Configuration Subsystem (`common/config`)

## 📐 System Design Philosophy

A distributed node requires dozens of configuration parameters (ports, timeouts, cache sizes, peer addresses). Passing these as individual arguments to constructors creates highly coupled, fragile code.

We solve this using **Strongly-Typed Plain Old Data (POD) Structs** loaded from a human-readable YAML file.

### Subsystem Isolation
The root configuration (`ServerConfig`) is broken down into subsystem-specific structs (`RaftConfig`, `StorageConfig`, `NodeConfig`). When instantiating the Storage Engine, we only pass `config.storage`. The Storage Engine remains completely unaware of RAFT timeouts or network ports, enforcing strict architectural boundaries.

## ⚙️ How It Works

### 1. `config.h`
Defines the structure of our configuration. This is pure data with no logic.
- `NodeConfig`: Identity (Node ID, IP address, Role).
- `StorageConfig`: Data directories, memtable thresholds, block cache limits.
- `RaftConfig`: Election timeouts, heartbeat intervals.
- `ClusterConfig`: Discovery addresses for peers.

### 2. `ConfigLoader` (`config_loader.h` & `.cpp`)
Uses the `yaml-cpp` library to parse a `config.yaml` file on disk.

**Error Handling Integration:**
`yaml-cpp` relies heavily on C++ exceptions for malformed files. Our `ConfigLoader` acts as a quarantine zone: it catches `YAML::Exception` internally and translates it into our exception-free `Status(StatusCode::kInvalidArgument)` paradigm.

```cpp
auto config_or = ConfigLoader::LoadFromFile("config.yaml");
if (!config_or.IsOk()) {
    // Gracefully handle the error, print to console, and exit
}
ServerConfig config = config_or.value();
```
