/*
 *	File	: pc.c
 *
 *	Title	: Demo Producer/Consumer.
 *
 *	Short	: A solution to the producer consumer problem using
 *		pthreads.
 *
 *	Long 	:
 *
 *	Author	: Andrae Muys
 *
 *	Date	: 18 September 1997
 *
 *	Revised	:
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#import <math.h>
#include <sys/time.h>

#define QUEUESIZE 10
#define LOOP 20

// Constants for number of producer and consumer threads
#define P 12
#define Q 1


// ===== Structures =====

// Work item struct
typedef struct {
    void * (*work)(void *);
    void * arg;
    struct timeval startTime;
}workFunction;

// Queue struct
typedef struct {
    workFunction* buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    bool prodEnd;   // Variable to signify end of production - used to exit consumers
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;


// ===== Function Signatures ======

// Function Signatures - void pointer as argument and as return type
void *producer (void *args);
void *consumer (void *args);

// Function Signatures for Queue and WorkFunction struct functions
queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction* in);
void queueDel (queue *q, workFunction **out);
workFunction *workFunctionInit (void * (*workFunc)(void *), void * arg);
double tenfold(double (*functionPtr)(double));


// ===== Global variables =====

// Array of 6 trigonometric function pointers
double (*funcArr[6])(double) = {&sin, &cos, &tan, &acos, &asin, &atan};

// Timing results array
int resultPtr = 0;
double *timeResults;

// ===== Function Definitions =====

// Entry Point
int main ()
{
    // Run for various combinations of producers (p) and consumers (q)
    experiment(P,Q);

}

// Run experiment
int experiment(int p, int q){

    // Create results array
    timeResults = calloc(p*LOOP, sizeof(double));

    // Create a queue
    queue *fifo;
    fifo = queueInit ();
    if (fifo ==  NULL) {
        fprintf (stderr, "main: Queue Init failed.\n");
        exit (1);
    }

    // Create and run Producer and Consumer threads
    pthread_t *pro = calloc(p, sizeof(pthread_t));
    pthread_t *con = calloc(q, sizeof(pthread_t));
    for(int i = 0; i < p; i++){
        pthread_create (pro + i, NULL, producer, fifo);
    }
    for(int j = 0; j < q; j++){
        pthread_create (con + j, NULL, consumer, fifo);
    }

    // Join Producer Threads
    for(int i = 0; i < p; i++){
        pthread_join (pro[i], NULL);
    }

    // Signal end of production
    fifo->prodEnd = true;
    pthread_cond_broadcast(fifo->notEmpty);

    // Join consumer threads
    for(int j = 0; j < q; j++){
        pthread_join (con[j], NULL);
    }

    // Print results
    for(int k = 0; k < p*LOOP; k++){
        printf("%f\n", timeResults[k]);
    }

    // Delete queue, free results, reset result pointer and return
    resultPtr = 0;
    free(timeResults);
    queueDelete (fifo);
    return 0;
}

// Producer Function
void *producer (void *q)
{
    // Cast received argument as a queue
    queue *fifo;
    fifo = (queue *)q;

    for (int i = 0; i < LOOP; i++) {
        pthread_mutex_lock (fifo->mut);
        while (fifo->full) {
            printf ("producer: queue FULL.\n");
            pthread_cond_wait (fifo->notFull, fifo->mut);
        }
        // Create workFunction struct
        // Select a random trig function and pass it to tenfold()
        workFunction* w = workFunctionInit((void *(*)(void *)) &tenfold, funcArr[rand() % 6]);
        // Print something
        //workFunction* w = workFunctionInit((void *(*)(void *)) &printf, "something...\n");
        queueAdd (fifo, w);
        printf ("producer: added function.\n");
        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
    }
    printf("Exiting producer\n");
    return (NULL);
}

// Consumer Function
void *consumer (void *q)
{
    // Cast received argument as a queue
    queue *fifo;
    fifo = (queue *)q;

    // Variable to hold consumed item
    workFunction* w;

    // End timer
    struct timeval endTime;
    double elapsedTime;

    // Consume
    while(1){
        // Get lock
        pthread_mutex_lock (fifo->mut);
        while ((fifo->empty) && (fifo->prodEnd == false)) {
            printf ("consumer: queue EMPTY.\n");
            pthread_cond_wait (fifo->notEmpty, fifo->mut);
        }
        // Check for end of production
        if (fifo->prodEnd){
            pthread_mutex_unlock (fifo->mut);
            break;
        }

        // Take an item off the queue
        queueDel (fifo, &w);

        // Calculate and write the workFunctions' waiting time in the queue to the results array
        gettimeofday(&endTime, NULL);
        elapsedTime = (endTime.tv_sec - (w->startTime).tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (endTime.tv_usec - (w->startTime).tv_usec) / 1000.0;   // us to ms
        timeResults[resultPtr] = elapsedTime;
        resultPtr++;

        // Run the work
        (w->work)(w->arg);

        // Free memory of consumed item
        free(w);

        printf ("consumer: received function.\n");
        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notFull);
    }
    printf("Exiting consumer\n");
    return (NULL);
}

// Initializer for workFunction
workFunction *workFunctionInit (void * (*workFunc)(void *), void * arg)
{
    workFunction *w;

    w = (workFunction *)malloc (sizeof (workFunction));
    if (w == NULL) return (NULL);

    w->work = workFunc;
    w->arg = arg;
    gettimeofday(&w->startTime, NULL);

    return w;
}

// Tenfold function: Receives a trig function pointer and executes it 10 times with random arguments
double tenfold(double (*functionPtr)(double)){
    double sum = 0;
    srand(time(NULL));
    for(int i = 0; i < 10; i++){
        sum += (double)(*functionPtr)((double)(rand() % 6));
    }
    return sum;
}

// Queue Constructor
queue *queueInit (void)
{
    queue *q;

    q = (queue *)malloc (sizeof (queue));
    if (q == NULL) return (NULL);

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->prodEnd = false;
    q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notEmpty, NULL);

    return (q);
}

// Queue destructor
void queueDelete (queue *q)
{
    pthread_mutex_destroy (q->mut);
    free (q->mut);
    pthread_cond_destroy (q->notFull);
    free (q->notFull);
    pthread_cond_destroy (q->notEmpty);
    free (q->notEmpty);
    free (q);
}

// Add element to queue in a cyclic buffer
void queueAdd (queue *q, workFunction* in)
{
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

// Remove element from queue
void queueDel (queue *q, workFunction **out)
{
    *out = q->buf[q->head];

    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}
