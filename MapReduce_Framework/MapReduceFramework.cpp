#include <mutex>
#include <iostream>
#include <map>
#include <atomic>
#include <algorithm>
#include <pthread.h>
#include "MapReduceFramework.h"
#include "Barrier.h"
#include "Barrier.cpp"

static const char *const PTHREAD_JOIN_ERR = "system error: error during pthread join\n";
static const char *const REDUCE_MUT = "REDUCING";
static const char *const MAP_MUT = "MAP_ST";
static const char *const EMIT3_MUT = "EMIT3";
static const char *const EMIT2_MUT = "EMIT2";
static const char *const SORT_MUT = "SORTING";
static const char *const MUTEX_LOCK_ERR = "system error: error on pthread_mutex_lock\n";
static const char *const MUTEX_UNLOCK_ERR = "system error: error on pthread_mutex_unlock\n";
static const char *const PTHREAD_CREATE_ERR = "system error: error during creating a thread\n";
static const char *const STATE_ERR = "system error: the given state is null\n";
static const char *const ERROR_ALLOCATION = "system error: Memory allocation fails\n";
static const char *const SHUFFLE_ST = "SHUFFLE_MUT";
static const char *const REDUCE_ST = "RED_MUT";
static const char *const SHUFFLE_MUTEX = "SHUFFLE";
static const char *const MERGING_MUTEX = "MERGING";
static const char *const GET_STATE = "STATE_CHANGE";
static const char *const WAIT_FOR_JOB = "JOB";

static const int PERCENTAGE = 100;
static const int FAILED = 1;
static const int SUCCESS = 0;
struct JobContext;
typedef struct
{
    pthread_t *thread;
    int id;
    JobContext *job;
} ThreadContext;

typedef struct JobContext
{
    bool wait;
    std::vector<ThreadContext *> *threads;
    std::map<int, IntermediateVec *> *threads_vec;
    std::map<std::string, pthread_mutex_t *> *mutexes;
    JobState *state;
    int num_of_threads;
    const InputVec *inputVec;
    OutputVec *outputVec;
    const MapReduceClient *client;
    std::atomic<int> *atomic_map_counter;
    std::atomic<int> *atomic_red_counter;
    std::atomic<int> *atomic_shuffle_counter;
    std::atomic<int> *atomic_emit3_counter;
    std::atomic<int> *atomic_pairs_counter;
    std::vector<IntermediateVec *> *shuffled_vec;
    Barrier *barrier;
} JobContext;

void map_stage (ThreadContext *jobContext);
void sort_stage (ThreadContext *context);
void shuffle_stage (JobContext *context);
void reduce_stage (JobContext *context);

void inline mutex_lock (pthread_mutex_t *mutex_to_lock)
{
  if (pthread_mutex_lock (mutex_to_lock) != SUCCESS)
  {
    std::cout << MUTEX_LOCK_ERR;
    exit (FAILED);
  }
}

void inline mutex_unlock (pthread_mutex_t *mutex_to_unlock)
{
  if (pthread_mutex_unlock (mutex_to_unlock) != SUCCESS)
  {
    std::cout << MUTEX_UNLOCK_ERR;
    exit (FAILED);
  }
}

void *job_ToDo (void *ptr)
{
  // Step 1: Cast the input pointer to the ThreadContext
  auto *context = (ThreadContext *) ptr;
  auto *jobContext = context->job;
  // Step 2: Map Phase
  map_stage (context);

  // Step 3: Sort Phase
  sort_stage (context);

  // Step 4: Synchronize after the Sort Phase
  jobContext->barrier->barrier ();

  // Step 5: Shuffle Phase (only for thread 0)
  if (context->id == 0)
  {
    mutex_lock (jobContext->mutexes->at (SHUFFLE_ST));
    jobContext->state->stage = SHUFFLE_STAGE;
    mutex_unlock (jobContext->mutexes->at (SHUFFLE_ST));
    shuffle_stage (jobContext);
  }

  // Step 6: Synchronize after the Shuffle Phase
  jobContext->barrier->barrier ();

  if (jobContext->state->stage != REDUCE_STAGE)
  {
    mutex_lock (jobContext->mutexes->at (REDUCE_ST));
    jobContext->state->stage = REDUCE_STAGE;
    mutex_unlock (jobContext->mutexes->at (REDUCE_ST));
  }
  reduce_stage (jobContext);

  // Step 8: End the thread's lifecycle
  return nullptr;
}

int comparator (const IntermediatePair &pair1, const IntermediatePair &pair2)
{
  return pair1.first->operator< (*pair2.first);
}

void reduce_stage (JobContext *context)
{
  // Call client reduce
  size_t counter;
  while ((counter = ((*(context->atomic_red_counter))++)) <
         context->shuffled_vec->size ())
  {
    context->client->reduce ((context->shuffled_vec->at (counter)),
                             context);
  }
}

void pair_merging_helper (IntermediateVec &first, IntermediateVec &second)
{
  IntermediateVec to_shuffle_temp = IntermediateVec ();
  std::merge (first.begin (), first.end (),
              second.begin (), second.end (),
              std::back_inserter (to_shuffle_temp), comparator);
  first = std::move (to_shuffle_temp);
}

IntermediateVec *merge_vectors (const std::vector<IntermediateVec *> &to_merge)
{
  if ((int) to_merge.size () <= 1)
  { // it's one vector or less -
    //there is no need for merge
    return to_merge.at (0);
  }
  std::vector<IntermediateVec *> vec_set = std::vector<IntermediateVec *> ();
  for (int n = 0; n < (int) to_merge.size (); n++, n++)
  {
    if (n < (int) to_merge.size () - 1)
    {
      IntermediateVec &first = *to_merge.at (n);
      IntermediateVec &second = *to_merge.at (n + 1);
      pair_merging_helper (first, second);
    }
    vec_set.push_back (to_merge.at (n));
  }
  if ((int) vec_set.size () <= 1)
  {
    IntermediateVec *&pVector = vec_set.at (0);
    return pVector;
  }
  return merge_vectors (vec_set);
}

void shuffle_stage (JobContext *context)
{
  mutex_lock (context->mutexes->at (SHUFFLE_MUTEX));

  auto to_shuffle_vector = IntermediateVec ();
  std::vector<IntermediateVec *> new_vec = std::vector<IntermediateVec *> ();
  if ((int) context->threads_vec->size () > 1)
  {
    for (auto pair: *(context->threads_vec))
    {
      // Collect the sorted intermediate vectors
      new_vec.push_back (pair.second);
    }
    mutex_lock (context->mutexes->at (MERGING_MUTEX));
    //Merge the sorted vectors into one
    to_shuffle_vector = *(merge_vectors (new_vec));
    mutex_unlock (context->mutexes->at (MERGING_MUTEX));
  }
  else
  {
    // Single Intermediate Vector
    to_shuffle_vector = *(context->threads_vec->at (0));
  }
  // Group intermediate pairs by their keys
  for (auto pair: to_shuffle_vector)
  {
    bool found_key = false;
    for (auto vec: *(context->shuffled_vec))
      if (!(pair.first->operator< (*vec->at (0).first))
          && !(vec->at (0).first->operator< (*pair.first)))
      {
        vec->push_back (pair);
        found_key = true;
        break;
      }
    if (!found_key)
    {
      auto *newx_vec = new IntermediateVec ();
      newx_vec->push_back (pair); // insert element
      context->shuffled_vec->push_back (newx_vec); // push vec to the data
    }
    ((*(context->atomic_shuffle_counter))++);
  }
  mutex_unlock (context->mutexes->at (SHUFFLE_MUTEX));
}

void sort_stage (ThreadContext *context)
{
  JobContext *jobContext = context->job;
  // Acquire the SORT_MUT mutex
  mutex_lock (jobContext->mutexes->at (SORT_MUT));
  if (jobContext->threads_vec->find
      (context->id) == jobContext->threads_vec->end ())
  {
    mutex_unlock (jobContext->mutexes->at (SORT_MUT));
    return; // No data to sort for this thread
  }
  auto vector = jobContext->threads_vec->find (context->id);
  // Sort the intermediate vector using the comparator
  std::sort (vector->second->begin (),
             vector->second->end (), comparator);
  // Release the SORT_MUT mutex
  mutex_unlock (jobContext->mutexes->at (SORT_MUT));
}

void map_stage (ThreadContext *context)
{

  JobContext *job = context->job;
  // Acquire the MAP_STAGE mutex
  mutex_lock (job->mutexes->at (MAP_MUT));
  if (job->state->stage != MAP_STAGE)
  {
    job->state->stage = MAP_STAGE;
  }
  mutex_unlock (job->mutexes->at (MAP_MUT));
  // Distribute work among threads using atomic counters
  size_t counter;
  while ((counter = ((*(job->atomic_map_counter))++)) < job->inputVec->size ())
  {
    job->client->map (job->inputVec->at (counter).first,
                      job->inputVec->at (counter).second, context);
  }
}
void creating_mutexes (JobContext *jobContext)
{

  // create mutexes
  auto *mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({MAP_MUT, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({REDUCE_MUT, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({EMIT2_MUT, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({EMIT3_MUT, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({SORT_MUT, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({GET_STATE, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({WAIT_FOR_JOB, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({MERGING_MUTEX, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({SHUFFLE_MUTEX, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({SHUFFLE_ST, mutexPtr});
  mutexPtr = new pthread_mutex_t;
  pthread_mutex_init (mutexPtr, nullptr);
  jobContext->mutexes->insert ({REDUCE_ST, mutexPtr});
}

JobHandle startMapReduceJob (const MapReduceClient &client,
                             const InputVec &inputVec, OutputVec &outputVec,
                             int multiThreadLevel)
{
  // Step 1: Create jobContext instance
  auto *jobContext = new JobContext{
      .wait = false,
      .threads = new std::vector<ThreadContext *> (),
      .threads_vec = new std::map<int, IntermediateVec *> (),
      .mutexes = new std::map<std::string, pthread_mutex_t *> (),
      .state = new JobState{.stage=UNDEFINED_STAGE, .percentage=0.0},
      .num_of_threads = multiThreadLevel,
      .inputVec= &inputVec, .outputVec = &outputVec, .client = &client,
      .atomic_map_counter=new std::atomic<int> (0),
      .atomic_red_counter = new std::atomic<int> (0),
      .atomic_shuffle_counter= new std::atomic<int> (0),
      .atomic_emit3_counter = new std::atomic<int> (0),
      .atomic_pairs_counter = new std::atomic<int> (0),
      .shuffled_vec = new std::vector<IntermediateVec *> (),
      .barrier = new Barrier (multiThreadLevel)
  };
  // Step 2: Create mutexes
  creating_mutexes (jobContext);
  // Step 3: Create threads
  for (int i = 0; i < multiThreadLevel; i++)
  {
    auto *s = new (std::nothrow) std::vector<IntermediatePair> ();
    jobContext->threads_vec->insert ({i, s});
    auto *thread = new (std::nothrow) ThreadContext{.thread= new
        (std::nothrow) pthread_t, .id=i, .job=jobContext};
    if (!s || !thread || !thread->thread)
    {
      std::cout << ERROR_ALLOCATION << std::endl;
      exit (FAILED);
    }
    jobContext->threads->push_back (thread);
  }
  for (int i = 0; i < multiThreadLevel; i++)
  {
    int res = pthread_create (
        (jobContext->threads->at (i)->thread), nullptr,
        &job_ToDo, (jobContext->threads->at (i)));
    if (res != SUCCESS)
    {
      std::cout << PTHREAD_CREATE_ERR;
      exit (FAILED);
    }
  }
  return static_cast<JobHandle>(jobContext);
}

void waitForJob (JobHandle job)
{

  auto *jobContext = (JobContext *) job;
  // Step 1: Acquire lock to ensure thread-safe operation
  mutex_lock (jobContext->mutexes->at (WAIT_FOR_JOB));
  // Step 2: Check if the threads are already joined
  if (!jobContext->wait)
  {
    // Step 3: Wait for all threads to finish
    for (int i = 0; i < jobContext->num_of_threads; i++)
    {
      int res = pthread_join (*(jobContext->threads->at (i)->thread),
                              nullptr);
      if (res != SUCCESS)
      {
        std::cout << PTHREAD_JOIN_ERR;
        exit (FAILED);
      }
    }
    // Mark the threads as joined
    jobContext->wait = true;
  }
  // Step 4: Release the lock
  mutex_unlock (jobContext->mutexes->at (WAIT_FOR_JOB));
}

void getJobState (JobHandle job, JobState *state)
{
  if (state == nullptr)
  {
    std::cout << STATE_ERR;
    exit (FAILED);
  }
  auto *jobContext = (JobContext *) job;

  // Step 1: Acquire lock to ensure thread-safe state access
  mutex_lock (jobContext->mutexes->at (GET_STATE));
  // Step 2: Update the state with the current stage
  state->stage = jobContext->state->stage;
  // Step 3: Update progress percentage based on the current stage
  if (state->stage == MAP_STAGE)
  {
    int counter = jobContext->atomic_map_counter->load ();
    if (counter <= (int) jobContext->inputVec->size ())
    {
      state->percentage = ((float) counter /
                           (float) jobContext->inputVec->size ()) *
                          PERCENTAGE;
    }
    else
    {
      state->percentage = PERCENTAGE;
    }
  }
  else if (state->stage == SHUFFLE_STAGE)
  {
    state->percentage = ((float) jobContext->atomic_shuffle_counter->load () /
                         (float) jobContext->atomic_pairs_counter->load ())
                        * PERCENTAGE;
  }
  else if (state->stage == REDUCE_STAGE)
  {
    state->percentage = ((float) jobContext->atomic_emit3_counter->load () /
                         (float) jobContext->shuffled_vec->size ()) *
                        PERCENTAGE;
  }
  // Step 4: Update the JobContext's state for progress tracking
  jobContext->state->percentage = state->percentage;
  // Step 5: Release lock
  mutex_unlock (jobContext->mutexes->at (GET_STATE));
}

void closeJobHandle (JobHandle job)
{
  // Step 1: Wait for all threads to complete
  waitForJob (job);
  auto *jobContext = (JobContext *) job;

  // Step 2: Clean up thread-related resources
  for (auto pthread: *(jobContext->threads))
  {
    delete (pthread->thread);
    delete (pthread);
  }
  delete jobContext->threads;

  // Step 3: Destroy and free all mutexes
  for (const auto &item: *jobContext->mutexes)
  {
    pthread_mutex_destroy (item.second);
    delete (item.second);
  }
  // Step 4: Free intermediate vectors
  for (const auto &item: *jobContext->threads_vec)
  {
    delete (item.second);
  }
  // Step 5: Free shuffled vectors
  for (const auto &item: *jobContext->shuffled_vec)
  {
    delete (item);
  }
  // Step 6: Free other job resources
  delete jobContext->mutexes;
  delete jobContext->state;
  delete jobContext->threads_vec;
  delete jobContext->atomic_map_counter;
  delete jobContext->atomic_emit3_counter;
  delete jobContext->atomic_red_counter;
  delete jobContext->atomic_shuffle_counter;
  delete jobContext->atomic_pairs_counter;
  delete jobContext->shuffled_vec;
  delete (jobContext->barrier);
  delete jobContext;
}

void emit2 (K2 *key, V2 *value, void *context)
{
  auto *context_ = (ThreadContext *) context;
  JobContext *job_context = context_->job;
  mutex_lock (job_context->mutexes->at (EMIT2_MUT));
  auto &a = job_context->threads_vec->find (context_->id)->second;
  a->push_back ({key, value});
  (*(context_->job->atomic_pairs_counter))++;
  mutex_unlock (job_context->mutexes->at (EMIT2_MUT));
}

void emit3 (K3 *key, V3 *value, void *context)
{
  auto *jobContext = (JobContext *) context;
  mutex_lock (jobContext->mutexes->at (EMIT3_MUT));
  jobContext->outputVec->push_back ({key, value});
  (*(jobContext->atomic_emit3_counter))++;
  mutex_unlock (jobContext->mutexes->at (EMIT3_MUT));
}
