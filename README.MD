# **67808 Operating Systems Projects**

This repository contains four projects developed as part of the **67808 Operating Systems** course at the Hebrew University. Each project explores key aspects of operating systems, including threading, distributed computation, inter-process communication (IPC), and memory management.

---

## **Projects Overview**

### 1. **User-Level Thread Library**
A lightweight **user-level thread library** implemented in C++, supporting thread creation, scheduling, blocking, and sleeping. Built using signals (`SIGVTALRM`) and context switching (`sigsetjmp` and `siglongjmp`), it offers preemptive multitasking.

**Highlights**:
- Implements thread states: `RUNNING`, `BLOCKED`, `READY`, and `SLEEP`.
- Timer-based preemptive scheduling using `SIGVTALRM`.
- Efficient stack and context management.

**Repository**: [User-Level Thread Library](Uthreads_Library/README.md)

---

### 2. **MapReduce Framework**
A multi-threaded **MapReduce Framework** implemented in C++, designed to process data using Map, Shuffle, and Reduce phases. It supports multi-threaded execution and provides robust synchronization mechanisms.

**Highlights**:
- Fully multithreaded implementation with user-defined thread pools.
- Efficient synchronization with mutexes, atomic counters, and barriers.
- Example use case: Counting character occurrences in strings.

**Repository**: [MapReduce Framework](MapReduce_Framework/README.md)

---

### 3. **Multi-Server Communication System**
A Linux-based system for multi-server and client communication, leveraging **shared memory** and **socket communication**. It demonstrates inter-process communication (IPC) in a distributed environment.

**Highlights**:
- Implements shared memory and TCP socket communication.
- Categorizes servers into Host, Container, and VM types.
- Tracks and reports server statistics.

**Repository**: [Multi-Server Communication System](Linux_Multi_Server_Ipc/README.md)

---

### 4. **Hierarchical Virtual Memory System**
A virtual memory management simulation implementing **hierarchical page tables**, frame allocation, and cyclic distance-based eviction policies. It focuses on efficient address translation and memory management.

**Highlights**:
- Multi-level page table structure for virtual-to-physical mapping.
- Frame allocation with heuristic-based eviction.
- Handles page faults and recursive frame traversal.

**Repository**: [Hierarchical Virtual Memory System](Hierarchical_Virtual_Memory_System/README.md)

---

## **Repository Structure**

```
.
├── Hierarchical_Virtual_Memory_System  # Virtual Memory System implementation 
├── Linux_Multi_Server_Ipc              # Multi-Server Communication System
├── MapReduce_Framework                 # MapReduce Framework implementation 
└── Uthreads_Library                    # User-Level Thread Library
```

---

## **Acknowledgments**
These projects were completed as part of the **67808 Operating Systems** course at the Hebrew University. Special thanks to the course instructors for their guidance and feedback throughout the course.

---