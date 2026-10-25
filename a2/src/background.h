#pragma once

#include "include/commons.h"

#include <sys/types.h>

typedef enum : unsigned char { RUNNING, STOPPED, DONE } JobStatus;

typedef struct {
  int job_number;
  int pid;
  String command;
  JobStatus status;
} BackgroundJob;

static inline void free_bj(BackgroundJob job) { free(job.command); }

LIST_DEFINE(BackgroundJob, free_bj);

extern List_BackgroundJob *background_jobs;

extern pid_t shell_pgid;

void init_job_control(void);

void give_terminal_to(pid_t pgid);

void print_background_job(const BackgroundJob *const job, const ssize_t index);
void reap_background_job(BackgroundJob *job);
void reap_all_background_jobs();
int get_next_job_number();

void register_background_job(pid_t pid, String label, JobStatus status);
