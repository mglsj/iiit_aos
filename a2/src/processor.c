#include "processor.h"

#include "background.h"
#include "builtins.h"
#include "executable.h"
#include "options.h"
#include "parser.h"
#include "pipeline.h"

// empty = no command or file redirect
static bool is_empty_command(const Command *command) {
  return command->argv.size == 0 && command->options.redirects.size == 0 &&
         command->options.operation == NONE;
}

// splits line into commands vector
static bool parse_command_list(ImmutableString buffer,
                               Vector_Command *const commands) {
  for (int offset = 0, consumed; buffer[offset] != '\0'; offset += consumed) {
    Command *command = Command_new();
    if (command == nullptr) {
      goto out_of_memory;
    }

    consumed = args_parser(buffer + offset, command);

    if (consumed < 0) {
      Command_delete(command);
      last_exit_status = 2;
      Vector_Command_clear(commands);
      return false;
    }

    if (is_empty_command(command)) {
      Command_delete(command);
      if (consumed == 0) {
        break;
      }
      continue;
    }

    if (!Vector_Command_push_back(commands, *command)) {
      Command_delete(command);
      goto out_of_memory;
    }
    free(command);

    if (consumed == 0) {
      break;
    }
  }

  // trailing `&&`, `||` or `|`
  if (commands->size > 0) {
    const Operation last = commands->data[commands->size - 1].options.operation;

    if (last == AND || last == OR || last == PIPE) {
      fprintf(stderr, "%s: syntax error: unexpected end of file\n", progname);
      last_exit_status = 2;
      Vector_Command_clear(commands);
      return false;
    }
  }

  return true;

out_of_memory:
  fprintf(stderr, "%s: out of memory\n", progname);
  last_exit_status = 1;
  Vector_Command_clear(commands);
  return false;
}

// index + 1 of pipeline end
static size_t pipeline_end(const Vector_Command *commands, size_t start) {
  size_t end = start;
  while (end + 1 < commands->size &&
         commands->data[end].options.operation == PIPE) {
    end++;
  }
  return end + 1;
}

static bool short_circuits(Operation op) {
  return (op == AND && last_exit_status != 0) ||
         (op == OR && last_exit_status == 0);
}

static void execute_pipeline_or_builtin(size_t n, Command commands[n]) {
  if (n == 1) {
    Command *const command = &commands[0];

    if (command->argv.size == 0) {
      // nothing to run
      apply_redirects_only(command);
      return;
    }

    if (is_builtin(command->argv.data[0])) {
      execute_builtin(command);
      return;
    }
  }

  execute_pipeline(n, commands);
}

static void execute_command_list(Vector_Command *const commands) {
  Operation previous = NONE;

  for (size_t i = 0; i < commands->size;) {
    const size_t end = pipeline_end(commands, i);
    const Operation terminator = commands->data[end - 1].options.operation;

    if (!short_circuits(previous)) {
      execute_pipeline_or_builtin(end - i, &commands->data[i]);
    }

    previous = terminator;
    i = end;
  }
}

void process_command(ImmutableString command_buffer) {
  Vector_Command commands;
  if (!Vector_Command_init(&commands)) {
    fprintf(stderr, "%s: out of memory\n", progname);
    last_exit_status = 1;
    return;
  }

  if (parse_command_list(command_buffer, &commands)) {
    execute_command_list(&commands);
  }

  Vector_Command_free(&commands);

  reap_all_background_jobs();
}
