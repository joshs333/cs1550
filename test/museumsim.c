/******************************************************************************
 * @file museumsim.c
 * @brief simulates a museum with visitors, project 2 for cs1550
 * @author Joshua Spisak <jjs231@pitt.edu>
 * @details Reqs:
 *          [ ] must use semaphores
 *          [ ] should not busy wait (use a condition variable)
 *          [ ] should be deadlock free
 *          [ ] tour guides and visitors should be numbered sequentially from 0
 *          [ ] When the museum is empty print "The museum is now empty."
 *****************************************************************************/
// #define USE_PTHREAD true // Allows debugging without using qemu
#include "sem.h"
#include "unistd.h"
#ifdef USE_PTHREAD
    #include <pthread.h>
    #include <semaphore.h>
#endif
#include <stdio.h> // printf()
#include <stdlib.h> // rand() srand()
#include <sys/mman.h> // mmap()
#include <sys/time.h> // gettimeofday()
#include <string.h> // strcmp()


/************************** Utility Functions    *****************************/
// downs/waits a semaphore
void down(struct cs1550_sem *sem) {
    #ifdef USE_PTHREAD
        sem_wait((sem_t *) sem);
    #else
        syscall(__NR_cs1550_down, sem);
    #endif
}

// Ups/posts a semaphore
void up(struct cs1550_sem *sem) {
    #ifdef USE_PTHREAD
        sem_post((sem_t *) sem);
    #else
        syscall(__NR_cs1550_up, sem);
    #endif
}

// Creates a semaphore and initializes it to a given value
struct cs1550_sem *create_sem(int value) {
    #ifdef USE_PTHREAD
        void *new_sem = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
        sem_init(new_sem, 1, value);
        return new_sem;
    #else
        struct cs1550_sem *new_sem = mmap(NULL, sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
        new_sem->value = value;
        return new_sem;
    #endif
}

// Frees a semaphore
void free_sem(struct cs1550_sem *sem) {
    #ifdef USE_PTHREAD
        sem_destroy((sem_t *)sem);
        munmap(sem, sizeof(sem_t));
    #else
        munmap(sem, sizeof(struct cs1550_sem));
    #endif
}

// Gets the value of a semaphore
int get_value(struct cs1550_sem *sem) {
    #ifdef USE_PTHREAD
        int ret_val;
        sem_getvalue((sem_t *)sem, &ret_val);
        return ret_val;
    #else
        return sem->value;
    #endif
}

// Gets an integer from command line arguments
int get_int_arg(int argc, char** argv, char* flag, int *ret_val) {
    int i;
    for(i = 0; i < argc; ++i) {
        if(strcmp(argv[i], flag) == 0) {
            if(i + 1 >= argc) {
                return 0;
            }
            *ret_val = atoi(argv[i + 1]);
            return 1;
        }
    }
    return 0;
}

// TODO(joshua.spisak): random function (integer percent probability)
int run_prob(int probability, int mod) {
    return (rand() % mod) < probability; 
}

// TODO(joshua.spisak): function to start clock and sample time
struct timeval tv_start;
int time_init = 0;
int get_time() {
    if(time_init == 0) {
        gettimeofday(&tv_start, NULL);
        time_init = 1;
        // Modify the time once to make calculations easier later
        tv_start.tv_sec += 1;
        tv_start.tv_usec = 1000000 - tv_start.tv_usec;
    }
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    return (tv_now.tv_sec - tv_start.tv_sec) + ((tv_now.tv_usec + tv_start.tv_usec) / 1000000);
}

/************************** Program State        *****************************/
// TODO(joshua.spisak): define structs containing all of the program state
//      variables that can all be allocated at once
struct ProgramState {
    //! Program arguments
    int visitor_count;
    int tour_guide_count;
    int prob_visitor;
    int delay_visitor;
    int seed_visitor;
    int prob_guide;
    int delay_guide;
    int seed_guide;
    int museum_opened;
    struct cs1550_sem *visitors_arrived;
    struct cs1550_sem *opening_sem;
    struct cs1550_sem *visitor_slots;
    struct cs1550_sem *tour_guides;
};

// TODO(joshua.spisak): functions to create this struct and to free it at end
struct ProgramState *createProgramState() {
    struct ProgramState *new_state = mmap(NULL, sizeof(struct ProgramState), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    new_state->visitors_arrived = create_sem(0);
    new_state->opening_sem = create_sem(1);
    new_state->visitor_slots = create_sem(0);
    new_state->tour_guides = create_sem(2);
    new_state->museum_opened = 0;
    return new_state;
}

void freeProgramState(struct ProgramState *state) {
    free_sem(state->visitors_arrived);
    free_sem(state->opening_sem);
    free_sem(state->visitor_slots);
    free_sem(state->tour_guides);
    munmap(state, sizeof(struct ProgramState));
}

/************************** Program Functions    *****************************/
// Data that is needed to run the different functions
// Shared between processes
struct ProgramState *state;
// Specific to each process
int visitor_guide_id;

// Reqs:
// [x] must block until there is also a tour guide
// [ ] called by visitor process
// [ ] must wait if museum is closed
// [ ] must wait if there is the max # of visitors has been reached
// [x] must print "Visitor %d arrives at time %d." at arrival
// [x] first visitor always arrives a time 0
void visitorArrives() {
    printf("Visitor %d arrives at time %d.\n", visitor_guide_id, get_time());
    up(state->visitors_arrived);
    down(state->visitor_slots);
}

// Reqs:
// [x] must be called after tourguideArrives
// [x] must print "Tour guide %d opens the museum for tours at time %d."
void openMuseum() {
    printf("Tour guide %d opens the museum for tours at time %d.\n", visitor_guide_id, get_time());
}

// Reqs:
// [x] must block until there is also a visitor
// [x] called by tour guide process
// [x] must wait if museum is closed
// [x] must wait if there are already 2 tour guides in the museum
// [x] must print "Tour guide %d arrives at time %d" at arrival
void tourguideArrives() {
    printf("Tour guide %d arrives at time %d.\n", visitor_guide_id, get_time());
    down(state->tour_guides); // wait if there are already 2
    down(state->opening_sem); // protect museum opening
    if(!state->museum_opened) { // will only happen once
        down(state->visitors_arrived);
        state->museum_opened = 1;
        openMuseum();
    }
    up(state->opening_sem);

    int i;
    for(i = 0; i < 10; ++i)
        up(state->visitor_slots); // Provide 10 slots for visitors
}

// Reqs:
// [x] called after visitorArrives
// [x] sleeps for two seconds (how long the tour takes)
// [x] must not block other tourists in the museum
// [x] must print "Visitor %d tours the museum at time %d."
void tourMuseum() {
    printf("Visitor %d tours the museum at time %d.\n", visitor_guide_id, get_time());
    sleep(2);
}

// Reqs:
// [x] must print "Visitor %d leaves the museum at time %d"
void visitorLeaves() {
    printf("Visitor %d leaves the museum at time %d\n", visitor_guide_id, get_time());
    // Provide mechanism to alert tourguide that noone is present
}

// Reqs:
// [ ] tour guide cannot leave museum until all visitors in museum leave
// [ ] prints "Tour guide %d leaves the museum at time %d"
void tourguideLeaves() {
    // waits for visitor_count to go to 0

    printf("Tour guide %d leaves the museum at time %d\n", visitor_guide_id, get_time());
    up(state->tour_guides);
}


/************************** Process Functions    *****************************/

// Reqs:
// [x] creates m visitor processes
// [ ] when a visitor arrives there is a pv (70%) chance that another visitor will arrive directly after
// [ ] if no visitor arrives there is a dv second wait until the next visitor arrives
// [ ] first visitor always arrives a time 0
//
// Needs:
//   m (number of visitors)
//   pv (percent chance of an arrival following a given one)
//   dv (wait until next arrival)
void visitorProcess() {
    srand(state->seed_visitor);
    int i;
    for(i = 0; i < state->visitor_count; ++i) {
        if(fork() == 0) {
            visitor_guide_id = i + 1;
            visitorArrives();
            tourMuseum();
            visitorLeaves();
            return;
        } else {
            if(!run_prob(state->prob_visitor, 100)) {
                sleep(state->delay_visitor);
            }
        }
    }

    for(i = 0; i < state->visitor_count; ++i) {
        wait(NULL);
    }
}

// Reqs:
// [x] creates k tour guide processes
// [ ] when a tour guide arrives there is a pg chance that another tour guide is immediately following them
// [ ] when a tour guide does not arrive there is a dg second delay before the next tour guide arrives
void tourguideProcess() {
    srand(state->seed_guide);
    int i;
    for(i = 0; i < state->tour_guide_count; ++i) {
        if(fork() == 0) {
            visitor_guide_id = i + 1;
            tourguideArrives();
            tourguideLeaves();
            return;
        } else {
            if(!run_prob(state->prob_guide, 100)) {
                sleep(state->delay_guide);
            }
        }
    }

    for(i = 0; i < state->tour_guide_count; ++i) {
        wait(NULL);
    }
}

/************************** Main Function        *****************************/
int main(int argc, char** argv) {
    // init time to program start
    get_time();
    state = createProgramState();
    get_int_arg(argc, argv, "-m", &state->visitor_count);
    get_int_arg(argc, argv, "-t", &state->tour_guide_count);
    get_int_arg(argc, argv, "-pv", &state->prob_visitor);
    get_int_arg(argc, argv, "-dv", &state->delay_visitor);
    get_int_arg(argc, argv, "-sv", &state->seed_visitor);
    get_int_arg(argc, argv, "-pg", &state->prob_guide);
    get_int_arg(argc, argv, "-dg", &state->delay_guide);
    get_int_arg(argc, argv, "-sg", &state->seed_guide);

    // museum is currently empty...
    printf("The museum is now empty.\n");
    int pid = fork();
    if(pid == 0) {
        tourguideProcess();
    } else {
        pid = fork();
        if(pid == 0) {
            visitorProcess();
        } else {
            wait(NULL);
            wait(NULL);
        }
    }
    printf("The museum is now empty.\n");

    freeProgramState(state);
}
