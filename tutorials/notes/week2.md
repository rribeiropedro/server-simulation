## Week 2 Notes

### Conditional Variables

The code below has two functions, `function_1` populates a queue and makes sure that it locks the mutex every time it touches the queue, and `function_2` pops from the queue. The issue right now is that we don't know what to do in `function_2` with the if else statement, as locking and unlocking the locker repeatedly wastes a lot of resources when it is not necessary here.

```cpp
#include <iostream>
#include <thread>
using namespace std;

std::deque<int> q;
std::mutex mu;

void function_1 () {
    int count = 10;
    while (count > 0) {
        std::unique_lock<mutex> locker(mu);
        q.push_front(count);
        locker.unlock();
        std::this_thread::sleep_for(chrono::seconds(1));
        count --;
    }
}

void function_2 () {
    int data = 0;
    while (data != 1) {
        std::unique_lock<mutex> locker(mu);
        if (!q.empty()) {
            data = q.back();
            q.pop_back();
            locker.unlock();
            count << "t2 got a value from t1: " << data << endl;
        } else {
            locker.unlock();
        }
    }
}
```

To fix this issue, we can use the `std::condition_variable`. What conditional variabels do is signalize a locker/other tools when you may use said resources. So for our case, we can notify funciton 2 using function 1 to unlock the locker and access the data inside the queue.

Code snippets:
```cpp
while (count > 0) {
    std::unique_lock<mutex> locker(mu);
    q.push_front(count);
    locker.unlock();
    cond.notify_one();
    std::this_thread::sleep_for(chrono::seconds(1));
    count --;
}
```

```cpp
while (data != 1) {
    std::unique_lock<mutex> locker(mu);
    cond.wait(locker)
    data = q.back();
    q.pop_back();
    locker.unlock();
    count << "t2 got a value from t1: " << data << endl;
}
```

In summary, function 2 can only fetch after function 1 tell it is valid.

Spurious wake is a case when a locker decides to wake up by itself for OS reasons. Cond wait can be implemented two ways, the first is to add a while loop around the wait.

```cpp
while (!q.empty()) {
    cond.wait(lockers)
}
```

Or you can use overload with the lambda function.

```cpp
cond.wait(locker, [&] { return !q.empty(); })
```

Additionally, many threads can be depending on the conditional variable, which is why we have access to both `cond.notify_one()` and `cond.notify_all()` to pick and choose how many we want to notify.