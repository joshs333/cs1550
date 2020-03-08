/******************************************************************************
 * @file museumsim.c
 * @brief simulates a museum with visitors, project 2 for cs1550
 * @author Joshua Spisak <jjs231@pitt.edu>
 * @details Reqs:
 *          [x] must use semaphores
 *          [x] should not busy wait (use a condition variable)
 *          [x] should be deadlock free
 *          [x] tour guides and visitors should be numbered sequentially from 0
 *          [x] When the museum is empty print "The museum is now empty."
 *****************************************************************************/
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
// Returns 0 if it isn't found, 1 if it is.
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


int run_prob(int probability, int mod) {
    return (rand() % mod + 1) < probability; 
}

// inits the time reference on the first call
// returns the seconds passed since the first call, first call returns 0 (duh)
struct timeval tv_start;
int time_init = 0;
int get_time() {
    if(time_init == 0) {
        gettimeofday(&tv_start, NULL);
        time_init = 1;
        // Modify the time once to make calculations easier later
        tv_start.tv_sec += 1; //  upper second mark
        tv_start.tv_usec = 1000000 - tv_start.tv_usec; // additional miscroseconds before upper second mark
    }
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    return (tv_now.tv_sec - tv_start.tv_sec) + ((tv_now.tv_usec + tv_start.tv_usec) / 1000000);
}

/************************** Program State        *****************************/
// all of the variables required for program operation
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

    //! Program Run Variables
    int visitors_present;
    int tour_guides_present;
    int tour_guides_pending;
    int visitors_pending;
    int visitor_slots_available;
    int remaining_visitors;
    int remaining_tour_guides;
    struct cs1550_sem *general_state_sem; // protects several shared variables
    struct cs1550_sem *visitors_arrived; // signals to waiting tour guides when a visitor has arrived
    struct cs1550_sem *opening_sem; // protects museum_opening
    struct cs1550_sem *closeing_sem; // protects museum_closing / tour guide leaving
    struct cs1550_sem *visitor_slots; // used to regulate visitor access to the museum
    struct cs1550_sem *tour_guides; // used to prevent more than 2 tour guides from entering at once
    struct cs1550_sem *empty_museum; // used to alert the tour guides when the musem is emptied
    struct cs1550_sem *visitor_arrival; // prevents issues with opening by only allowing one visitor process
                                        // run entry logic at a time
};

// creates a ProgramState struct and populates defualt values
struct ProgramState *createProgramState() {
    struct ProgramState *new_state = mmap(NULL, sizeof(struct ProgramState), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    new_state->general_state_sem = create_sem(1);
    new_state->visitors_arrived = create_sem(0);
    new_state->opening_sem = create_sem(1);
    new_state->closeing_sem = create_sem(1);
    new_state->visitor_slots = create_sem(0);
    new_state->tour_guides = create_sem(2);
    new_state->empty_museum = create_sem(0);
    new_state->visitor_arrival = create_sem(1);
    new_state->visitors_present = 0;
    new_state->tour_guides_present = 0;
    new_state->tour_guides_pending = 0;
    new_state->visitors_pending = 0;
    new_state->visitor_slots_available = 0;
    new_state->remaining_visitors = 0;
    new_state->remaining_tour_guides = 0;
    return new_state;
}

// frees a ProgramState struct
void freeProgramState(struct ProgramState *state) {
    free_sem(state->general_state_sem);
    free_sem(state->visitors_arrived);
    free_sem(state->opening_sem);
    free_sem(state->visitor_slots);
    free_sem(state->tour_guides);
    free_sem(state->empty_museum);
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
// [x] called by visitor process
// [x] must wait if museum is closed
// [x] must wait if there is the max # of visitors has been reached
// [x] must print "Visitor %d arrives at time %d." at arrival
// [x] first visitor always arrives a time 0
void visitorArrives() {
    int need_sem_again = 0;
    printf("Visitor %d arrives at time %d.\n", visitor_guide_id, get_time());
    down(state->visitor_arrival);
    down(state->general_state_sem);
    state->visitors_pending += 1;
    if(state->visitor_slots_available <= 0) {
        need_sem_again = 1;
        if(state->visitors_pending == 1) // the first pending visitor triggers tour guide processes
            up(state->visitors_arrived);
        up(state->general_state_sem); // free this semaphore
                                      // so other processes can use it to up visitor_slots

    }

    // will not be available until museum is open and a tour guide has arrived
    // can only be upped if there are available visitor slots (max has not been reached)
    down(state->visitor_slots);

    if(need_sem_again)
        down(state->general_state_sem);

    if(state->remaining_tour_guides == 0) {
        up(state->general_state_sem);
        exit(0); // We should exit now because there will be no more tours
    }
    state->visitors_pending -= 1;
    state->visitors_present += 1;
    state->tour_guides_pending = 0;
    state->visitor_slots_available -= 1;
    up(state->general_state_sem);
    up(state->visitor_arrival);
}

// Reqs:
// [x] must block until there is also a visitor
// [x] called by tour guide process
// [x] must wait if museum is closed
// [x] must wait if there are already 2 tour guides in the museum
// [x] must print "Tour guide %d arrives at time %d" at arrival
void tourguideArrives() {
    down(state->tour_guides); // wait if there are already 2
    down(state->general_state_sem);
    printf("Tour guide %d arrives at time %d.\n", visitor_guide_id, get_time());
    up(state->general_state_sem);
}

// Reqs:
// [x] called after visitorArrives
// [x] sleeps for two seconds (how long the tour takes)
// [x] must not block other tourists in the museum
// [x] must print "Visitor %d tours the museum at time %d."
void tourMuseum() {
    // sample again at time of tour to make sure visitors tour in correct order
    printf("Visitor %d tours the museum at time %d.\n", visitor_guide_id, get_time());
    sleep(2);
}

// Reqs:
// [x] must be called after tourguideArrives
// [x] must print "Tour guide %d opens the museum for tours at time %d."
void openMuseum() {
    down(state->opening_sem); // protect museum opening
    down(state->general_state_sem);
    // while(state->tour_guides_present > 0 && state->state->visitor_slots_available == 0) {
    //     // doesn't really prevent an error or deadlock, but forces
    //     // existing tour guides to exit before this one arrives
    //     // and adds more slots (preventing the remaining tour guide
    //     // with no slots from exitting)
    //     // Kinda a busy wait... but really shouldn't happen more than a few loops so...
    //     up(state->general_state_sem);
    //     down(state->closing_sem);
    //     up(state->closing_sem);
    //     down(state->general_state_sem);
    // }
    if(state->remaining_visitors <= 0) {
        up(state->general_state_sem);
        up(state->opening_sem);
        up(state->tour_guides);
        exit(0);
    }
    state->tour_guides_pending = 1;
    state->tour_guides_present += 1;
    if(state->tour_guides_present <= 1 && state->visitors_pending <= 0) {
        up(state->general_state_sem);
        down(state->visitors_arrived);
        down(state->general_state_sem);
    }
    printf("Tour guide %d opens the museum for tours at time %d.\n", visitor_guide_id, get_time());

    state->visitor_slots_available += 10;
    int i;
    for(i = 0; i < 10; ++i)
        up(state->visitor_slots); // Provide 10 slots for visitors
    up(state->general_state_sem);
    up(state->opening_sem);
}

// Reqs:
// [x] must print "Visitor %d leaves the museum at time %d"
void visitorLeaves() {
    down(state->general_state_sem);
    printf("Visitor %d leaves the museum at time %d\n", visitor_guide_id, get_time());
    state->visitors_present -= 1;
    state->remaining_visitors -= 1;
    if(state->visitors_present == 0) {
        // alert the tour guides that all the visitors have left
        up(state->empty_museum);
    }
    up(state->general_state_sem);
}

// Reqs:
// [x] tour guide cannot leave museum until all visitors in museum leave
// [x] prints "Tour guide %d leaves the museum at time %d"
void tourguideLeaves() {
    down(state->closeing_sem);
    down(state->general_state_sem);
    while(1) {
        // catches when visitors have just been woken up but havent gotten the chance
        // to increment visitors present
        int visitors_pending = (state->visitors_pending > 0 && state->tour_guides_pending);
        if(state->visitors_present == 0 && !visitors_pending) {
            // Remove tour guide slots
            while(state->visitor_slots_available > 0) {
                state->visitor_slots_available -= 1;
                down(state->visitor_slots);
            }
            break;
        }
        up(state->general_state_sem);
        // waits for visitor_count to go to 0
        down(state->empty_museum);
        down(state->general_state_sem);
        // between waking and getting the general_state_sem, more visitors may have entered
        // so we will check again at the top of the loop...
    }
    printf("Tour guide %d leaves the museum at time %d\n", visitor_guide_id, get_time());
    state->tour_guides_present -= 1;
    state->remaining_tour_guides -= 1;
    if(state->tour_guides_present == 0) {
        printf("The museum is now empty.\n");
    }
    if(state->remaining_tour_guides == 0) {
        int i = 0;
        for(i = 0; i < state->remaining_visitors + 10; ++i) // this can overshoot...
            up(state->visitor_slots); // wake the waiting visitors so they can exit
    }
    up(state->general_state_sem);
    up(state->closeing_sem);
    up(state->tour_guides);
}

/************************** Process Functions    *****************************/

void visitorProcess() {
    visitorArrives();
    tourMuseum();
    visitorLeaves();
    exit(0);
}

void tourguideProcess() {
    tourguideArrives();
    openMuseum();
    tourguideLeaves();
    exit(0);
}

// Reqs:
// [x] creates m visitor processes
// [x] when a visitor arrives there is a pv (eg 70%) chance that another visitor will arrive directly after
// [x] if no visitor arrives there is a dv second wait until the next visitor arrives
// [x] first visitor always arrives a time 0
//
// Needs:
//   m (number of visitors)
//   pv (percent chance of an arrival following a given one)
//   dv (wait until next arrival)
void visitorProcessSpawner() {
    srand(state->seed_visitor);
    int i;
    for(i = 0; i < state->visitor_count; ++i) {
        if(fork() == 0) {
            visitor_guide_id = i;
            visitorProcess();
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
// [x] when a tour guide arrives there is a pg chance that another tour guide is immediately following them
// [x] when a tour guide does not arrive there is a dg second delay before the next tour guide arrives
void tourguideProcessSpawner() {
    srand(state->seed_guide);
    int i;
    for(i = 0; i < state->tour_guide_count; ++i) {
        if(fork() == 0) {
            visitor_guide_id = i;
            tourguideProcess();
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
    // we assume we do in fact get these arguments... 
    // even though we don't care what order we get them in
    // we won't set default values
    get_int_arg(argc, argv, "-m", &state->visitor_count);
    get_int_arg(argc, argv, "-k", &state->tour_guide_count);
    get_int_arg(argc, argv, "-pv", &state->prob_visitor);
    get_int_arg(argc, argv, "-dv", &state->delay_visitor);
    get_int_arg(argc, argv, "-sv", &state->seed_visitor);
    get_int_arg(argc, argv, "-pg", &state->prob_guide);
    get_int_arg(argc, argv, "-dg", &state->delay_guide);
    get_int_arg(argc, argv, "-sg", &state->seed_guide);
    state->remaining_tour_guides = state->tour_guide_count;
    state->remaining_visitors = state->visitor_count;

    // because of course it is
    printf("The museum is now empty.\n");
    int pid = fork();
    if(pid == 0) {
        tourguideProcessSpawner();
    } else {
        pid = fork();
        if(pid == 0) {
            visitorProcessSpawner();
        } else {
            wait(NULL);
            wait(NULL);
        }
    }

    // clean up memory
    freeProgramState(state);
}
