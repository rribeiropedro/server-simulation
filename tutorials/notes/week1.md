## Week 1 Notes

### Process vs. Threads

---

- Multiprocessing: Each process only have one thread running, and all processes communicate with interprocess communications (files, pipes, message queues, etc.)
- Multithreading: A process can contain two or more threads and the threads communicate with each other through shared memory.

- Multithreading Pros:
    - Fast to start
    - Low overhead/easy to start
- Multiheading Cons:
    - Difficult to implement
    - Can't run on distributed system

People argue that multithreading shouldn't use shared memory as it does not allow the system to be a distributed system whenever necessary, but it does save a lot of overhead.

Something about the code below, int main() spawns a thread, so the program below has two threads, main and t1.
``` cpp
#include <iosteam>
#include <thread>
using namespace std;

void function_1() {
    std::cout << "Hello World" << std:: endl;
}

int main() {
    std::thread t1(function_1); // t1 starts running
    t1.join(); //main thread waits for t1 to finish
}

```

In the code below, t1 now runs seperately from main thread, main does not need to wait for the t1 thread to join before continuing. In the code below, nothing will print out as main thread will actually finish before t1.

``` cpp
#include <iosteam>
#include <thread>
using namespace std;

void function_1() {
    std::cout << "Hello World" << std:: endl;
}

int main() {
    std::thread t1(function_1); // t1 starts running
    t1.detach(); // t1 will freely on its own -- daemon process
}

```

NOTE, once thread is detached, it is FOREVER detached. Same goes to joining. To check the condition to be joinable:

``` cpp
if (t1.joinable())
    t1.join();
```


### Thread Management

---

Proper safe code makes sure that any potential code that has the possiblity of crashing after spanwing a thread is surrounded by a try catch statement, which means that it will force join the thread on error which is the safe way to do it.

``` cpp
#include <iosteam>
#include <thread>
using namespace std;

void function_1() {
    std::cout << "Hello World" << std:: endl;
}

int main() { // treat main as a regular parent function
    std::thread t1(function_1); // t1 starts running
    
    try { 
        throw std::runtime_error("error");
    } catch (...) {
        if (t1.joinable())
            t1.join();
    }

    return 0;
}

```

Side note before next example, a functor is C++ object/class that overlaods operator(), and is used instead of a normal function for:
    - Hold state in member variables
    - Behave like a function
    - Useful for algorithms, threads, callbacks, etc.

Example:
``` cpp
struct Multiplier {
    int factor;  // stored state
    Multiplier(int f) : factor(f) {}
    int operator()(int x) const {
        return x * factor;
    }
};

Multiplier times3(3);
Multiplier times10(10);

std::cout << times3(5);   // 15
std::cout << times10(5);  // 50
```

Now lets go back to talking about threads.
An alternative approach that is safer is RAII (Resource Acquisittion Is Initialization) for thread functions. The key idea of RAII is that a resource is acquired when an object is created and automatically released when the object is destroyed.

``` cpp
#include <iosteam>
#include <thread>
using namespace std;

// RAII
class Fctor {
    public:
        void operator()() {
            for (int i = 0; i < 100; i++) {
                std:: cout << "t1: " << i << enl;
            }
        }

}


int main() { // treat main as a regular parent function
    // code below doesthe same as passing Fctor fct;
    std::thread t1((Fctor())); // t1 starts running
    
    try { 
        for (int i = 0; i < 100; i++) {
            std:: cout << "main: " << i << enl;
        }
    } catch (...) {
        if (t1.joinable())
            t1.join();
    }

    return 0;
}

```

If you want to pass let's say a string by to the thread as a n argument, you can do so by the following line.
``` cpp
std::thread t1((Fctor(), s))
```

But let's say that now the void operator of Fctor is expecting a reference `string& s` to say copying time, a thread will still pass the value rathern than by reference.

If you really want to pass the string by reference, you must use
``` cpp
std::thread t1((Fctor(), std::ref(s)))
```

The following code snippet prints out:
"t1 says: Hello World"
"from main: Bye World"

``` cpp
class Fctor {
    public:
        void operator()(string &msg) {
            cout << "t1 says: " << msg << endl;
            msg = "Bye World";
        }
}

int main() { // treat main as a regular parent function
    string s = "Hello World";
    std::thread t1((Fctor()), std::ref(s));
    
    t1.join();
    cout << "from main: " << s << endl;

    return 0;
}
```

Another way to pass the parameter is to instead use `std::move(s)` which transfers the ownership of the string from the thread in main to t1 thread.

This works the same for threads, you CANNOT copy threads to other ones, you must pass ownership with the move as shows here `std::thread t2 = std::move(t1)`.

To check out the parent's id, you use `cout << std::this_thread::get_id() << endl;`, and to check out the child's id you use `cout << t1.get_id() << endl;`

Oversubscription:
    - Running more threads than CPU cores, degrades performance.

To avoid this, we can use `std::thread::hardware_councurrency();` to check out how much threads the current program can endure, a lot of times this ends up being the CPU cores.


### Race Condition and Mutex

---

Let's revisit a program from some time ago. The code snippet belows has both threads currently sharing the same resource, cout, which means they will be printing simultaneously to cout and out of order.

``` cpp
#include <iosteam>
#include <thread>
using namespace std;

void function_1() {
    for (int i = 0; i > -100; i--)
        cout << "From t1: " << i << endl;
}

int main() {
    std::thread t1(function_1); // t1 starts running
    
    for (int i = 0; i < 100; i++)
        cout << "From main: " << i << endl;
    
    t1.join();
    return 0;
}
```

To solve this, we use a `mutex`. Both threads call `shared_print()`, and inside that function the mutex is locked before printing and unlocked after printing. This ensures only one thread prints at a time, so the output does not get mixed together.

``` cpp
#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

std::mutex mu;

void shared_print(string msg, int id) {
    mu.lock();
    cout << msg << id << endl;
    mu.unlock();
}

void function_1() {
    for (int i = 0; i > -100; i--)
        shared_print(string("From t1: "), i)
}

int main() {
    std::thread t1(function_1); // t1 starts running
    
    for (int i = 0; i < 100; i++)
        shared_print(string("From main: "), i)
    
    t1.join();
    return 0;
}
```

Now the code snippet aboves isn't the best practice or the safest as in case of an error, it does not unlock the mutex, what we can do instead is use `std::lock_guard<std::mutex> guard(mu)` instead of using just `mu.lock()` and `mu.lock()` which isn't RAII.

Adding on to this `cout` is also not currently protected at all. This time, let us build a class that does protect everything about the shared print function.

``` cpp
class LogFile {
    std::mutex m_mutex;
    ofstream f;
public:
    LogFile() {
        f.open("log.txt");
    } // Need deconstructor to cloes the file, ignore for now
    void shared_print(string id, int value) {
        std::lock_guard<mutex> locker(m_mutex);
        f << "From " << id << ": " << value << endl;
    }
};

```

Now in the main and function 1, we can rewrite it to include this new class and everythingis now protected.

``` cpp
void function_1(LogFile& log) {
    for (int i = 0; i > -100; i--)
        log.shared_print(string("From t1: "), i);
}

int main() {
    LogFile log;
    std::thread t1(function_1, std::ref(log)); // t1 starts running
    
    for (int i = 0; i < 100; i++)
        log.shared_print(string("From main: "), i)
    
    t1.join();
    return 0;
}
```

Here are somethings we should NEVER do when creating classes such as LogFile():
    - NEVER return f to the outside world such as `ofstream& getStream() {return f;}` as user has f that is not mutex locked.
    - NEVER pass f as an argument to user provided function `void processf(void fun(ofstream&)) { fun(f); }`


### Deadlock

---

A deadlock is something that happens in concurrent programming where two or more threads are permanently stuck waiting for each other, so none of them can continue.

Below we have an example of how this can happend assuming there are two threads, A and B, with two mutexes, m1 and m2.
Example:
    - Thread A locks mutex1
    - Thread B locks mutex2
    - Thread A tries to lock mutex2 and waits
    - Thread B tries to lock mutex1 and waits

``` cpp
std::mutex m1, m2;

void threadA() {
    std::lock_guard<std::mutex> lock1(m1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> lock2(m2);
}

void threadB() {
    std::lock_guard<std::mutex> lock1(m2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> lock2(m1);
}
```

To avoid this, we need to make sure that everyone is locking the mutexes in the same order. We can use the following pattern to make sure everything is safe.

``` cpp
std::lock(_mu, _mu2);
std::lock_guard<mutex> locker(_mu, std::adopt_lock);
std::lock_guard<mutex> locker2(_mu2, std::adopt_lock);
```
Breakdown:
- `std::lock(_mu, _mu2)` locks both mutexes together using a deadlock algorithm
- `std::adopt_lock` tells the locker it is already locked, just take ownership and unlock it later

Avoiding Deadlock Tip:
- Prefer locking single mutex.
- Avoid locking a mutex and then calling a user provided function.
- Use std::lock() to lock more than one mutex.
- Lock the mutex in same order.

Locking Granurality:
- Fine-grained lock: protects small amount of data
- Coarse-grained lock: protects big amount of data.


### Unique_lock and Lazy Initialization

---

Let's revisit the following code

``` cpp
class LogFile {
    std::mutex _mu;
    ofstream _f;
public:
    LogFile() {
        f.open("log.txt");
    } // Need deconstructor to cloes the file, ignore for now
    void shared_print(string id, int value) {
        std::lock_guard<mutex> locker(_mu);
        _f << "From " << id << ": " << value << endl;
    }
};
```

We will be learning `std::unique_lock<mutex> locker(_mu)`. This type of lock gives us a lot more felxibility than the previous lock to lock andunlock whenever necessary. We can also transfer ownership of said lock to other other unique locks. The defer lock inside the unique lock just tells it to create it, but do not lock the mutex quite yet.

``` cpp
class LogFile {
    std::mutex _mu;
    ofstream _f;
public:
    LogFile() {
        _f.open("log.txt");
    }
    void shared_print(string id, int value) {
        std::unique_lock<mutex> locker(_mu, std::defer_lock);
        // do something else
        locker.lock();
        _f << "From " << id << ": " << value << endl;
        locker.unlock();
        // ...
        locker.lock();
        std::unique_lock<mutex> locker2 = std::move(locker);
    }
};
```

Lazy loading: 
A strategy we use to make sure that stuff we want to access, such as a file, is only created when itis going to be called upon, saving us loading time for these classes. In our case, we will be lazy loading the file we open.

``` cpp
class LogFile {
    std::mutex _mu;
    std::once_flag _flag;
    ofstream _f;
public:
    LogFile() {
    }
    void shared_print(string id, int value) {
        // lambda function starting from [&]
        std::call_once(_flag, [&](){ _f.open("log.txt"); });

        std::unique_lock<mutex> locker(_mu, std::defer_lock);
        // do something else
        locker.lock();
        _f << "From " << id << ": " << value << endl;
        locker.unlock();
        // ...
        locker.lock();
        std::unique_lock<mutex> locker2 = std::move(locker);
    }
};
```