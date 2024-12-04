# **User-Level Thread Library**

A **user-level thread library** implemented in C++ as part of the **67808 Operating Systems** course at the Hebrew University. This library provides core functionalities for thread management, including creation, scheduling, blocking, resuming, and sleeping, all while leveraging **signals** and **timers** for preemptive multitasking. 

**Note**: This library was specifically developed and tested on **Linux Operating Systems**.

---

## **Features**
- **Thread Management**:
    - Create, terminate, block, and resume threads.
    - Sleep threads for a specified number of quantums.
- **Scheduler**:
    - Timer-based preemptive scheduler using `SIGVTALRM`.
    - Context switching implemented with `sigsetjmp` and `siglongjmp`.
- **Resource Efficiency**:
    - Lightweight thread implementation with custom stacks.
    - Tracks thread states (e.g., `RUNNING`, `BLOCKED`, `READY`, `SLEEP`).

---

## **How It Works**
1. **Thread States**:
    - `RUNNING`: The thread currently using the CPU.
    - `READY`: The thread is ready to run and waiting in the queue.
    - `BLOCKED`: The thread is paused until explicitly resumed.
    - `SLEEP`: The thread is paused for a specified number of quantums.
2. **Context Switching**:
    - Saves and restores thread execution contexts using `sigsetjmp` and `siglongjmp`.
    - Ensures smooth transitions between threads without kernel involvement.
3. **Timer Integration**:
    - A virtual timer (`SIGVTALRM`) enforces quantum-based scheduling.

---

## **Installation**
1. Clone the repository:
   ```bash
   git clone https://github.com/SamaMilhem/67808-Operating-Systems.git
   cd 67808-Operating-Systems/Uthreads_Library
   ```
2. Compile the code:
   ```bash
   make
   ```

**Note**: Ensure you are using a **Linux-based OS** with GCC installed.

---

## **Usage**
### **API Overview**
- **Initialize the Library**:
  ```cpp
  int uthread_init(int quantum_usecs);
  ```
  Initialize the thread library with a quantum duration (in microseconds).

- **Create a Thread**:
  ```cpp
  int uthread_spawn(thread_entry_point entry_point);
  ```
  Create a new thread with the given entry function.

- **Terminate a Thread**:
  ```cpp
  int uthread_terminate(int tid);
  ```
  Terminate the thread with the given ID.

- **Block or Resume Threads**:
  ```cpp
  int uthread_block(int tid);
  int uthread_resume(int tid);
  ```

- **Sleep Threads**:
  ```cpp
  int uthread_sleep(int num_quantums);
  ```

- **Get Information**:
  ```cpp
  int uthread_get_tid();
  int uthread_get_total_quantums();
  int uthread_get_quantums(int tid);
  ```

---

## **Example**
Here’s an example demonstrating basic usage:

```cpp
#include "uthreads.h"
#include <iostream>

void thread_function() {
    for (int i = 0; i < 5; ++i) {
        std::cout << "Thread is running!" << std::endl;
        uthread_sleep(2);
    }
    uthread_terminate(uthread_get_tid());
}

int main() {
    uthread_init(100000); // 100ms quantum

    uthread_spawn(thread_function);

    while (true) {
        std::cout << "Main thread is running!" << std::endl;
        uthread_sleep(1);
    }

    return 0;
}
```

---

## **Project Structure**
```
.
├── uthreads.cpp       # Thread library implementation
├── uthreads.h         # Thread library header
├── Makefile           # Build script
└── tests.cpp          # Test cases
```

---

## **Known Limitations**
- **Main Thread Restrictions**:
    - The main thread cannot sleep or be blocked.
- **Thread Limit**:
    - Supports a maximum of `100` concurrent threads.
- **Preemption**:
    - Preemptive scheduling is based on quantum expiry.
- **Linux-Specific**:
    - This library uses `SIGVTALRM`, `sigsetjmp`, and `siglongjmp`, which are Linux-specific and may not work on other operating systems.

---

## **Acknowledgments**
This library was developed as part of the **67808 Operating Systems** course at the Hebrew University. Special thanks to the course instructors for their guidance and support.

---
