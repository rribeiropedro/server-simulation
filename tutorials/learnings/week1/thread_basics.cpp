#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

    std::string thread_id_to_string() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }

}

class Counter {
   private:
    std::mutex _mu;

   public:
    Counter() = default;

    void shared_printing(const std::string &id, std::uint64_t value) {
        std::lock_guard<std::mutex> lock(_mu);
        std::cout << id << ": " << value << '\n';
    }
};

void worker(Counter &counter) {
    for (std::uint64_t i = 0; i < 100; i++) {
        counter.shared_printing(thread_id_to_string(), i);
    }
}

int main() {
    Counter counter;
    std::thread t1(worker, std::ref(counter));
    std::thread t2(worker, std::ref(counter));
    std::thread t3(worker, std::ref(counter));
    std::thread t4(worker, std::ref(counter));

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    return 0;
}