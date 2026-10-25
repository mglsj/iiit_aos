#include "background.h"

#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

List_BackgroundJob *background_jobs;
pid_t shell_pgid;

static bool interactive;

void init_job_control(void) {
  interactive = isatty(STDIN_FILENO);

  /* The shell must ignore the signals job control aims at whoever owns the
     terminal*/
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);

  if (!interactive) {
    return;
  }

  shell_pgid = getpid();

  if (setpgid(shell_pgid, shell_pgid) == -1) {
    // already a group leader, or not permitted
    shell_pgid = getpgrp();
  }

  tcsetpgrp(STDIN_FILENO, shell_pgid);
}

void give_terminal_to(pid_t pgid) {
  if (interactive && pgid > 0) {
    tcsetpgrp(STDIN_FILENO, pgid);
  }
}

int get_next_job_number() {
  int i = 1;
  List_BackgroundJob_Node *ptr = background_jobs->head;

  while (ptr != nullptr) {
    BackgroundJob *job = &ptr->value;
    if (i != job->job_number) {
      return i;
    }
    ptr = ptr->next;
    i++;
  }
  return i;
}

void register_background_job(pid_t pid, String label, JobStatus status) {
  const int job_number = get_next_job_number();

  BackgroundJob job = {
      .job_number = job_number, .pid = pid, .status = status, .command = label};

  if (!List_BackgroundJob_insert_at(background_jobs, job_number - 1, job)) {
    fprintf(stderr, "%s: cannot track background job %d\n", progname, pid);
    free(label);
    return;
  }

  if (status == STOPPED) {
    // Ctrl-Z
    print_background_job(&job, (ssize_t)background_jobs->size - 1);
  } else {
    printf("[%d] %d\n", job.job_number, job.pid);
  }
}

void reap_background_job(BackgroundJob *const job) {
  if (job->status == DONE) {
    return;
  }

  int status;
  pid_t w = waitpid(job->pid, &status, WNOHANG);

  // no such child
  if (w == -1) {
    job->status = DONE;
    return;
  }

  if (w > 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
    job->status = DONE;
  }
}

void print_background_job(const BackgroundJob *const job, const ssize_t index) {
  printf("[%d]", job->job_number);

  switch (background_jobs->size - index) {
  case 1:
    printf("+");
    break;
  case 2:
    printf("-");
    break;
  default:
    printf(" ");
  }
  printf("  ");
  switch (job->status) {
  case RUNNING:
    printf("%-27s", "Running");
    break;
  case STOPPED:
    printf("%-27s", "Stopped");
    break;
  case DONE:
    printf("%-27s", "Done");
    break;
  }
  printf("%s", job->command != nullptr ? job->command : "(unknown)");

  if (job->status == RUNNING) {
    printf(" &");
  }
  printf("\n");
  fflush(stdout);
}

void reap_all_background_jobs() {
  size_t i = 0;
  List_BackgroundJob_Node *ptr = background_jobs->head, *next;

  // Reap tracked jobs by pid
  for (List_BackgroundJob_Node *p = background_jobs->head; p != nullptr;
       p = p->next) {
    reap_background_job(&p->value);
  }

  while (waitpid(-1, nullptr, WNOHANG) > 0)
    ;

  while (ptr != nullptr) {
    BackgroundJob *job = &ptr->value;

    next = ptr->next;
    if (job->status == DONE) {
      print_background_job(job, i);
      List_BackgroundJob_delete_node(background_jobs, ptr);
    } else {
      i++;
    }
    ptr = next;
  }
}
