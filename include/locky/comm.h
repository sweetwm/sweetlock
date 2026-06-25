#ifndef LOCKY_COMM_H
#define LOCKY_COMM_H

#include <stdbool.h>
#include <sys/types.h>

struct locky_password;

bool locky_comm_spawn_child(void);

bool locky_comm_write_request(struct locky_password *pw);

ssize_t locky_comm_read_request(char **out);

void locky_comm_free_request(char *buf, size_t size);

bool locky_comm_write_reply(bool success);

bool locky_comm_read_reply(bool *success);

int locky_comm_reply_fd(void);

void locky_comm_finish(void);

#endif
