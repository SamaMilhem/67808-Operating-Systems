#pragma once

#include "MemoryConstants.h"

/**
 * Initialize the virtual memory.
 */
void VMinitialize ();

/**
 * Reads a word from the given virtual address and stores its value in the provided pointer.
 *
 * @param virtualAddress The virtual address to read from.
 * @param value A pointer to store the word read from the given virtual address.
 * @return int Returns 1 on success, or 0 if the address cannot be mapped to a physical
 * address for any reason
 */

int VMread (uint64_t virtualAddress, word_t *value);

/**
 * Writes a value to a specific virtual address in the virtual memory system.
 * The function validates the virtual address, translates it into a physical address,
 * and writes the specified value to the resolved physical address.
 *
 * @param virtualAddress The virtual address where the value should be written.
 * @param value The value to write to the virtual address.
 * @return int Returns 1 on success, or 0 on failure.
 */
int VMwrite (uint64_t virtualAddress, word_t value);

