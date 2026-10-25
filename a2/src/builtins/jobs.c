#include "../background.h"
#include "commands.h"

void jobs([[maybe_unused]] const ImmutableString *argv) {
  List_BackgroundJob_Node *ptr = background_jobs->head;
  ssize_t i = 0;

  while (ptr != nullptr) {
    BackgroundJob *job = &ptr->value;

    // query
    reap_background_job(job);

    // print
    print_background_job(job, i);

    List_BackgroundJob_Node *next = ptr->next;
    if (job->status == DONE) {
      List_BackgroundJob_delete_node(background_jobs, ptr);
    } else {
      i++;
    }
    ptr = next;
  }
}
