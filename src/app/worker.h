#ifndef SWEETLOCK_APP_WORKER_H
#define SWEETLOCK_APP_WORKER_H

#include <pthread.h>
#include <stdbool.h>

// owned by the code that submits it, and untouched by it until collected
struct worker_job {
	struct worker_job *next;
	void (*run)(struct worker_job *job);
};

// the one helper thread: decodes and renders off the main loop
struct worker {
	int fd;
	bool stopping;
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t wake;
	struct worker_job *queue;
	struct worker_job *done;
};

// fd is -1 until this succeeds, and again after worker_stop
bool worker_start(struct worker *worker);

// jobs run in the order they were submitted
void worker_submit(struct worker *worker, struct worker_job *job);

// call when fd is readable; the finished jobs, newest first
struct worker_job *worker_collect(struct worker *worker);

// waits for a job in progress; queued jobs never run and stay their owners'
void worker_stop(struct worker *worker);

#endif
