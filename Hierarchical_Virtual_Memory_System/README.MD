
# **Hierarchical Virtual Memory System (HVMS)**

## **Overview**
The **Hierarchical Virtual Memory System (HVMS)** simulates a virtual memory management system, featuring hierarchical page tables, efficient frame allocation, and a cyclic distance-based eviction policy. It implements core functionalities like address translation, frame allocation, and page swapping.

---

## **Features**
- **Hierarchical Page Tables**:
    - Multi-level page table structure for efficient virtual-to-physical address mapping.
- **Frame Allocation**:
    - Dynamically allocates frames for virtual memory pages, ensuring optimal memory utilization.
    - Allocates unused frames, or if no frames are available but the maximal frame index is less than the total frames, assigns a new frame.
- **Cyclic Distance Eviction**:
    - Employs a heuristic-based eviction policy to prioritize frames that are least likely to be reused soon.
- **Recursive Traversal**:
    - Navigates hierarchical tables to locate, allocate, or evict frames efficiently.
- **Validation**:
    - Ensures memory access is valid, preventing out-of-bound errors.

---

## **Project Structure**
```
.
├── VirtualMemory.h          # Header file defining the virtual memory API
├── VirtualMemory.cpp        # Implementation of virtual memory functions
├── PhysicalMemory.h         # Header file for low-level memory operations
├── PhysicalMemory.cpp       # Implementation of physical memory functions
├── README.md                # Documentation for the project
└── Makefile                 # Build script for compiling the project
```

---

## **How It Works**
### **Address Translation**
1. Virtual addresses are mapped to physical addresses using hierarchical page tables.
2. Resolves missing pages by allocating new frames or evicting existing ones.

### **Frame Allocation**
- Searches for unused frames.
- Allocates a new frame if the maximal frame index is less than the total available frames.
- Otherwise, employs a cyclic distance heuristic to evict a frame.

### **Eviction Policy**
- Uses cyclic distance to select frames for eviction, favoring those that are less likely to be reused.

---

## **Usage**
### **Compilation**
To compile the project, use the provided `Makefile`:
```bash
make
```

### **Execution**
Run the test program to validate virtual memory operations:
```bash
./VirtualMemoryTest
```

### **API Functions**
1. **`int VMread(uint64_t virtualAddress, word_t *value);`**
    - Reads the value from the specified virtual address.
    - Returns `1` on success, `0` on failure.

2. **`int VMwrite(uint64_t virtualAddress, word_t value);`**
    - Writes a value to the specified virtual address.
    - Returns `1` on success, `0` on failure.

3. **`void VMinitialize();`**
    - Initializes the virtual memory system by clearing all frames.

---

## **Implementation Details**
- **Recursive Traversal**:
    - `tree_traversal` recursively navigates hierarchical page tables to locate frames or identify eviction candidates.
- **Cyclic Distance**:
    - `cyclic_distance` calculates the distance between pages to prioritize eviction.
- **Memory Clearing**:
    - `clearMemory` ensures all frames are initialized before use.
- **Address Translation**:
    - `extractPhysicalAdd` resolves virtual addresses to physical addresses by traversing page tables.

---

## **Example Test**
Here’s an example of a test that writes to and reads from virtual memory:
```cpp
#include "VirtualMemory.h"
#include <cstdio>
#include <cassert>

int main(int argc, char **argv) {
    VMinitialize();

    // Write to virtual memory
    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        printf("writing to %llu\n", (long long int)i);
        VMwrite(5 * i * PAGE_SIZE, i);
    }

    // Read from virtual memory and verify correctness
    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        word_t value;
        VMread(5 * i * PAGE_SIZE, &value);
        printf("reading from %llu %d\n", (long long int)i, value);
        assert(uint64_t(value) == i);
    }

    printf("success\n");
    return 0;
}
```