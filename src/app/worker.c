#include "app/worker.h"

#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "log.h"

static void *work(void *data) {
	struct worker *worker = data;
	pthread_mutex_lock(&worker->lock);
	for (;;) {
		while (!worker->queue && !worker->stopping) {
			pthread_cond_wait(&worker->wake, &worker->lock);
		}
		if (worker->stopping) {
			break;
		}
		struct worker_job *job = worker->queue;
		worker->queue = job->next;
		pthread_mutex_unlock(&worker->lock);
		job->run(job);
		pthread_mutex_lock(&worker->lock);
		job->next = worker->done;
		worker->done = job;
		// only fails when the counter would overflow
		eventfd_write(worker->fd, 1);
	}
	pthread_mutex_unlock(&worker->lock);
	return NULL;
}

bool worker_start(struct worker *worker) {
	*worker = (struct worker){.fd = -1};
	worker->fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (worker->fd < 0) {
		log_error("failed to create the render worker's eventfd: %m");
		return false;
	}
	pthread_mutex_init(&worker->lock, NULL);
	pthread_cond_init(&worker->wake, NULL);
	int error = pthread_create(&worker->thread, NULL, work, worker);
	if (error != 0) {
		log_error("failed to start the render worker: %s",
			strerrordesc_np(error));
		pthread_cond_destroy(&worker->wake);
		pthread_mutex_destroy(&worker->lock);
		close(worker->fd);
		*worker = (struct worker){.fd = -1};
		return false;
	}
	return true;
}

// a walk to the tail, since the queue holds a few jobs at most
void worker_submit(struct worker *worker, struct worker_job *job) {
	job->next = NULL;
	pthread_mutex_lock(&worker->lock);
	struct worker_job **tail = &worker->queue;
	while (*tail) {
		tail = &(*tail)->next;
	}
	*tail = job;
	pthread_cond_signal(&worker->wake);
	pthread_mutex_unlock(&worker->lock);
}

struct worker_job *worker_collect(struct worker *worker) {
	eventfd_t count = 0;
	eventfd_read(worker->fd, &count);
	pthread_mutex_lock(&worker->lock);
	struct worker_job *done = worker->done;
	worker->done = NULL;
	pthread_mutex_unlock(&worker->lock);
	return done;
}

void worker_stop(struct worker *worker) {
	if (worker->fd < 0) {
		return;
	}
	pthread_mutex_lock(&worker->lock);
	worker->stopping = true;
	pthread_cond_signal(&worker->wake);
	pthread_mutex_unlock(&worker->lock);
	pthread_join(worker->thread, NULL);
	pthread_cond_destroy(&worker->wake);
	pthread_mutex_destroy(&worker->lock);
	close(worker->fd);
	*worker = (struct worker){.fd = -1};
}
