# Thread Pool Subsystem (`common/thread_pool`)

## 📐 System Design Philosophy

In modern C++, creating and destroying `std::thread` objects dynamically for every incoming request is extremely slow due to OS kernel context-switching overhead. A high-performance database requires a pool of pre-allocated background threads that sleep until work arrives.

We implement a highly optimized, lock-minimizing `ThreadPool` that automatically wraps tasks in C++ futures (`std::packaged_task`), allowing caller threads to seamlessly schedule work and wait for results without manually managing condition variables.

## ⚙️ How It Works

### 1. The Queue and the Lock
The core of the thread pool is `std::queue<std::function<void()>>`.
Worker threads wake up, lock the `std::mutex`, pop one function off the queue, **unlock the mutex**, and then execute the function.
Unlocking the mutex *before* executing the function is absolutely critical. If a thread holds the lock while traversing a massive HNSW graph, it blocks all other worker threads from popping lightweight tasks off the queue, essentially turning a multi-threaded system into a single-threaded bottleneck.

### 2. `std::invoke_result` & Promises
The `Enqueue` template method leverages modern C++ type traits (`std::invoke_result`) to deduce the return type of any function or lambda passed to it. It wraps the function in a `std::packaged_task`, which acts as a bridge to a `std::future`.

```cpp
// Enqueue a complex lambda that returns a float
auto future = pool.Enqueue([](float a, float b) {
    return a * b;
}, 5.0f, 10.0f);

// Wait for the result asynchronously
float result = future.get(); // returns 50.0f
```

### 3. Graceful Shutdown
When `~ThreadPool` is called, it:
1. Locks the queue.
2. Sets `stop_ = true`.
3. Calls `condition_.notify_all()` to wake up any sleeping workers.
4. Calls `worker.join()` on every thread, ensuring all currently executing WAL writes or network RPCs complete safely, preventing data corruption during database shutdown.
