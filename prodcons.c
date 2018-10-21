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

pthread_t   thread_id[NROF_PRODUCERS + 1]; // Needed threads is # of producers + 1 consumer

static ITEM buffer[BUFFER_SIZE];		// The buffer to be filled by the producers
static bool ready = 0;					// Non-zero when all items are handled
static int i = 0;						// Position of last entry of producer in buffer
static int j = 0;						// Position of last fetch of consumer from buffer
static int items = 0;					// Amount of items in buffer

static void rsleep(int t);				// already implemented (see below)
static ITEM get_next_item(void);		// already implemented (see below)

/* producer thread */
static void *
producer(void * arg)
{
	ITEM temp_item;

	while (!ready)
	{
		// TODO: 
		// * get the new item



		rsleep(100);	// simulating all kind of activities...

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

		pthread_mutex_lock(&mutex);
		while (items >= BUFFER_SIZE) {
			pthread_cond_signal(&condition);	// Signal is sent to prevent deadlocks
			pthread_cond_wait(&condition, &mutex);
		}

		// If ready = TRUE, all items are picked and the thread can be exited
		if (ready) {
			pthread_mutex_unlock(&mutex);
			break;
		}

		// Next item is picked
		temp_item = get_next_item();
		fprintf(stderr, "Producer: %lx got item %d\n", pthread_self(), temp_item);

		if (temp_item == NROF_ITEMS) {
			ready = 1;
			fprintf(stderr, "All items are picked\n");
		}

		// Item is stored in the buffer
		buffer[i] = temp_item;
		items++;

		// Producer iterates over all buffer entries
		i++;
		if (i >= BUFFER_SIZE) {
			i = 0;
		}

		// If items = 1 send signal to consumer. Buffer went from empty state to partly filled state.
		if (items == 1) {
			pthread_cond_signal(&condition);
		}

		pthread_mutex_unlock(&mutex);
	}
	fprintf(stderr, "Producer: %lx exits\n", pthread_self());
	return (NULL);
}

/* consumer thread */
static void *
consumer(void * arg)
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

		pthread_mutex_lock(&mutex);
		while (!items && !ready) {
			pthread_cond_wait(&condition, &mutex);
		}
		next_item = buffer[j];
		buffer[j] = 0;
		items--;

		// If items = BUFFER_SIZE - 1 then signal producer, buffer goes from full state to partly filled.
		if (items == BUFFER_SIZE - 1 && ready == 0) {
			pthread_cond_signal(&condition);
		}

		pthread_mutex_unlock(&mutex);

		// Consumer iterates over all buffer entries
		j++;
		if (j >= BUFFER_SIZE) {
			j = 0;
		}
		printf("%d\n", next_item);
		fprintf(stderr, "\tConsumer: %lx printed item %d\n", pthread_self(), next_item);

		// If last entry is found, end consumer thread.
		if (next_item == NROF_ITEMS)
			break;

		rsleep(100);		// simulating all kind of activities...
	}

	// Extra broadcast is added to signal all waiting producer threads
	pthread_cond_broadcast(&condition);
	fprintf(stderr, "\tConsumer: %lx exits\n", pthread_self());
	return (NULL);
}

int main(void)
{
	init();
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
rsleep(int t)
{
	static bool first_call = true;

	if (first_call == true)
	{
		srandom(time(NULL));
		first_call = false;
	}
	usleep(random() % t);
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
	static pthread_mutex_t	job_mutex = PTHREAD_MUTEX_INITIALIZER;
	static bool 			jobs[NROF_ITEMS + 1] = { false };	// keep track of issued jobs
	static int              counter = 0;    // seq.nr. of job to be handled
	ITEM 					found;          // item to be returned

	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock(&job_mutex);

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
			found = (random() % (2 * NROF_PRODUCERS)) % NROF_ITEMS;
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

	pthread_mutex_unlock(&job_mutex);
	return (found);
}

void init() {
	// Create consumer thread
	pthread_create(&thread_id[0], NULL, consumer, NULL);

	// Create producer threads
	for (int id = 1; id <= NROF_PRODUCERS; id++) {
		pthread_create(&thread_id[id], NULL, producer, NULL);
	}
}

void deinit() {
	// Join all threads
	for (int id = 0; id <= NROF_PRODUCERS; id++) {
		pthread_join(thread_id[id], NULL);
	}

	fflush(stdout);
}