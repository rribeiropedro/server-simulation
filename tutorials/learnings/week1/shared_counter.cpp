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

class SharedCounter {
   private:
    std::mutex _mu;
    uint64_t _count;

   public:
    SharedCounter() {
        _count = 0;
    }

    void counter(const std::string &thread_id) {
        std::lock_guard<std::mutex> lock(_mu);
        std::cout << thread_id << ": " << _count << std::endl;
        _count++;
    }
};

void worker(SharedCounter &counter) {
    for (int i = 0; i < 100000; i++) {
        counter.counter(thread_id_to_string());
    }
}

int main() {
    SharedCounter counter;
    std::thread t1(worker, std::ref(counter));
    std::thread t2(worker, std::ref(counter));
    std::thread t3(worker, std::ref(counter));
    std::thread t4(worker, std::ref(counter));
    std::thread t5(worker, std::ref(counter));
    std::thread t6(worker, std::ref(counter));
    std::thread t7(worker, std::ref(counter));
    std::thread t8(worker, std::ref(counter));
    std::thread t9(worker, std::ref(counter));
    std::thread t10(worker, std::ref(counter));
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    t7.join();
    t8.join();
    t9.join();
    t10.join();
    return 0;
}