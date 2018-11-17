#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
// #include "lib/fixed-point.h"
#include "threads/fixed-point.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

int64_t next_wakeup_time ;

struct thread *array1[2040];
int array2[2040];
int array3[2040];

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/*list for blocked threads */
static struct list sleepers;

/* Idle thread. */
static struct thread *idle_thread;

static struct thread *wakeup_managerial_thread;

static struct thread *mlfqs_managerial_thread;
/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
void thread_priority_temporarily_up(void);
void thread_priority_restore(void);
bool before (const struct list_elem *a, const struct list_elem *b,void *aux UNUSED);
bool before_ready (const struct list_elem *a, const struct list_elem *b,void *aux UNUSED);
void thread_run_untill(int64_t wakeup_at, int64_t currentTime);
void set_wakeup_next(void);
int load_avg;
/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init(&sleepers);
  next_wakeup_time = INT64_MAX;
  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  // initial_thread->nice = 0;
  // initial_thread->recent_cpu = 0;
  init_thread (initial_thread, "main", PRI_DEFAULT);
  
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();


  memset(array1,NULL,sizeof(array1));
  memset(array2,0,sizeof(array2));
  memset(array3,-2,sizeof(array3));
  array1[initial_thread->tid] = thread_current();

}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  load_avg = 0;
  // next_wakeup_time=-7;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);
    /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
  thread_create ("wakeup_managerial_thread", PRI_MAX, timer_wakeup, NULL);
  thread_create ("mlfqs_managerial_thread",PRI_MAX, mlfqs_work,NULL);

}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

    int64_t ticks = timer_ticks();



	
  /*Check if sleepers list is empty or not.*/
    /*if list isn't empty then compare current time to the next_wake_up.*/
   // ASSERT(is_thread(wakeup_managerial_thread));
  if(timer_ticks() == next_wakeup_time) thread_unblock(wakeup_managerial_thread);


  if(thread_mlfqs && timer_ticks >0){
    // if( thread_current() != idle_thread && thread_current() != mlfqs_managerial_thread && thread_current() != wakeup_managerial_thread) 
    
    // increments the recent cpu time of current running thread by 1 value after every tick	
    mlfqs_increment();

    /* TIMER_FREQ = 100
       after every 1 second, unblock the mlfqs managerial thread and update the priorities and load avg value 
    */
    if(timer_ticks() % TIMER_FREQ == 0 ){
	    if(mlfqs_managerial_thread && mlfqs_managerial_thread->status == THREAD_BLOCKED)
	      thread_unblock(mlfqs_managerial_thread);
	  } 

    /* after running for 4 ticks, priority of a running thread is decreased by 1 value 
       before sending it to ready list    */ 
	  else if(timer_ticks() % 4 == 0){
	      if(thread_current()->priority != PRI_MIN) thread_current()->priority -= 1;
	      // thread_mlfqs_change_priority();
	  }
  }

  


  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}


// updates the sorted ready list after recalculating the priorities of every thread
void update_ready_list(void)
{
  list_sort(&(ready_list), before_ready, NULL);
}


/* run after 1 sec (or 100 ticks)  and updates the values of 
   recent_cpu time usage and priority for every thread and also the global load_avg value */
static void mlfqs_work(void){
	mlfqs_managerial_thread = thread_current();
  while(true)
  {
    // mlfqs_recalculate();
    
    enum intr_level old_level = intr_disable();
    // mlfqs_recalculate ();
    // mlfqs_load_avg ();
    
    // updates recent cpu time usage for every thread
    thread_calc_recent_cpu();

    // updates priority of every thread
    thread_mlfqs_change_priority();

    // updates the sorted ready list after recalculating the priorities of every thread
    update_ready_list();

    // updates the global load avg value
    thread_mlfqs_change_load_avg();
  
    // blocks the current managerial thread
    thread_block();
    intr_set_level(old_level);

  }
}




static void timer_wakeup(void){
  wakeup_managerial_thread = thread_current();
  
  while(1){

    enum intr_level old_level = intr_disable();

    while(!list_empty(&sleepers)){

      struct list_elem *t1 = list_front (&sleepers);
      struct thread *t = list_entry (t1, struct thread, elem);
      if (t->wakeup_t <= next_wakeup_time)
      {
          list_pop_front (&sleepers);
          thread_unblock (t);
      }
      else break;
    }

    if(!list_empty(&sleepers)){
      next_wakeup_time = list_entry(list_begin(&sleepers),struct thread,elem)->wakeup_t;
    }
    else{
      next_wakeup_time =  INT64_MAX;
    }

    thread_block();

    intr_set_level(old_level);
  }
}







/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.	

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);

  /*successful birth of thread tid*/
  tid = t->tid = allocate_tid ();

  array1[tid] = t;

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack'
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  // if(name == "wakeup_managerial_thread") wakeup_managerial_thread = t;
  // if(name == "mlfqs_managerial_thread") mlfqs_managerial_thread = t;
  thread_unblock (t);
  thread_priority_check();
  

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_insert_ordered(&ready_list, &t->elem, before_ready, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread frolist_push_backm all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it call schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    list_insert_ordered(&ready_list, &cur->elem, before_ready, NULL);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);

}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{

  enum intr_level old_level = intr_disable();
  struct thread *t = thread_current();
  t->pre_priority = t->priority;
  t->priority = new_priority;

  /*store the priority of thread before donation */
  t->priority_before = new_priority;

  /**/
  thread_donate_priority(t);
  thread_priority_check();
  intr_set_level(old_level);

}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED)
{
  /* Not yet implemented. */ 
  struct thread *t = thread_current(); 
  thread_current()->nice = nice ;
  if (t == idle_thread || t == wakeup_managerial_thread || t == mlfqs_managerial_thread) return;
  
  int term1 = convert_n_to_fixed_point (PRI_MAX);
  int term2 = divide_x_by_n (t->recent_cpu, 4);
  int term3 = convert_n_to_fixed_point (multiply_x_by_n (t->nice, 2));
  term1 = substract_y_from_x (term1, term2);
  term1 = substract_y_from_x (term1, term3);
  
  /* In B.2 Calculating Priority : The result should be rounded down to the nearest integer (truncated). */
  term1 = convert_x_to_integer_zero (term1);

  if (term1 < PRI_MIN) t->priority = PRI_MIN;
  else if (term1 > PRI_MAX) t->priority = PRI_MAX;
  else  t->priority = term1;


  thread_priority_check();
  // int aux = _ADD_INT (_DIVIDE_INT (thread_current()->recent_cpu, 4), 2*thread_current()->nice);
  // int val = _TO_INT_ZERO (_INT_SUB (PRI_MAX, aux));
  // thread_current()->priority = val > PRI_MAX ? PRI_MAX : val < PRI_MIN ? PRI_MIN : val;   
  // thread_priority_check();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  return thread_current()->nice ;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  /* Not yet implemented. */
  // enum intr_level old_level = intr_disable ();
   enum intr_level old_level = intr_disable ();
  int load_avg_nearest = convert_x_to_integer_nearest (multiply_x_by_n (load_avg, 100) );
  intr_set_level (old_level);
  return load_avg_nearest;

}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  /* Not yet implemented. */
  // enum intr_level old_level = intr_disable ();
  enum intr_level old_level = intr_disable ();
  int recent_cpu_nearest = convert_x_to_integer_nearest (multiply_x_by_n (thread_current ()->recent_cpu, 100) );
  intr_set_level (old_level);
  return recent_cpu_nearest;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->priority_before = t->priority;
  t->magic = THREAD_MAGIC;

  t->nice = 0;
  t->recent_cpu = 0;
  list_init(&t->acquired_locks);
  t->seeking = NULL;
  list_push_back (&all_list, &t->allelem);
  int i;
  for (i = 0; i < MAX_FILES; ++i)
  {
      t->files[i] = NULL; 
  }


  // if(t!=initial_thread){
  //   t->parent = thread_current();
  //   list_push_back (&(t->parent->children), &t->parent_elem);
  // }
  // else t->parent = NULL;

  // list_init (&t->children);
  sema_init(&t->sema_ack,0);
  sema_init(&t->thread_load_done,0);
  sema_init(&t->sema_exit,0);
  t->executable = NULL;
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);


/*Temporarily makes the priority of the current thread to maximum.
It's current priority is stored as it's pre priority for restoring purposes.*/
void thread_priority_temporarily_up(void){
  struct thread *t = thread_current();
  t->pre_priority = t->priority;
  t->priority = PRI_MAX;
}

/*Restore it's original priority(i.e. stored in it's pre priority)
just after it gets blocked. */
void thread_priority_restore(void){
  struct thread *t = thread_current();
  t->priority = t->pre_priority;
}

/*It's compares the wake_up time of two list elements */
bool before (const struct list_elem *a, const struct list_elem *b,void *aux UNUSED)
{
  struct thread *ta = list_entry (a, struct thread, elem);
  struct thread *tb = list_entry (b, struct thread, elem);

  return ta->wakeup_t < tb->wakeup_t;

}

/*It's compares the priority of two list elements */
bool before_ready (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct thread *ta = list_entry (a, struct thread, elem);
  struct thread *tb = list_entry (b, struct thread, elem);
  return ta->priority > tb->priority;
}

/* Current running thread is transferred to sleepers list and its wakeup time is set.
  The time to wake up next thread is also set*/
void thread_run_untill(int64_t wakeup_at, int64_t currentTime){
  enum intr_level previous = intr_disable();
  struct thread *t = thread_current();


/*No need to block & push the current running thread to the sleepers
 list if the current time is greater than equal to it's wake up time */
  if(currentTime >= wakeup_at) return;
  //lock_init(&tid_lock);
  //lock_acquire(&tid_lock);



  ASSERT(t->status == THREAD_RUNNING);
          t->wakeup_t = wakeup_at;
          /*Push the current running thread to the sleepers list*/
          list_insert_ordered(&sleepers,&(t->elem),before,NULL);

  if(!list_empty(&sleepers)) next_wakeup_time = list_entry(list_begin(&sleepers),struct thread,elem)->wakeup_t;
  else next_wakeup_time = INT64_MAX;  
  //lock_release(&tid_lock);
  
  thread_block();
  intr_set_level(previous);

}

void set_wakeup_next(void)
{

  enum intr_level old_level;
  old_level = intr_disable();

  // if sleepers(block) list is empty, set next wakeup time to infinity.
  if(list_empty(&sleepers))			 
    next_wakeup_time = INT64_MAX;
  else
  {
  	/*f points to the first element of sleepers list which has 
    the earliest wake up time among all the elements in the list.*/
    struct list_elem *t1 = list_front(&sleepers);    
    
    /*creates a thread 'th' that converts the front
     list element (of type elem) into a thread data structure*/
    struct thread *th = list_entry(t1, struct thread, elem);
    
    /*if wake_up time of current thread is less than current
     next_wake time and timer clock isn't before next_wakeup time 
     so that a thread isn't blocked before its wake_up time.*/
    if(th->wakeup_t <= next_wakeup_time && timer_ticks()>= next_wakeup_time)   
    { 
    /*pops the first element of sleepers list that has the earliest wake_up time.*/								 
      list_pop_front(&sleepers);
      
      /*unblocks the current thread (pointing to first element)
       to tranfer to ready queue list from blocked list.*/	
      thread_unblock(th);			 

      if(list_empty(&sleepers))		
        next_wakeup_time=INT64_MAX;
        else
        { 
         // next first element in list
          t1=list_front(&sleepers); 
          
          // next thread having earliest wake_up time among the remaining elements in sleepers list
          th = list_entry(t1, struct thread, elem); 
          
          // and sets next_wake_up time to its.
          next_wakeup_time = th->wakeup_t; 
        }
    }
    else
     // else the thread remains blocked and next_wake_up time is set to its.
      next_wakeup_time =  th->wakeup_t;  
  }

  //returns the previous interrupt level 
  intr_set_level (old_level); 
}


/*comparison template to sort the lock waiters list*/
bool before_lock (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{

  struct lock *l1 = list_entry(a, struct lock, elem);
  struct lock *l2 = list_entry(b, struct lock, elem);

  return l1->priority > l2->priority;
}



/*donate priority or set the new priority from its locks list.If donated priority is greater
than the priority of any lock it acquires then set the thread priority to it(can be handled 
similarly because we already changed the lock priority to donated in lock_acquire) or change
priority from other lock having in the thread list having greater priority */
void thread_donate_priority (struct thread *t)
{
  enum intr_level old_level = intr_disable ();
  
  /*change the priority of thread to the highest possible from the locks it acquired */
  priority_change(t);
  
  /* If thread is in ready list, sort it. */
  if (t->status == THREAD_READY)
  {
      list_sort(&ready_list,before_ready,NULL);
      // schedule();
  }
  
  intr_set_level (old_level);
}



/*compares the priority of thread to the locks iit acquirs and change if necessary*/
void priority_change(struct thread *t)
{

  enum intr_level old_level = intr_disable ();

   /* may happen initial priority is the larger than any other lock or may be PRI_MAX */
  int max_prio = t->priority_before;          
  int lock_prio;

  /* Get locks' max priority. */
  if (!list_empty (&t->acquired_locks))       
    {

      /*sort the locks acquired according to their priority*/
      list_sort (&t->acquired_locks, before_lock, NULL);    
      lock_prio = list_entry (list_front (&t->acquired_locks),
                                  struct lock, elem)->priority;
      if (lock_prio > max_prio)
        max_prio = lock_prio;
    }

  t->priority = max_prio;
  intr_set_level (old_level);
}



/* yield the current running thread if it has lower priority */
void thread_priority_check(void){
  enum intr_level old_level = intr_disable();
  struct thread *t = thread_current();
  if(!list_empty(&ready_list)){
    struct thread *head = list_entry(list_front(&ready_list),struct thread,elem);
    if(head->priority > t->priority){
      thread_yield();
    }
  }
  intr_set_level(old_level);
}


void insert_lock(struct lock *lock)
{
  enum intr_level old_level = intr_disable ();
  /*insert the lock to the acquired locks list*/
  list_insert_ordered (&thread_current ()->acquired_locks, &lock->elem, before_lock, NULL);  /* insert lock to the aquired locks list of the current thread. */

  /*since one thread has acquired the lock, we now should change the 
  lock's priority to  maximum of the  waiter's list's priority */
  if(!list_empty(&(lock->semaphore.waiters)))
  {
    list_sort(&lock->semaphore.waiters,before_ready,NULL);
    lock->priority = list_entry(list_front(&lock->semaphore.waiters),struct thread , elem)->priority;

  }
  else lock->priority = 0;

  intr_set_level (old_level);
}


/* Remove lock from list and update priority. */
void remove_lock(struct lock *lock)
{
  enum intr_level old_level = intr_disable ();

  /* remove lock from the thread's lock_acquired list. */
  list_remove (&lock->elem);  

  /*change the priority of the thread*/
  priority_change(thread_current ());     
  intr_set_level (old_level);                                
}

 //Function to calculte new recent_cpu value of all the threads
 void thread_calc_recent_cpu(void){
  // enum intr_level old_level = intr_disable ();

  struct list_elem *y;

  for (y = list_begin (&all_list); y != list_end (&all_list); y = list_next (y))
    {
      struct thread *t = list_entry (y, struct thread, allelem);
      if( t!=idle_thread && t!=wakeup_managerial_thread && t!=mlfqs_managerial_thread){
        // int temp = ( ) ;
        int term1 = multiply_x_by_n (2, load_avg);     //According to pintos doc, formula for recent_cpu is implemented using 17.14 fixed point arthimetic
        int term2 = term1 + convert_n_to_fixed_point (1); //i.e. recent_cpu = (2*load_avg )/(2*load_avg + 1) * recent_cpu + nice
        term1 = multiply_x_by_y (term1, t->recent_cpu);
        term1 = divide_x_by_y (term1, term2);
        term1 = add_x_and_n (term1, t->nice);
        
        t->recent_cpu = term1;
            
      }
    }

  // intr_set_level (old_level);
}

/* updates the priority of all the threads other than idle thread and the two managerial threads after 1 second */
 void thread_mlfqs_change_priority(void){
  // enum intr_level old_level = intr_disable ();

  struct list_elem *y;

  /* traversing every thread other than the three mentioned above
     and recalculating its priority by the formula :-
         priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)      */ 
  for (y = list_begin (&all_list); y != list_end (&all_list); y = list_next (y))
  { 
    struct thread *t = list_entry (y, struct thread, allelem);
    if(  t!=idle_thread && t!=wakeup_managerial_thread && t!=mlfqs_managerial_thread){
     	int term1 = convert_n_to_fixed_point (PRI_MAX);
      int term2 = divide_x_by_n (t->recent_cpu, 4);
      int term3 = convert_n_to_fixed_point (multiply_x_by_n (t->nice, 2));
      term1 = substract_y_from_x (term1, term2);
      term1 = substract_y_from_x (term1, term3);
      
      /* In B.2 Calculating Priority : The result should be rounded down to the nearest integer (truncated). */
      term1 = convert_x_to_integer_zero (term1);

      // if priority is less than 0, then set it to 0 
      if (term1 < PRI_MIN) t->priority = PRI_MIN;
      // if priority is greater than maximum value of 63, then set it to PRI_MAX (63) 
      else if (term1 > PRI_MAX) t->priority = PRI_MAX;
      // else assign it the calculated value
      else  t->priority = term1;
    }
  }

  // intr_set_level (old_level);
}

  //Function to calculate new load_avg value of all the threads
 void thread_mlfqs_change_load_avg(void){

	 // enum intr_level old_level = intr_disable ();

	int ready_threads = list_size(&ready_list); //getting number of threads in ready list
    
	if(wakeup_managerial_thread && wakeup_managerial_thread->status == THREAD_READY) ready_threads--; //If threads in ready list has wakeup_managerial_thread or mlfqs_managerial thread 
	if(mlfqs_managerial_thread && mlfqs_managerial_thread->status == THREAD_READY) ready_threads--;// decrease the number of such threads

	if (thread_current() != idle_thread && thread_current() != wakeup_managerial_thread && thread_current() != mlfqs_managerial_thread) ready_threads++;

    // int term1 = 
  	int term1 = divide_x_by_y (59, 60);    //According to pintos doc, formula for load_avg is implemented using 17.14 fixed point arthimetic
    term1 = multiply_x_by_y (term1, load_avg); //i.e. load_avg = (59/60)*load_avg + (1/60)*ready_threads.
    int term2 = divide_x_by_y (ready_threads, 60);
    term1 = add_x_and_y (term1, term2);
    
    load_avg = term1;
  	// intr_set_level (old_level);
}


// increments the recent cpu time of current running thread by 1 value
void 
mlfqs_increment (void)
{
  if (thread_current() == idle_thread || thread_current() == wakeup_managerial_thread || thread_current() == mlfqs_managerial_thread) return;
  thread_current ()->recent_cpu = add_x_and_n (thread_current ()->recent_cpu, 1);
}

