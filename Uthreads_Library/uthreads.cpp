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


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
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


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
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

enum STATE {RUNNING, BLOCKED, READY, SLEEP, SLEEP_BLOCK};

typedef struct{
    STATE state;
    int id;
    int quantum;
    address_t pc;
    address_t sp;
    char* stack;
    thread_entry_point entry_point;
    sigjmp_buf env;
    int sleep_quantum;
    bool terminated;

} thread;

map <int, thread*> sleeping_threads;
map <int, thread*> threads;
queue<thread*> ready_queue;
vector<thread*> blocked_threads;
map <int, thread*> termination;
thread* running_thread ;
int quantums_number = 0;
struct sigaction sa;
struct itimerval timer;
queue<thread*> temp_q ;
queue<thread*> thread_deleting;
vector <int> ids;

int threads_cur_number;

void block(){
    if (sigprocmask(SIG_BLOCK, &maskedSignals, nullptr) == SUCCESS){
        return;
    }
    exit(FAILURE);
}
void unblock(){
    if (sigprocmask(SIG_UNBLOCK, &maskedSignals, nullptr) == SUCCESS){
        return;
    }
    exit(FAILURE);
}

void delete_thread(thread* to_delete){ 
    block();
    int counter = 0;
    for (auto it : blocked_threads) 
    { // deleting a blocked thread or sleep_blocked thread
        if(it->id == to_delete->id){
            blocked_threads.erase(blocked_threads.begin() + counter);
            break;
        }
        counter += 1;
    }
    if (to_delete->state == SLEEP || to_delete->state == SLEEP_BLOCK){
        // deleting a slept thread or sleep_blocked thread
        sleeping_threads.erase(to_delete->id);
    }

    if (to_delete->state == READY) {
        // removing the deleted thread from the ready queue
        while (!ready_queue.empty()) {
            thread *current_item = ready_queue.front();
            ready_queue.pop();
            if (current_item->id != to_delete->id) {
                thread_deleting.push(current_item);
            }
        }
        while (!thread_deleting.empty()) { 
            ready_queue.push(thread_deleting.front());
            thread_deleting.pop();
        }
    }

    if (to_delete->id != 0) {
       delete[] to_delete->stack; // delete the thread's stack
        if (!to_delete->terminated) {
            threads.erase(to_delete->id); // remove the thread from the threads map
            threads_cur_number--;
        }
        delete to_delete; // delete the thread pointer
    }
    unblock();

}

void schedule_handler(int){
    block();
   if (!running_thread->terminated) { // check if running thread is terminated
       if (sigsetjmp(running_thread->env, 1) == FAILURE) {
           unblock();
           return;
       }
   }

//    }
    for (auto iter : sleeping_threads){
        iter.second->sleep_quantum -= 1; // change the remaining sleep quantums

        if (iter.second->sleep_quantum == 0){ // remaining sleep quantums is 0
            if (iter.second->state == SLEEP_BLOCK){
                iter.second->state = BLOCKED; // pay attention to a possibility that the thread is also blocked
            }
            else {
                iter.second->state = READY; // get the thread back to the ready queue
                ready_queue.push(iter.second);
            }
            ids.push_back(iter.second->id);
        }
    }
    for (auto iter : ids) {
        sleeping_threads.erase(iter); // delete the thread from the sleeping threads map
    }
    ids.clear();
    for (auto it: termination) { 
        delete_thread(it.second);  // delete the threads that were marked as terminated
    }
    termination.clear();
    
    if (running_thread->terminated){ // mark the running thread as terminated
        int s = running_thread->id;
        termination.insert({s, threads.find(s)->second});
        threads.erase(s);
        threads_cur_number--;
    }
    if (!running_thread->terminated && running_thread->state != SLEEP && running_thread->state != BLOCKED &&
    running_thread->state != SLEEP_BLOCK && !ready_queue.empty()){
        // quantum expired
        running_thread->state = READY;
        int s = running_thread->id;
        ready_queue.push(threads.find(s)->second);
    }
    bool flag = false;
    if (!running_thread->terminated && running_thread->state != SLEEP && running_thread->state != BLOCKED && 
    ready_queue.empty() && running_thread->state != SLEEP_BLOCK){
        //  the ready queue is empty
        running_thread->quantum += 1;
        quantums_number += 1;
        flag = true;
    }
    if (!flag) {  // switch threads
        running_thread = ready_queue.front();
        ready_queue.pop();
        running_thread->quantum += 1;
        running_thread->state = RUNNING;
        quantums_number += 1;
        unblock();
        siglongjmp(running_thread->env, 1);
    }
    unblock();
}

int setTimer(int tv_sec, int tv_usec){
    block();
    sa = {nullptr};
    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &schedule_handler;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr <<SIGACTION_ERR;
        exit(FAILURE);
    }
    // Configure the timer to expire after quantum usec... */
    timer.it_value.tv_sec =tv_sec;        //time interval,seconds part
    timer.it_value.tv_usec = tv_usec;        //time interval,microseconds part

    // configure the timer to expire every quantum usec after that.
    timer.it_interval.tv_sec = tv_sec;    // following time intervals, seconds part
    timer.it_interval.tv_usec = tv_usec;    // following time intervals, microseconds part

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << SET_TIMER_ERR;
        exit(FAILURE);
    }
    unblock();
    return SUCCESS;
}


void addthread(thread* to_add){
    // adds the new thread to the threads map and gives it the minimal possible id 
    block();
    for (int i=0; i< MAX_THREAD_NUM; i++){
        if (threads.find (i) == threads.end()){
            to_add->id = i; // set id
            threads.insert ({i, to_add}); // insert to the threads map
            threads_cur_number ++;
            unblock();
            return;
        }
    }
    unblock();
    std::cerr << MAX_THR_ERR; // reached num of threads limit
    exit(FAILURE);
}

int uthread_init(int quantum_usecs){

    if (quantum_usecs <= 0){ // negative quantum size
        std::cerr << QUANTUM_ERR;
        return FAILED_LIB;
    }
    auto* main_thread = new thread {.state = RUNNING ,.id = 0, .quantum = 1,
            .pc = 0, .sp = 0 , .stack = nullptr ,.entry_point = nullptr, .sleep_quantum = 0,.terminated = false};
    quantums_number = 1;
    addthread(main_thread); 
    running_thread = main_thread;
    sigsetjmp(main_thread->env, 1); // save the thread env
    (main_thread->env->__jmpbuf)[JB_PC] = translate_address(main_thread->pc); // update pc
    (main_thread->env->__jmpbuf)[JB_SP] = translate_address(main_thread->sp); // update sp
    sigemptyset(&main_thread->env->__saved_mask);
    sigemptyset(&maskedSignals); // blocked signals set
    sigaddset(&maskedSignals, SIGVTALRM);

    // Install timer_handler as the signal handler for SIGVTALRM.
    return setTimer(quantum_usecs / MILLION,
                    quantum_usecs % MILLION);
}

int uthread_spawn(thread_entry_point entry_point)
{
    block();
    if (entry_point == nullptr){ // error - entry point is null
        std::cerr << ENTRY_PTR_ERR;
        unblock();
        return FAILED_LIB;
    }
    if (threads_cur_number >= MAX_THREAD_NUM){
        std::cerr << MAX_THR_ERR; // reached num of threads limit
        unblock();
        return FAILED_LIB;
    }
    char* stack = new char [STACK_SIZE]; // init the thread's stack
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t); // init sp
    auto pc = (address_t) entry_point; // init pc
    auto* new_thread = new thread {.state = READY ,.id = -1, .quantum = 0,.pc = pc, .sp =
    sp , .stack = stack ,.entry_point= entry_point, .sleep_quantum = 0, .terminated = false}; // create thread pointer
    addthread (new_thread);
    ready_queue.push (new_thread); 
    // set the thread's env and save it
    sigsetjmp(new_thread->env, 1); 
    (new_thread->env->__jmpbuf)[JB_PC] = translate_address(new_thread->pc);
    (new_thread->env->__jmpbuf)[JB_SP] = translate_address(new_thread->sp);
    sigemptyset(&new_thread->env->__saved_mask);
    unblock();
    return new_thread->id;
}

int uthread_terminate(int tid){
    block();
    if (threads.find (tid) == threads.end()){ // unknown id error
        std::cerr << TID_ERR;
        unblock();
        return FAILED_LIB;
    }
    thread* to_terminate = threads.find (tid)->second;
    if (to_terminate->id != 0){
        if (to_terminate->state == RUNNING) {
            // handle running thread termination
            to_terminate->terminated = true;
            unblock();
            sa.sa_handler(0);
            return SUCCESS;
        }
        else{
            delete_thread (to_terminate);
            unblock();
            return SUCCESS;
        }
    }
    // main thread was terminated - delete all the threads
    for (auto it : threads)
    {
        if (it.second->id != 0) {
                delete[] it.second->stack;
        }
        delete it.second;
        it.second = nullptr;
    }
    threads.clear();
    exit(SUCCESS);
}

int uthread_block(int tid){ 
    block();
    if (threads.find (tid) == threads.end()){ // unknown id
        std::cerr << TID_ERR;
        unblock();
        return FAILED_LIB;
    }
    if (tid == 0){ // can't block the main thread
        std::cerr << BLOCK_MAIN_ERR;
        unblock();
        return FAILED_LIB;
    }

    thread* to_block = threads.find (tid)->second; // find the thread to block
    if (to_block->state == RUNNING){
        // handle the case of blocking a running thread
        to_block->state = BLOCKED;
        blocked_threads.push_back(to_block);
        unblock();
        sa.sa_handler(0);
        return SUCCESS;
    }
    if (to_block->state == SLEEP){
        // blocking a sleeping thread
        to_block->state = SLEEP_BLOCK;
        blocked_threads.push_back(to_block);
    }

    if (to_block->state == READY){
        // blocking a ready thread - remove it from the ready queue
        to_block->state = BLOCKED;
        while (!ready_queue.empty()) {
            thread* current_item = ready_queue.front();
            ready_queue.pop();
            if (current_item->id != tid) {
                temp_q.push(current_item);
            }
            else{
                blocked_threads.push_back(current_item);
            }
        }

        while (!temp_q.empty()){
            ready_queue.push(temp_q.front());
            temp_q.pop();
        }
    }
    unblock();
    return SUCCESS;
}

int uthread_resume(int tid){ 
    block();
    if (termination.find(tid) != termination.end()){
        std::cerr << TERMINATION_ERR;
        unblock();
        return FAILED_LIB;
    }
    if (threads.find (tid) == threads.end()){
        std::cerr << TID_ERR; // unknown id
        unblock();
        return FAILED_LIB;
    }
    int i;
    bool found= false;
    for (i=0; i < (int) blocked_threads.size(); i++){
        // change the thread's state
        if (blocked_threads.at (i)->id == tid){
            thread* blocked_thread = blocked_threads.at(i);
            if (threads.find(tid)->second->state == SLEEP_BLOCK){
                // the thread is also in a sleep state
                blocked_thread->state = SLEEP;
            }
            else{
                // get the thread back to the ready queue
                blocked_thread->state = READY;
                ready_queue.push (blocked_thread);
            }
            found = true; // thread was found and we have dealt with it
            break;
        }
    }
    if (found) {
        blocked_threads.erase(blocked_threads.begin() + i); // delete the thread from blocked_threads
    }
    unblock();
    return SUCCESS;
}

int uthread_sleep(int num_quantums){
    
    block();
    if (running_thread->id == 0){
        std::cerr << SLEEP_ERR; // main thread can't be in a sleep state
        unblock(); 
        return FAILED_LIB;
    }
    if (num_quantums <= 0){
        std::cerr << NEGATIVE_SLEEP_ERR;  // negative num of quantums
        unblock();
        return FAILED_LIB;
    }
    int i = running_thread->id;
    thread * s = threads.find (i)->second;
    sleeping_threads.insert({i, s}); // insert the thread pointer to the sleeping_threads map
    
    running_thread->state = SLEEP; // change the state to SLEEP_STATE 
    running_thread->sleep_quantum = num_quantums + 1; // update remaining sleep quantum
    unblock();
    sa.sa_handler(0); // handle the running thread - context switch
    return SUCCESS;
}

int uthread_get_tid(){
    return running_thread->id;
}

int uthread_get_total_quantums(){
    return quantums_number;
}

int uthread_get_quantums(int tid){
    block();
    if (threads.find (tid) != threads.end()){ 
        thread* to_get = threads.find (tid)->second; // get the thread's quantum
        unblock();
        return to_get->quantum;
    }
    std::cerr << TID_ERR;
    unblock();
    return FAILED_LIB;
}

