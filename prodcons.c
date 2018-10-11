/* 
 * Operating Systems  [2INCO]  Practical Assignment
 * Condition Variables Application
 *
 * Nephtaly Aniceta   (0876672)
 * Thijs Jan van Hoek (0944214)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8. 
 * "Extra" steps can lead to higher marks because we want students to take the initiative. 
 * Extra steps can be, for example, in the form of measurements added to your code, a formal 
 * analysis of deadlock freeness etc.
 */
 
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>		// for Semaphores
#include <fcntl.h>          // For O_* constants
#include <sys/stat.h>       // For mode constants
#include <string.h>

#include "prodcons.h"

 // declare a mutex and a condition variable, and they are initialized as well
static pthread_mutex_t      mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t       condition = PTHREAD_COND_INITIALIZER;

pthread_t   thread_id[NROF_PRODUCERS + 1];

sem_t	*sem_full;
sem_t	*sem_empty;
static char semaphoreName_full[50];
static char semaphoreName_empty[50];

static ITEM buffer[BUFFER_SIZE];
static bool empty = 1;					// non-zero when buffer is empty
static bool ready = 0;					// non-zero when all items are handled
static int i = 0;						// position of last entry in buffer
static int j = 0;

static void rsleep (int t);			// already implemented (see below)
static ITEM get_next_item (void);	// already implemented (see below)


/* producer thread */
static void * 
producer (void * arg)
{
	ITEM temp_item = get_next_item();
	fprintf(stderr, "Producer: %lx got item %d\n", pthread_self(), temp_item);

	sem_wait(sem_full);
	pthread_mutex_lock(&mutex);
	buffer[0] = temp_item;
	sem_post(sem_empty);
	pthread_mutex_unlock(&mutex);

    while (!ready)
    {
		sem_wait(sem_full);
        // TODO: 
        // * get the new item

		temp_item = get_next_item();
		fprintf(stderr, "Producer: %lx got item %d\n", pthread_self(), temp_item);

		if (temp_item == NROF_ITEMS) {
			ready = 1;
			fprintf(stderr, "All items are picked\n");
		}
		
        rsleep (100);	// simulating all kind of activities...
		
		// TODO:
		// * put the item into buffer[]
		//
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
        //      while not condition-for-this-producer
        //          wait-cv;
        //      critical-section;
        //      possible-cv-signals;
        //      mutex-unlock;
        //

		i++;
		if  (i >= BUFFER_SIZE){
			i = 0;
		}
		pthread_mutex_lock(&mutex);
		buffer[i] = temp_item;
		sem_post(sem_empty);
		pthread_mutex_unlock(&mutex);


        // (see condition_test() in condition_basics.c how to use condition variables)
    }
	fprintf(stderr, "Producer: %lx exits\n", pthread_self());
	return (NULL);
}

/* consumer thread */
static void * 
consumer (void * arg)
{
	ITEM next_item;

	while (1)
	{
		// TODO: 
		// * get the next item from buffer[]
		// * print the number to stdout
		//
		// follow this pseudocode (according to the ConditionSynchronization lecture):
		//      mutex-lock;
		//      while not condition-for-this-consumer
		//          wait-cv;
		//      critical-section;
		//      possible-cv-signals;
		//      mutex-unlock;

		sem_wait(sem_empty);
		pthread_mutex_lock(&mutex);
		next_item = buffer[j];
		sem_post(sem_full);
		pthread_mutex_unlock(&mutex);

		j++;
		if  (j >= BUFFER_SIZE){
			j = 0;
		}
		printf("%d\n", next_item);
		fprintf(stderr, "\tConsumer: %lx printed item %d\n", pthread_self(), next_item);
		if (next_item == NROF_ITEMS)
			break;
		
        rsleep (100);		// simulating all kind of activities...
    }

	fprintf(stderr, "\tConsumer: %lx exits\n", pthread_self());
	return (NULL);
}

int main (void)
{
	init();
    // TODO: 
    // * startup the producer threads and the consumer thread
    // * wait until all threads are finished  

	// Open a semaphore with the count of max number of threads

	pthread_create(&thread_id[0], NULL, consumer, NULL);
	pthread_create(&thread_id[1], NULL, producer, NULL);

	pthread_join(thread_id[1], NULL);
	pthread_join(thread_id[0], NULL);

	fflush(stdout);
	deinit();
    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void 
rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
}


/* 
 * get_next_item()
 *
 * description:
 *		thread-safe function to get a next job to be executed
 *		subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1 
 *		in arbitrary order 
 *		return value NROF_ITEMS indicates that all jobs have already been given
 * 
 * parameters:
 *		none
 *
 * return value:
 *		0..NROF_ITEMS-1: job number to be executed
 *		NROF_ITEMS:		 ready
 */
static ITEM
get_next_item(void)
{
    static pthread_mutex_t	job_mutex	= PTHREAD_MUTEX_INITIALIZER;
	static bool 			jobs[NROF_ITEMS+1] = { false };	// keep track of issued jobs
	static int              counter = 0;    // seq.nr. of job to be handled
    ITEM 					found;          // item to be returned
	
	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer 
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

    counter++;
	if (counter > NROF_ITEMS)
	{
	    // we're ready
	    found = NROF_ITEMS;
	}
	else
	{
	    if (counter < NROF_PRODUCERS)
	    {
	        // for the first n-1 items: any job can be given
	        // e.g. "random() % NROF_ITEMS", but here we bias the lower items
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
	    }
	    else
	    {
	        // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
	        found = counter - NROF_PRODUCERS;
	        if (jobs[found] == true)
	        {
	            // already handled, find a random one, with a bias for lower items
	            found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
	        }    
	    }
	    
	    // check if 'found' is really an unhandled item; 
	    // if not: find another one
	    if (jobs[found] == true)
	    {
	        // already handled, do linear search for the oldest
	        found = 0;
	        while (jobs[found] == true)
            {
                found++;
            }
	    }
	}
    jobs[found] = true;
			
	pthread_mutex_unlock (&job_mutex);
	return (found);
}

void init() {
	// Create a unique semaphore name
	sprintf(semaphoreName_full, "Semaphore_full_%d", getpid());
	sprintf(semaphoreName_empty, "Semaphore_empty_%d", getpid());

	// Open a semaphore with the count of max number of threads
	sem_empty = sem_open(semaphoreName_empty, O_CREAT, 0777, 0);
	if (sem_empty == SEM_FAILED)
	{
		perror("Creation of semaphore failed");
		exit(1);
	}

	// Open a semaphore with the count of max number of threads
	sem_full = sem_open(semaphoreName_full, O_CREAT, 0777, BUFFER_SIZE);
	if (sem_full == SEM_FAILED)
	{
		perror("Creation of semaphore failed");
		exit(1);
	}
}


void deinit() {
	// Close all semaphores
	if (sem_close(sem_full) != 0)
	{
		perror("Closing of semaphore failed");
		exit(1);
	}

	if (sem_close(sem_empty) != 0)
	{
		perror("Closing of semaphore failed");
		exit(1);
	}

	// Unlink semaphore
	if (sem_unlink(semaphoreName_full) != 0)
	{
		perror("Closing of semaphore failed");
		exit(1);
	}
	// Unlink semaphore
	if (sem_unlink(semaphoreName_empty) != 0)
	{
		perror("Closing of semaphore failed");
		exit(1);
	}
}