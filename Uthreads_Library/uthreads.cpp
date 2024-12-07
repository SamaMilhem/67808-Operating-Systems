#include "uthreads.h"
#include <iostream>
#include <map>
#include <queue>
#include <list>
#include <signal.h>
#include <sys/time.h>
#include "vector"
#include "cmath"
#include <setjmp.h>

using namespace std;

// Constants and error messages
#define SUCCESS 0
#define FAILURE 1
#define FAILED_LIB (-1)
#define MILLION 1000000
#define SIGACTION_ERR "system error: sigaction error\n"
#define SET_TIMER_ERR "system error: set timer error\n"
#define QUANTUM_ERR "thread library error: non-positive quantum_usecs\n"
#define ENTRY_PTR_ERR "thread library error: null entry_point\n"
#define MAX_THR_ERR "thread library error: number of concurrent threads to exceed "\
            "the limit\n"
#define TID_ERR "thread library error: no thread with ID tid exists\n"
#define BLOCK_MAIN_ERR "thread library error: tried to block the main thread\n"
#define SLEEP_ERR "thread library error: main thread called uthread_sleep\n"

#define NEGATIVE_SLEEP_ERR "thread library error: non positive quantums number - uthread_sleep\n"

#define  TERMINATION_ERR "thread library error: the thread had been terminated"

// Platform-dependent definitions for context manipulation
#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

// Translate virtual to physical address
address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

// Translate virtual to physical address
address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%gs:0x18,%0\n"
               "rol    $0x9,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}
#endif

sigset_t maskedSignals;
// Enum representing thread states
enum STATE
{
    RUNNING, BLOCKED, READY, SLEEP, SLEEP_BLOCK
};

// Thread structure
typedef struct
{
    STATE state;                // Current state of the thread
    int id;                     // Unique thread ID
    int quantum;                // Number of quantums consumed
    address_t pc;               // Program counter
    address_t sp;               // Stack pointer
    char *stack;                // Stack memory
    thread_entry_point entry_point; // Thread entry point function
    sigjmp_buf env;             // Thread execution environment
    int sleep_quantum;          // Remaining sleep quantums
    bool terminated;            // Termination flag
} thread;

// Global data structures
map<int, thread *> sleeping_threads;
map<int, thread *> threads;
queue<thread *> ready_queue;
vector<thread *> blocked_threads;
map<int, thread *> termination;
thread *running_thread;
int quantums_number = 0;
struct sigaction sa;
struct itimerval timer;
queue<thread *> temp_q;
queue<thread *> to_delete_threads;
vector<int> waking_up_threads;

int threads_cur_number;

void block ()
{
  /*** Block signals (SIGVTALRM) to prevent interruptions */
  if (sigprocmask (SIG_BLOCK, &maskedSignals, nullptr) == SUCCESS)
  {
    return;
  }
  exit (FAILURE);
}

void unblock ()
{
  /***  Unblock signals (SIGVTALRM) to allow normal scheduling */
  if (sigprocmask (SIG_UNBLOCK, &maskedSignals, nullptr) == SUCCESS)
  {
    return;
  }
  exit (FAILURE);
}

void delete_thread (thread *to_delete)
{
  /*** Delete a thread and clean up its resources */
  block ();

  // Remove thread from blocked_threads / deleting sleep_blocked threads
  int counter = 0;
  for (auto it: blocked_threads)
  {
    if (it->id == to_delete->id)
    {
      blocked_threads.erase (blocked_threads.begin () + counter);
      break;
    }
    counter += 1;
  }
  // Remove thread from sleeping_threads / deleting sleep_blocked threads
  if (to_delete->state == SLEEP || to_delete->state == SLEEP_BLOCK)
  {
    sleeping_threads.erase (to_delete->id);
  }

  if (to_delete->state == READY)
  {
    // Remove thread from ready_queue
    while (!ready_queue.empty ())
    {
      thread *current_item = ready_queue.front ();
      ready_queue.pop ();
      if (current_item->id != to_delete->id)
      {
        to_delete_threads.push (current_item);
      }
    }
    while (!to_delete_threads.empty ())
    {
      ready_queue.push (to_delete_threads.front ());
      to_delete_threads.pop ();
    }
  }

  // Clean up thread resources
  if (to_delete->id != 0)
  {
    // delete the thread's stack
    delete[] to_delete->stack;
    if (!to_delete->terminated)
    {
      // Remove the thread from the threads map
      threads.erase (to_delete->id);
      threads_cur_number--;
    }
    delete to_delete; // Delete the thread pointer
  }
  unblock ();

}

void schedule_handler (int)
{
  /*** Schedule and switch to the next thread */
  block ();
  if (!running_thread->terminated)
  {
    // Save the running thread env
    if (sigsetjmp (running_thread->env, 1) == FAILURE)
    {
      unblock ();
      return;
    }
  }

  for (auto iter: sleeping_threads)
  {
    // Update the remaining sleep quantums
    iter.second->sleep_quantum -= 1;

    // Handle threads wake-up
    if (iter.second->sleep_quantum == 0)
    {
      // Pay attention to a possibility that the thread is also blocked
      if (iter.second->state == SLEEP_BLOCK)
      {
        iter.second->state = BLOCKED;
      }
      else     // Get the thread back to the ready queue
      {
        iter.second->state = READY;
        ready_queue.push (iter.second);
      }
      waking_up_threads.push_back (iter.second->id);
    }
  }
  // Delete the thread from the sleeping threads map
  for (auto iter: waking_up_threads)
  {
    sleeping_threads.erase (iter);
  }
  waking_up_threads.clear ();

  // Delete the threads that were marked as terminated
  for (auto it: termination)
  {
    delete_thread (it.second);
  }
  termination.clear ();

  // Handle a case where the running thread was terminated
  if (running_thread->terminated)
  {
    int terminated_tid = running_thread->id;
    termination.insert ({terminated_tid,
                         threads.find (terminated_tid)->second});
    threads.erase (terminated_tid);
    threads_cur_number--;
  }

  // quantum expired
  if (!running_thread->terminated && running_thread->state != SLEEP
      && running_thread->state != BLOCKED &&
      running_thread->state != SLEEP_BLOCK)
  {
    // put the running thread back in the ready queue and update it's state
    if (!ready_queue.empty ())
    {
      running_thread->state = READY;
      int s = running_thread->id;
      ready_queue.push (threads.find (s)->second);
    }
    else
    { //  the ready queue is empty, don't switch
      running_thread->quantum += 1;
      quantums_number += 1;
      unblock ();
      return;
    }
  }

  running_thread = ready_queue.front ();
  ready_queue.pop ();
  running_thread->quantum += 1;
  running_thread->state = RUNNING;
  quantums_number += 1;
  unblock ();
  siglongjmp (running_thread->env, 1);
  unblock ();
}

int setTimer (int tv_sec, int tv_usec)
{
  /***  Set up the timer and associate it with SIGVTALRM */
  block ();
  sa = {nullptr};
  // Install timer_handler as the signal handler for SIGVTALRM.
  sa.sa_handler = &schedule_handler;
  if (sigaction (SIGVTALRM, &sa, nullptr) < 0)
  {
    std::cerr << SIGACTION_ERR;
    exit (FAILURE);
  }
  // Configure the timer to expire after quantum usec... */
  timer.it_value.tv_sec = tv_sec;        //Time interval,seconds part
  timer.it_value.tv_usec = tv_usec;        //Time interval,microseconds part

  // Configure the timer to expire every quantum usec after that.
  timer.it_interval.tv_sec = tv_sec;    // Following time intervals, seconds
  // part
  timer.it_interval.tv_usec = tv_usec;    // Following time intervals,
  // microseconds part

  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer (ITIMER_VIRTUAL, &timer, nullptr))
  {
    std::cerr << SET_TIMER_ERR;
    exit (FAILURE);
  }
  unblock ();
  return SUCCESS;
}

void addthread (thread *to_add)
{
  /*** Adds the new thread to the threads map and gives it the minimal
   * possible id
  */

  block ();
  for (int i = 0; i < MAX_THREAD_NUM; i++)
  {
    if (threads.find (i) == threads.end ())
    {
      to_add->id = i; // set id
      threads.insert ({i, to_add}); // Insert to the threads map
      threads_cur_number++;
      unblock ();
      return;
    }
  }
  unblock ();
  std::cerr << MAX_THR_ERR; // Reached maximal threads number limit
  exit (FAILURE);
}

int uthread_init (int quantum_usecs)
{
  /***Initializes the thread library*/
  if (quantum_usecs <= 0) // Negative quantum size
  {
    std::cerr << QUANTUM_ERR;
    return FAILED_LIB;
  }
  auto *main_thread = new thread{.state = RUNNING, .id = 0, .quantum = 1,
      .pc = 0, .sp = 0, .stack = nullptr, .entry_point = nullptr, .sleep_quantum = 0, .terminated = false};
  quantums_number = 1;
  addthread (main_thread);
  running_thread = main_thread;
  sigsetjmp (main_thread->env, 1); // Save the thread env
  // Update the pc
  (main_thread->env->__jmpbuf)[JB_PC] = translate_address (main_thread->pc);
  // update the sp
  (main_thread->env->__jmpbuf)[JB_SP] = translate_address (main_thread->sp);
  sigemptyset (&main_thread->env->__saved_mask);
  sigemptyset (&maskedSignals); // clear all signals from the signal set
  sigaddset (&maskedSignals, SIGVTALRM); // add SIGVTALRM to the signal set

  // Install timer_handler as the signal handler for SIGVTALRM.
  return setTimer (quantum_usecs / MILLION,
                   quantum_usecs % MILLION);
}

int uthread_spawn (thread_entry_point entry_point)
{
  /*** initializes a new thread, allocates its stack, sets up its context,
   * and adds it to the thread library's data structures for scheduling. */

  // Block signals to ensure thread creation is atomic and uninterrupted.
  block ();
  // Check if the entry_point is null.
  if (entry_point == nullptr)
  {
    std::cerr << ENTRY_PTR_ERR;
    unblock ();
    return FAILED_LIB;
  }

  // Ensure the total number of threads does not exceed the maximum allowed.
  if (threads_cur_number >= MAX_THREAD_NUM)
  {
    std::cerr << MAX_THR_ERR;
    unblock ();
    return FAILED_LIB;
  }

  // Allocate memory for the thread's stack.
  char *stack = new char[STACK_SIZE];

  // Initialize the stack pointer to the top of the allocated stack.
  address_t sp =
      (address_t) stack + STACK_SIZE - sizeof (address_t);

  // Set the program counter to the thread's entry point function.
  auto pc = (address_t) entry_point;

  // Create and initialize the new thread object.
  auto *new_thread = new thread{.state = READY, .id = -1, .quantum = 0, .pc = pc, .sp =
  sp, .stack = stack, .entry_point= entry_point, .sleep_quantum = 0, .terminated = false};

  // Add the thread to the threads map and assign it a unique ID.
  addthread (new_thread);

  // Add the thread to the ready queue for scheduling.
  ready_queue.push (new_thread);

  // Save the thread's execution context.
  sigsetjmp (new_thread->env, 1);

  // Update the program counter and stack pointer in the context.
  (new_thread->env->__jmpbuf)[JB_PC] = translate_address (new_thread->pc);
  (new_thread->env->__jmpbuf)[JB_SP] = translate_address (new_thread->sp);

  // Clear the signal mask in the thread's context
  sigemptyset (&new_thread->env->__saved_mask);
  unblock ();

  // Return the unique thread ID of the newly created thread.
  return new_thread->id;
}

int uthread_terminate (int tid)
{
  /*** Terminate the thread with the given ID, cleans up its resources,
    * and removes it from all relevant thread data structures*/

  // Block signals to ensure thread termination is atomic and uninterrupted.
  block ();

  // Validate the thread ID.
  if (threads.find (tid) == threads.end ())
  {
    std::cerr << TID_ERR;
    unblock ();
    return FAILED_LIB;
  }

  // Retrieve the thread object from the threads map.
  thread *to_terminate = threads.find (tid)->second;

  // Check if the thread to terminate is not the main thread.
  if (to_terminate->id != 0)
  {
    if (to_terminate->state == RUNNING)
    {
      // handle running thread termination
      to_terminate->terminated = true;
      unblock ();
      sa.sa_handler (0); // Trigger a context switch by calling the scheduler.
      return SUCCESS;
    }
    else
    {
      // If the thread is not running, delete its resources directly.
      delete_thread (to_terminate);
      unblock ();
      return SUCCESS;
    }
  }

  // main thread was terminated - Iterate through all threads and clean up their resources.
  for (auto it: threads)
  {
    if (it.second->id != 0)
    {
      // Delete the allocated stack for non-main threads.
      delete[] it.second->stack;
    }
    // Delete the thread object.
    delete it.second;
    it.second = nullptr;
  }
  // Clear the threads map to remove all entries.
  threads.clear ();
  exit (SUCCESS);
}

int uthread_block (int tid)
{
  /*** from being scheduled until it is explicitly resumed.*/
  // Block signals to ensure atomicity during thread state modification.
  block ();

  // Validate the thread ID.
  if (threads.find (tid) == threads.end ())
  {
    std::cerr << TID_ERR;
    unblock ();
    return FAILED_LIB;
  }
  // Prevent blocking the main thread.
  if (tid == 0)
  {
    std::cerr << BLOCK_MAIN_ERR;
    unblock ();
    return FAILED_LIB;
  }

  // Retrieve the thread object.
  thread *to_block = threads.find (tid)->second;

  // Case 1: If the thread is RUNNING.
  if (to_block->state == RUNNING)
  {
    to_block->state = BLOCKED;
    blocked_threads.push_back (to_block);
    // Unblock signals and trigger a context switch by calling the scheduler.
    unblock ();
    sa.sa_handler (0);
    return SUCCESS;
  }

  // Case 2: If the thread is SLEEPING.
  if (to_block->state == SLEEP)
  {
    // Change the state to SLEEP_BLOCK to indicate the thread is both sleeping and blocked.
    to_block->state = SLEEP_BLOCK;
    blocked_threads.push_back (to_block);
  }

  // Case 3: If the thread is READY.
  if (to_block->state == READY)
  {
    // Mark the thread as BLOCKED and remove it from the ready_queue.
    to_block->state = BLOCKED;
    while (!ready_queue.empty ())
    {
      thread *current_item = ready_queue.front ();
      ready_queue.pop ();
      if (current_item->id != tid)
      {
        temp_q.push (current_item);
      }
      else
      {
        blocked_threads.push_back (current_item);
      }
    }

    // Restore all other threads back into the ready_queue.
    while (!temp_q.empty ())
    {
      ready_queue.push (temp_q.front ());
      temp_q.pop ();
    }
  }

  unblock ();
  return SUCCESS;
}

int uthread_resume (int tid)
{
  /*** This function transitions a thread from the BLOCKED or SLEEP_BLOCK state
    * back to the READY or SLEEP state, making it eligible for scheduling if
    * not sleeping.
 */

  // Block signals to ensure thread state modifications are atomic.
  block ();

  // Check if the thread is in the termination map (already terminated).
  if (termination.find (tid) != termination.end ())
  {
    std::cerr << TERMINATION_ERR;
    unblock ();
    return FAILED_LIB;
  }

  // Validate the thread ID.
  if (threads.find (tid) == threads.end ())
  {
    std::cerr << TID_ERR; // unknown id
    unblock ();
    return FAILED_LIB;
  }

  // Iterate through the blocked_threads list to find the target thread.
  int i;
  bool found = false;
  for (i = 0; i < (int) blocked_threads.size (); i++)
  {
    // Found the thread in the BLOCKED state.
    if (blocked_threads.at (i)->id == tid)
    {
      thread *blocked_thread = blocked_threads.at (i);

      // Check if the thread is also in the SLEEP_BLOCK state.
      if (threads.find (tid)->second->state == SLEEP_BLOCK)
      {
        blocked_thread->state = SLEEP;
      }
      else
      {
        // Transition the thread to the READY state.
        blocked_thread->state = READY;
        ready_queue.push (blocked_thread);
      }
      found = true; // Mark the thread as found and processed.
      break;
    }
  }

  // If the thread was found, remove it from the blocked_threads list.
  if (found)
  {
    blocked_threads.erase (blocked_threads.begin ()
                           + i);
  }
  unblock ();
  return SUCCESS;
}

int uthread_sleep (int num_quantums)
{
  /***This function transitions the currently running thread into the SLEEP state,
    * preventing it from being scheduled until the specified number of quantums
    * have elapsed.
   */

  // Block signals to ensure thread state modifications are atomic.
  block ();

  // Ensure that the main thread is not attempting to sleep.
  if (running_thread->id == 0)
  {
    std::cerr << SLEEP_ERR;
    unblock ();
    return FAILED_LIB;
  }

  // Validate that the sleep duration is positive.
  if (num_quantums <= 0)
  {
    std::cerr << NEGATIVE_SLEEP_ERR;  // negative num of quantums
    unblock ();
    return FAILED_LIB;
  }

  // Get the ID and pointer of the currently running thread.
  int id = running_thread->id;
  thread *u_thread = threads.find (id)->second;
  // Insert the thread pointer to the sleeping_threads map
  sleeping_threads.insert ({id,u_thread});

  // Update the thread's state to SLEEP and set the remaining sleep quantums.
  running_thread->state = SLEEP;
  running_thread->sleep_quantum =num_quantums + 1;
  unblock ();
  // Trigger a context switch by invoking the scheduler via the signal handler.
  sa.sa_handler (0);
  return SUCCESS;
}

int uthread_get_tid ()
{
  return running_thread->id;
}

int uthread_get_total_quantums ()
{
  return quantums_number;
}

int uthread_get_quantums (int tid)
{
  block ();
  if (threads.find (tid) != threads.end ())
  {
    // Get the thread's quantum
    thread *to_get = threads.find (tid)->second;
    unblock ();
    return to_get->quantum;
  }
  std::cerr << TID_ERR;
  unblock ();
  return FAILED_LIB;
}

