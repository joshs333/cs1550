typedef struct queueTask
{
    struct task_struct* task;
    int priority;
} queueTask;

typedef struct queue
{
	int length;
	unsigned max_length;
	struct queueTask** taskHeap;
} queue;

typedef struct cs1550_sem
{
    int value;
    //Some priority queue of your devising
    struct queue* semQueue;
} cs1550_sem;