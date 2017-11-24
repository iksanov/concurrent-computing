#include <atomic>
#include <thread>
#include <vector>

class peterson_mutex {
public:
    peterson_mutex();

    peterson_mutex(const peterson_mutex &other) = delete;

    peterson_mutex &operator=(const peterson_mutex &other) = delete;

    void lock(std::size_t id);

    void unlock(std::size_t id);

private:
    std::atomic_uint victim_;
    std::array<std::atomic_bool, 2> want_;

    std::size_t other(std::size_t id);
};

class TreeMutex {
public:
    TreeMutex(std::size_t n_threads);

    TreeMutex(const TreeMutex &) = delete;

    TreeMutex &operator=(const TreeMutex &) = delete;

    void lock(std::size_t id);

    void unlock(std::size_t id);

private:
    static std::size_t tree_size(std::size_t n_threads);

    static std::size_t get_bit(std::size_t n, std::size_t mask);

    std::vector<peterson_mutex> mutexes_;
};

peterson_mutex::peterson_mutex() {
    victim_.store(0);
    want_[0].store(false);
    want_[1].store(false);
}

void peterson_mutex::lock(std::size_t id) {
    want_[id].store(true);
    victim_.store(id);

    while (want_[other(id)].load() && victim_.load() == id)
        std::this_thread::yield();
}

void peterson_mutex::unlock(std::size_t id) {
    want_[id].store(false);
}

std::size_t peterson_mutex::other(std::size_t id) {
    return 1 - id;
}

TreeMutex::TreeMutex(std::size_t n_threads) : mutexes_(tree_size(n_threads)) {}

void TreeMutex::lock(std::size_t id) {
    std::size_t mutex_id = mutexes_.size() + id;

    do {
        std::size_t local_id = 1 - mutex_id % 2;
        mutex_id = (mutex_id - 1) / 2;
        mutexes_[mutex_id].lock(local_id);
    } while (mutex_id > 0);
}

void TreeMutex::unlock(std::size_t id) {
    // tree path is restored by bits of thread number
    std::size_t mask = (mutexes_.size() + 1) / 2; // higher bit of thread id
    std::size_t mutex_id = 0;

    while (mask) {
        mutexes_[mutex_id].unlock(get_bit(id, mask));
        mutex_id = mutex_id * 2 + 1 + get_bit(id, mask);
        mask /= 2;
    }
}

std::size_t TreeMutex::tree_size(std::size_t n_threads) {
    std::size_t size = 2;

    while (size < n_threads)
        size *= 2;

    return size - 1;
}

std::size_t TreeMutex::get_bit(std::size_t n, std::size_t mask) {
    return n & mask ? 1 : 0;
}
