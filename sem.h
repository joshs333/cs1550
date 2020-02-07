#ifndef CS_1550_SEM_H
#define CS_1550_SEM_H

/**
 * @brief an entry of the priority queue
 **/
struct priority_queue_entry {
	//! task associated with this entry
	struct task_struct *task;
	//! Priorty of this task
	int priority;
};


//! Size of priority queue (limits number of processes able to use a semaphore)
#define CS_1550_PQUEUE_SIZE 255
/**
 * @brief a cs1550 semaphore with a priority queue implemented via a heap
 **/
struct cs1550_sem {
	//! Semaphore value
    int value;
    //! Some priority queue of your devising (heap logic on array)
	/// TODO(joshua.spisak): convert this to a pointer and kmalloc it dynamically?
    struct priority_queue_entry pqueue[CS_1550_PQUEUE_SIZE];
	//! Current slot on queue (unfilled)
    int pqueue_slot;
};

#endif
