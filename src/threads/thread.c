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
#include "threads/fixed_point.h"

#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

#ifdef USERPROG
#include "userprog/process.h"
#endif

/*Random value for struct thread's `magic' member.
Used to detect stack overflow. See the big comment at the top
of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/*List of processes in THREAD_READY state, that is, processes
that are ready to run but not actually running. */
static struct list ready_list;

/*List of all processes. Processes are added to this list
when they are first scheduled and removed when they exit. */
static struct list all_list;
/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/*sleep상태인 스레드 리스트*/
static struct list sleep_list;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
	void *eip; 		/* Return address. */
	thread_func *function; 	/* Function to call. */
	void *aux; 		/* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks; 	/* # of timer ticks spent idle. */
static long long kernel_ticks; 	/* # of timer ticks in kernel threads. */
static long long user_ticks; 	/* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4 		/* # of timer ticks to give each thread. */
static unsigned thread_ticks; 	/* # of timer ticks since last yield. */

/*If false (default), use round-robin scheduler.
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
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
/*리스트에 저장된 스레들의 wakeup_tick 중 최소 값 저장*/
static int64_t next_tick_to_awake = INT64_MAX;

int load_avg;


/*Initializes the threading system by transforming the code
that's currently running into a thread. This can't work in
general and it is possible in this case only because loader.S
was careful to put the bottom of the stack at a page boundary.

Also initializes the run queue and the tid lock.

After calling this function, be sure to initialize the page
allocator before trying to create any threads with
thread_create().

It is not safe to call thread_current() until this function
finishes. */


/*재계산*/
void mlfqs_recalc(void)
{
	struct list_elem *e;
	struct thread* t;

	for( e = list_begin(&all_list) ; e != list_end(&all_list) ; e = list_next(e))
	{
		t =  list_entry (e, struct thread,allelem);
		mlfqs_recent_cpu (t);
		mlfqs_priority (t);
		
	}	
}

void mlfqs_increment(void)
{
	struct thread *t = thread_current();
	if(t!=idle_thread)
		t-> recent_cpu = add_mixed(t-> recent_cpu ,1);

}

void mlfqs_priority (struct thread *t)
{
	/*if( t!=idle_thread ) 
	{
		int a = div_mixed(t->recent_cpu,4); // t-> recent_cpu값은 FP
		int b = int_to_fp(PRI_MAX - (t->nice*2));
		t->priority = fp_to_int_round (sub_fp(a,b)); //FP 계산 후 int로 변
	}*/
if(idle_thread == t) 
		return;

//priority = PRI_MAX - (recent_cpu / 4) - (nice * 2);
//아래의 변수들은 위 식의 우변의 각 항을 나타냄.
	int term1, term2, term3;

	// 우변의 세 항을 일단 계산
	term1 = int_to_fp(PRI_MAX);
	term2 = div_mixed(t->recent_cpu, 4);
	term3 = int_to_fp(t->nice * 2);
	
	// 왼쪽에서 부터 차례로 뺄셈
	term1 = sub_fp(term1, term2);
	term1 = sub_fp(term1, term3);

	// 다시 int로 바꾸어서 저장
t->priority = fp_to_int(term1);
}

void mlfqs_recent_cpu(struct thread *t)
{
	/*if( t != idle_thread )
	{
		int a = div_fp(mult_mixed(load_avg,2),add_mixed(mult_mixed(load_avg,2),1));
		t->recent_cpu = add_mixed(mult_fp(a,t->recent_cpu),t->nice);
	}*/

		//시발 왜 배껴도 안됨^^
if(idle_thread == t)
		return;
//recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice;
//아래의 변수들을 위 식 우변의 네 개의 항을 각각 나타냄.
	int term1, term2, term3, term4;
	term1 = mult_mixed(load_avg, 2);
	term2 = add_mixed(term1, 1);		// 겹치는 부분이 있어서 term1을 활용함
	term3 = t->recent_cpu;
	term4 = t->nice;

	//왼쪽부터 차례로 계산
	term1 = div_fp(term1, term2);
	term1 = mult_fp(term1, term3);
	term1 = add_mixed(term1, term4);

	//결과값 저장
t->recent_cpu = term1;
} 


void mlfqs_load_avg(void)
{
	/*int size = list_size (&ready_list);
	
	if(thread_current() == idle_thread) size--;	

	load_avg = div_mixed(mult_mixed(load_avg,59),60);
	load_avg = add_fp(mult_mixed(load_avg,1),div_mixed(int_to_fp(size),60));
	if(load_avg<0)load_avg=0;*/

	//load_avg = (59 / 60) * load_avg + (1 / 60) * ready_threads
	//아래의 변수들은 위 식 우변의 네개의 항을 각각 나타냄.
	int term1, term2, term3, term4;
	term1 = div_mixed(int_to_fp(59), 60);
	term2 = load_avg;
	term3 = div_mixed(int_to_fp(1), 60);
	// term4는 ready_list에 있는 thread의 갯수와 현재스레드의 수를 저장하는데
	// 아래의 탐색을 통해 찾는다. 다만 현재idle_thread라면, 더하지 않는다.
	term4 = 1;
	struct list_elem *e;
	for(e = list_begin(&ready_list); e != list_end(&ready_list);
			e = list_next(e))
		term4++;

	if(thread_current() == idle_thread)
		term4--;
	// 각 항을 초기화 했으므로 load_avg를 계산한다
	term1 = mult_fp(term1, term2);
	term3 = mult_mixed(term3, term4);
	term1 = add_fp(term1, term3);

	load_avg = term1;
	
	//load_avg는 0보다 작아질 수 없다.
	if(load_avg < 0)
load_avg = 0;
}


/*우선순위 비교 a가 더 크면 1 반환*/
bool thread_compare_priority ( const struct list_elem* a_,
				const struct list_elem* b_, 
					void* aux UNUSED)
{
	return list_entry(a_, struct thread, elem) -> priority
		> list_entry(b_ , struct thread, elem) -> priority;
}



void thread_sleep(int64_t ticks)
{
	struct thread* thread_cur = thread_current();

	if( thread_cur != idle_thread)
	{
		intr_disable(); // interrupt 불가	
		
		thread_cur-> status = THREAD_BLOCKED; //현재 상태 idle_thread상태 아닐 경우 대기상태로 바꾼다
		thread_cur-> wakeup_tick = ticks; // 깨우는 시간 저장
		list_push_back(&sleep_list, &thread_cur->elem); // 슬립 큐에 저장
		update_next_tick_to_awake(ticks); // 깨우는 시간 업데이트
			
		schedule();
	
		intr_enable(); // interrupt 가능
	}
}



void update_next_tick_to_awake(int64_t ticks)
{
	/*next_tick_to_awake가 ticks보다 크면 ticks로 갱신*/
	if(next_tick_to_awake > ticks)
		next_tick_to_awake = ticks;
}

int64_t get_next_tick_to_awake(void)
{
	return next_tick_to_awake; //next_tick_to_awake(최소 tick) 반환
}


void
thread_init (void)
{
	ASSERT (intr_get_level () == INTR_OFF);
	
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&all_list);
	list_init (&sleep_list); //sleep_list 초기화
	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
Also creates the idle thread. */
void
thread_start (void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);
	load_avg = LOAD_AVG_DEFAULT;

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
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

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
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
and adds it to the ready queue. Returns the thread identifier
for the new thread, or TID_ERROR if creation fails.
If thread_start() has been called, then the new thread may be
scheduled before thread_create() returns. It could even exit
before thread_create() returns. Contrariwise, the original
thread may run for any amount of time before the new thread is
scheduled. Use a semaphore or some other form of
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
	tid = t->tid = allocate_tid ();

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
	
	/* setting hierarchy */
	t -> parent = thread_current();
	
	/*initialize child list*/
	list_init(&t->child_list);
	
	t -> is_embark_memory = 0;
	t -> is_end = 0;

	/*initialize semaphore*/
	sema_init(&t -> exit_sema, 0);
	sema_init(&t -> load_sema, 0);

	list_push_back(&t -> parent -> child_list, &t -> child_elem);

	/*initialize file descriptor table*/
	t -> fd_table = palloc_get_page(0);
	t -> fd = 2;

	/* Add to run queue. */
	thread_unblock (t);
	
	/*만들어진 스레드의 우선순위가 더 클 경우 CPU 양보 */
	if(thread_current () -> priority < priority) thread_yield();
	 
	return tid;
}

/* Puts the current thread to sleep. It will not be scheduled
again until awoken by thread_unblock().
This function must be called with interrupts turned off. It
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
This is an error if T is not blocked. (Use thread_yield() to
make the running thread ready.)

This function does not preempt the running thread. This can
be important: if the caller had disabled interrupts itself,
it may expect that it can atomically unblock a thread and
update other data. */ 

void test_max_priority(void)
{	
	/*우선순위가 가장 높은 첫번째 스레드*/
	if(!list_empty(&ready_list))
	{	
		struct thread* t = list_entry(list_front(&ready_list),struct thread,elem);
		if(thread_current()->priority < t -> priority) thread_yield();
	}
}

void
thread_unblock (struct thread *t)
{
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_insert_ordered (&ready_list, &t->elem, 
				thread_compare_priority,0); // 우선순위대로 삽입
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
	have overflowed its stack. Each thread has less than 4 kB
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

/* Deschedules the current thread and destroys it. Never
returns to the caller. */
void
thread_exit (void)
{
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Remove thread from all threads list, set our status to dying,
	and schedule another process. That process will destroy us
	when it calls thread_schedule_tail(). */
	intr_disable ();
	list_remove (&thread_current()->allelem);

	if(initial_thread!=true) sema_up(&thread_current() -> exit_sema);

	thread_current ()->status = THREAD_DYING;
	schedule ();
	NOT_REACHED ();
}

/* Yields the CPU. The current thread is not put to sleep and
may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
	struct thread *cur = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();

	if (cur != idle_thread)
		list_insert_ordered (&ready_list, &cur->elem
					,thread_compare_priority,0);
		//우선순위대로 삽입으로 수정
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
	if(thread_mlfqs!=true) // mlfqs스케줄러 활성화되지 않을경우
	{	
		intr_disable();
		thread_current ()->priority = new_priority;
		intr_enable();
		test_max_priority(); // 가장 높은 priority와  비교하여 스케줄링
	}
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
	intr_disable();

	struct thread* t = thread_current(); 
	t -> nice = nice;
	mlfqs_priority (t);
	test_max_priority();

	intr_enable();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
	int nice;

	intr_disable();

	nice = thread_current()->nice;

	intr_enable();
 
	return nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
	int load_avg_;

	intr_disable();

	load_avg_= fp_to_int_round(mult_mixed( load_avg,100));

	intr_enable();

	return load_avg_;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
	int recent_cpu_;
	struct thread *t = thread_current();
	intr_disable();

	recent_cpu_= fp_to_int_round(mult_mixed(t->recent_cpu,100));

	intr_enable();

	return recent_cpu_;
}

void thread_awake(int64_t ticks)
{
	struct list_elem* e;// 모든 엔트리 순회하기 위한 조건 저장
	struct thread* entry; // 순회 하는 쓰레드
	
	for(e = list_begin(&sleep_list);
		e != list_end(&sleep_list) ;)
	{
		entry = list_entry ( e, struct thread, elem);

		/*현재 ticks가 깨울 ticks 보다 크면 슬립큐 제거하고  깨운다*/	
		if(entry -> wakeup_tick <= ticks) 
		{
			e = list_remove(&entry->elem);
			thread_unblock(entry);
		}
		/* 현재 ticks가 깨울 ticks 보다 작으면 다음으로*/ 
	
		else{
			update_next_tick_to_awake (entry -> wakeup_tick);
			e = list_next(e);
		}
		
		
	}

	
}


/* Idle thread. Executes when no other thread is ready to run.
The idle thread is initially put on the ready list by
thread_start(). It will be scheduled once initially, at which
point it initializes idle_thread, "up"s the semaphore passed
to it to enable thread_start() to continue, and immediately
blocks. After that, the idle thread never appears in the
ready list. It is returned by next_thread_to_run() as a
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
		instructions are executed atomically. This atomicity is
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

	intr_enable (); /* The scheduler runs with interrupts off. */
	function (aux); /* Execute the thread function. */
	thread_exit (); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
	uint32_t *esp;

	/* Copy the CPU's stack pointer into `esp', and then round that
	down to the start of a page. Because `struct thread' is
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

	/* initialize thread */
	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->stack = (uint8_t *) t + PGSIZE;
	t->priority = priority;
	t->magic = THREAD_MAGIC;
	list_push_back (&all_list, &t->allelem);
	
	list_init(&t->child_list); //initialize child list
	
	t->nice = NICE_DEFAULT; // NICE 초기화 (0)
	t->recent_cpu = RECENT_CPU_DEFAULT; // recnt_cpu 초기화 (0)
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

/* Chooses and returns the next thread to be scheduled. Should
return a thread from the run queue, unless the run queue is
empty. (If the running thread can continue running, then it
will be in the run queue.) If the run queue is empty, return
idle_thread. */
static struct thread *next_thread_to_run (void)
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
still disabled. This function is normally invoked by
thread_schedule() as its final action before returning, but
the first time a thread is scheduled it is called by
switch_entry() (see switch.S).

It's not safe to call printf() until the thread switch is
complete. In practice that means that printf()s should be
added at the end of the function.

After this function and its caller returns, the thread switch
is complete. */
void
thread_schedule_tail (struct thread *prev)
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
	thread. This must happen late so that thread_exit() doesn't
	pull out the rug under itself. (We don't free
	initial_thread because its memory was not obtained via
	palloc().) */
	if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
	{
		ASSERT (prev != cur);
	}
}

/* Schedules a new process. At entry, interrupts must be off and
the running process's state must have been changed from
running to some other state. This function finds another
thread to run and switches to it.

It's not safe to call printf() until thread_schedule_tail()
has completed. */
static void schedule (void)
{
	struct thread *cur = running_thread ();
	struct thread *next = next_thread_to_run ();
	struct thread *prev = NULL;

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (cur->status != THREAD_RUNNING);
	ASSERT (is_thread (next));

	if (cur != next)
		prev = switch_threads (cur, next);
	thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid (void)
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




