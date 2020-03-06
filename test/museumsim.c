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
#include "sem.h"
#include "unistd.h"
// I will use pthread semaphores so I can code / test without the qemu machine
// Then switch to the custom implementation for submission
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h> // printf()
#include <stdlib.h> // rand() srand()
#include <sys/mman.h> // mmap()
#include <sys/time.h> // gettimeofday()
#include <string.h> // strcmp()


/************************** Utility Functions    *****************************/
// Borrowed from the homework
void down(struct cs1550_sem *sem) {
    // syscall(__NR_cs1550_down, sem);
    sem_wait((sem_t *) sem);
}

// Borrowed from the homework
void up(struct cs1550_sem *sem) {
    // syscall(__NR_cs1550_up, sem);
    sem_post((sem_t *) sem);
}

// Creates a semaphore and initializes it to a given value
struct cs1550_sem *create_sem(int value) {
    void *new_sem = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    sem_init(new_sem, 1, value);
    return new_sem;
}

// Gets the value of a semaphore
int get_value(struct cs1550_sem *sem) {
    int ret_val;
    sem_getvalue((sem_t *)sem, &ret_val);
    return ret_val;
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
    int remaining_visitors;
    int remaining_tour_guides;
    int prob_visitor;
    int delay_visitor;
    int seed_visitor;
    int prob_guide;
    int delay_guide;
    int seed_guide;
};

// TODO(joshua.spisak): functions to create this struct and to free it at end
struct ProgramState *createProgramState() {
    void *new_state = mmap(NULL, sizeof(struct ProgramState), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

    return new_state;
}

void freeProgramState(struct ProgramState *state) {
    munmap(state, sizeof(struct ProgramState));
}

/************************** Program Functions    *****************************/
struct ProgramState *state;

// Reqs:
// [ ] must block until there is also a tour guide
// [ ] called by visitor process
// [ ] must wait if museum is closed
// [ ] must wait if there is the max # of visitors has been reached
// [ ] must print "Visitor %d arrives at time %d." at arrival
// [ ] first visitor always arrives a time 0
void visitorArrives() {
    // lock(visitors_arrived);
    // visitors_arrived = true;
    // unlock(visitors_arrived);
    // // Tour slots will not be populated until museum is open and tourguideArrives, etc...
    // down(tour_slots);

    // lock(tour_slots_used);
    // ++tour_slots_used;
    // unlock(tour_slots_used);
}

// Reqs:
// [ ] must block until there is also a visitor
// [ ] called by tour guide process
// [ ] must wait if museum is closed
// [ ] must wait if there are already 2 tour guides in the museum
// [ ] must print "Tour guide %d arrives at time %d" at arrival
void tourguideArrives() {
    // lock(opening)
    // if(not opened)
    //     openMuseum()
    // unlock(opening)

    // print("tour guide arrives");
    // for(int i = 0; i < 10; ++i)
    //     up(tour_slots);
}

// Reqs:
// [ ] called after visitorArrives
// [ ] sleeps for two seconds (how long the tour takes)
// [ ] must not block other tourists in the museum
// [ ] must print "Visitor %d tours the museum at time %d."
void tourMuseum() {
    // sleep(2);
}

// Reqs:
// [ ] must be called after tourguideArrives
// [ ] must print "Tour guide %d opens the museum for tours at time %d."
void openMuseum() {
    // print("tour guide opens stuff")
}

// Reqs:
// [ ] must print "Visitor %d leaves the museum at time %d"
void visitorLeaves() {
    // print("Visitor leaves ...")
    // lock(tour_slots_used);
    // --tour_slots_used;
    // if(tour_slots_used == 0)
    //     up(visitors_left);
    //     set tour_slots to 0
    //     sem->value = 0
    // unlock(tour_slots_used);
}

// Reqs:
// [ ] tour guide cannot leave museum until all visitors in museum leave
// [ ] prints "Tour guide %d leaves the museum at time %d"
void tourguideLeaves() {
    // waits for visitor_count to go to 0
    // all tours slots used or all visitors have left (uped a mutex?)
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
    int i;
    for(i = 0; i < state->remaining_visitors; ++i) {
        if(fork() == 0) {
            printf("i am a visitor %d\n", i);
            break;
        } else {
            sleep(1);
        }
    }

    for(i = 0; i < state->remaining_visitors; ++i) {
        wait(NULL);
    }
}

// Reqs:
// [x] creates k tour guide processes
// [ ] when a tour guide arrives there is a pg chance that another tour guide is immediately following them
// [ ] when a tour guide does not arrive there is a dg second delay before the next tour guide arrives
void tourguideProcess() {
    int i;
    for(i = 0; i < state->remaining_tour_guides; ++i) {
        if(fork() == 0) {
            printf("i am a tour guide %d\n", i);
            return;
        } else {
            sleep(1);
        }
    }

    for(i = 0; i < state->remaining_tour_guides; ++i) {
        wait(NULL);
    }
}

/************************** Main Function        *****************************/
int main(int argc, char** argv) {
    // init time to program start
    get_time();
    state = createProgramState();
    get_int_arg(argc, argv, "-m", &state->remaining_visitors);
    get_int_arg(argc, argv, "-t", &state->remaining_tour_guides);
    get_int_arg(argc, argv, "-pv", &state->prob_visitor);
    get_int_arg(argc, argv, "-dv", &state->delay_visitor);
    get_int_arg(argc, argv, "-sv", &state->seed_visitor);
    get_int_arg(argc, argv, "-pg", &state->prob_guide);
    get_int_arg(argc, argv, "-dg", &state->delay_guide);
    get_int_arg(argc, argv, "-sg", &state->seed_guide);

    int pid = fork();
    if(pid == 0) {
        visitorProcess();
    } else {
        pid = fork();
        if(pid == 0) {
            tourguideProcess();
        } else {
            wait(NULL);
            wait(NULL);
        }
    }

    freeProgramState(state);
}
