#include "VirtualMemory.h"
#include "PhysicalMemory.h"

typedef struct recursionFrame
{
    uint64_t frame;  // frame number parent*p_size + offset
    uint64_t parent; // the parent of this frame
    bool unused; // is it a used frame?
    uint64_t depth;  // frame number
    uint64_t offset;
} recursionFrame;

typedef struct cyclicDistance
{
    uint64_t page_num;  // page_number - all the path
    uint64_t frame;  // the actual frame - parent*p_size + offset
    uint64_t parent;  // the parent of this frame
    uint64_t distance;  // the best cyclic distance
    uint64_t offset;
} cyclicDistance;

void clearMemory (uint64_t idx, bool clear_all);
uint64_t extractPhysicalAdd (uint64_t virtualAddress);

/**
 * Computes the cyclic distance between the currently evaluated page and the target page.
 * Updates the maximum cyclic distance structure if the current distance is greater.
 */
void cyclic_distance (cyclicDistance &max_cyclic, cyclicDistance cyclic_tmp,
                      uint64_t original_page)
{
  int ans = 0;      // Tracks the absolute difference between the pages
  uint64_t sub = 0; // Tracks the smaller cyclic distance (forward or backward)

  // Calculate the absolute difference between original_page and cyclic_tmp.page_num
  if ((int) (original_page - cyclic_tmp.page_num) < 0)
  {
    ans -= (int) (original_page - cyclic_tmp.page_num);
  }
  else
  {
    ans = (int) (original_page - cyclic_tmp.page_num);
  }

  // Determine the cyclic distance by comparing forward and backward distances
  if (NUM_PAGES - ans > ans)
  {
    sub = ans;
  }
  else
  {
    sub = NUM_PAGES - ans;
  }

  // If the current cyclic distance is greater than the tracked maximum, update max_cyclic
  if (sub > max_cyclic.distance)
  {
    max_cyclic.page_num = cyclic_tmp.page_num;
    max_cyclic.distance = sub;
    max_cyclic.frame = cyclic_tmp.frame;
    max_cyclic.parent = cyclic_tmp.parent;
    max_cyclic.offset = cyclic_tmp.offset;
  }
}

/**
 * Recursively traverses the hierarchical page table to:
 * 1. Find an unused frame.
 * 2. Track the maximum frame index.
 * 3. Identify eviction candidates using cyclic distance.
 */

recursionFrame tree_traversal (uint64_t cur_address, uint64_t parent, uint64_t
path, uint64_t cur_depth, cyclicDistance &max_cyclic_distance, uint64_t original_page, uint64_t &maximal_frame, uint64_t
                               offset, uint64_t old_frame)
{

  // Initialize the recursionFrame structure for the current frame
  recursionFrame current = {.frame=cur_address, .parent =parent,
      .unused=false, .depth = cur_depth, .offset= offset};

  // Base case: If at the last level of the page table
  if (cur_depth == TABLES_DEPTH)
  {
    // Create a cyclicDistance structure for eviction evaluation
    cyclicDistance tmp = {.page_num= path,
        .frame=cur_address, .parent=parent, .distance=0,
        .offset=current.offset};
    // Update the max_cyclic_distance with the current frame
    cyclic_distance (max_cyclic_distance, tmp, original_page);
    return current; // Return the current frame as it is the last level
  }

  // Assume the current frame is unused until proven otherwise
  current.unused = true;

  // Traverse all entries in the current table
  for (uint64_t j = 0; j < PAGE_SIZE; j++)
  {
    // Calculate the address of the current entry in the table
    auto new_add = cur_address * PAGE_SIZE + j;
    word_t value = 0;

    // Read the value stored at the current entry
    PMread (new_add, &value);

    if (value != 0)
    {
      // If the entry points to another frame, process it recursively
      current.unused = false; // The current frame is used

      // Accumulate the virtual path by appending the current index
      uint64_t add_path = (path << OFFSET_WIDTH) + j;

      // Recursively traverse the child frame
      recursionFrame child_frame = tree_traversal (value, cur_address,
                                                   add_path, cur_depth
                                                             + 1, max_cyclic_distance, original_page,
                                                   maximal_frame, j, old_frame);

      // Update the maximal frame index if necessary
      if (maximal_frame < child_frame.frame)
      {
        maximal_frame = child_frame.frame;
      }

      // If an unused frame is found and it is not the old frame, return it
      if (child_frame.unused && old_frame != child_frame.frame)
      {
        // Return the current frame if no unused frame is found in the subtree
        return child_frame;  // found empty frame
      }

    }
  }
  // Return the current frame if no unused frame is found in the subtree
  return current;
}

/**
 * Finds an available frame in physical memory, prioritizing:
 * 1. Reusing an unused frame.
 * 2. Allocating a new frame if available.
 * 3. Evicting a frame based on cyclic distance if necessary.
 */
uint64_t emptyFrame (uint64_t page_num, uint64_t old_frame)
{
  // Initialize tracking variables
  cyclicDistance min_cyclic_disance = {.page_num= 0, .frame=0, .parent=0,
      .distance=0, .offset=0};
  uint64_t maximal_frame = 0;

  // Traverse the page table to find unused frames or identify eviction candidates
  recursionFrame chosen_frame = tree_traversal
      (0, 0, 0, 0,
       min_cyclic_disance, page_num, maximal_frame, 0, old_frame);

  // Option 1: Use an unused frame
  if (chosen_frame.unused && chosen_frame.depth < TABLES_DEPTH &&
      chosen_frame.frame != old_frame)
  {
    // Unlink from parent
    PMwrite (chosen_frame.parent * PAGE_SIZE + chosen_frame.offset, 0);
    return chosen_frame.frame;
  }

  // Option 2: Allocate a new frame if available
  if (maximal_frame + 1 < NUM_FRAMES)
  {
    return maximal_frame + 1;
  }

  // Option 3: Evict a frame based on cyclic distance
  PMevict (min_cyclic_disance.frame, min_cyclic_disance.page_num);
  // Unlink from parent
  PMwrite (
      min_cyclic_disance.parent * PAGE_SIZE + min_cyclic_disance.offset, 0);
  return min_cyclic_disance.frame;
}

void VMinitialize ()
{
  // Clean the RAM
  clearMemory (0, true);
}

/**
 * Clears specified parts or the entire physical memory.
 */
void clearMemory (uint64_t idx, bool clear_all)
{
  // If clearing a specific frame only
  if (!clear_all)
  {
    // Write 0 to each word in the specified frame
    for (uint64_t i = 0; i < PAGE_SIZE; i++)
    {
      PMwrite (idx * PAGE_SIZE + i, 0);
    }
  }
  else
  {
    // clearing all frames in physical memory
    for (uint64_t i = 0; i < NUM_FRAMES; i++)
    {
      for (uint64_t j = 0; j < PAGE_SIZE; j++)
      {
        // Write 0 to each word in every frame
        PMwrite (i * PAGE_SIZE + j, 0);
      }
    }
  }
}

/**
 * Finds a new frame in physical memory for a virtual address.
 * If the current depth corresponds to a page frame, restores its content.
 * Otherwise, initializes it as a table frame.
 */
uint64_t findEmptyCell (uint64_t depth, uint64_t virtualAddress, uint64_t
address, uint64_t old_frame)
{
  auto page_num = virtualAddress >> OFFSET_WIDTH;   // Extract page number
  auto empty_frame = emptyFrame (page_num, old_frame);  // Empty Frame index

  if (TABLES_DEPTH - 1 == depth) // Final level (page frame)
  {
    PMrestore (empty_frame, page_num);  // Restore page content
    PMwrite (address, empty_frame);  // Update parent table
  }
  else // Intermediate level (table frame)
  {
    clearMemory (empty_frame, false); // Clear frame for table
    PMwrite (address, empty_frame);  // Update parent table
  }
  return empty_frame;  // Return the frame number
}

/**
 * Translates a virtual address into a physical address by traversing the
 * hierarchical page table structure.
 */
uint64_t extractPhysicalAdd (uint64_t virtualAddress)
{
  uint64_t parent = 0;          // Start at the root frame (frame 0)
  word_t temp_addr = 0;         // Temporary storage for the current frame
  uint64_t old_frame = 0;       // Keeps track of the previously accessed frame
  uint64_t offset =
      virtualAddress & (PAGE_SIZE - 1); // Extract the offset within the page

  // Traverse the page table hierarchy
  for (uint64_t i = 0; i < TABLES_DEPTH; i++)
  {
    // Calculate the index within the current table
    uint64_t idx = virtualAddress >> ((TABLES_DEPTH - i) * OFFSET_WIDTH);
    idx = idx & (PAGE_SIZE - 1);

    // Read the frame number at the current index
    PMread (parent + idx, &temp_addr);

    if (temp_addr == 0)
    { // Empty cell encountered

      temp_addr = findEmptyCell (i, virtualAddress, parent + idx, old_frame);

    }
    // Update the old frame tracker and move to the next level in the hierarchy
    old_frame = temp_addr;
    parent = PAGE_SIZE * temp_addr;
  }
  return temp_addr * PAGE_SIZE + offset;
}

int VMwrite (uint64_t virtualAddress, word_t value)
{
  // Check if the virtual address is valid
  if (VIRTUAL_MEMORY_SIZE <= virtualAddress)
  {
    return 0;
  }

  // Translate the virtual address to a physical address
  auto physical_add = extractPhysicalAdd (virtualAddress);

  // Read the value to the physical memory.
  PMwrite (physical_add, value);
  return 1;
}

int VMread (uint64_t virtualAddress, word_t *value)
{
  // Check if the virtual address is valid
  if (VIRTUAL_MEMORY_SIZE <= virtualAddress)
  {
    return 0; // Address is out of bounds, return failure
  }

  // Translate the virtual address to a physical address
  auto physical_add = extractPhysicalAdd (virtualAddress);

  // Read the value from the physical memory and store it in *value
  PMread (physical_add, value);
  return 1;
}