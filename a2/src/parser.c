#include "parser.h"
#include "helpers.h"
#include "options.h"

#include <unistd.h>

#define NUL '\0'
#define SINGLE_QUOTE '\''
#define DOUBLE_QUOTE '"'
#define WHITE_SPACE ' '
#define TAB_SPACE '\t'
#define BACKSLASH '\\'
#define LESS_THAN '<'
#define GREATER_THAN '>'
#define AMP '&'
#define SEMICOLON ';'
#define BAR '|'

static bool is_token_terminator(char c) {
  return c == NUL || c == WHITE_SPACE || c == TAB_SPACE || c == GREATER_THAN ||
         c == LESS_THAN || c == AMP || c == BAR || c == SEMICOLON;
}

// Parser state
typedef struct {
  ImmutableString input;
  size_t at;          // read position in input
  Vector_char token;  // characters of the word being built
  bool in_token;      // whether token has been started
  bool awaiting_file; // expecting filename after a redirect
  Command *command;
} Parser;

// failure cleanup
static void parser_unwind(Parser *p) {
  Vector_char_free(&p->token);
  Command_clear(p->command);
}

// failure
[[nodiscard]] static int parser_fail(Parser *p, ImmutableString message) {
  parser_unwind(p);
  fprintf(stderr, "%s: %s\n", progname, message);
  return -1;
}

// OOM Error
[[nodiscard]] static int out_of_memory(Parser *p) {
  return parser_fail(p, "out of memory while parsing");
}

// Syntax Errorr
[[nodiscard]] static int unexpected_token(Parser *p, ImmutableString token) {
  parser_unwind(p);
  fprintf(stderr, "%s: syntax error near unexpected token `%s'\n", progname,
          token);
  return -1;
}

static ImmutableString operator_text(char c, bool doubled) {
  switch (c) {
  case SEMICOLON:
    return ";";
  case AMP:
    return doubled ? "&&" : "&";
  case BAR:
    return doubled ? "||" : "|";
  default:
    unreachable();
  }
}

// Rinish current token -> args or filename
[[nodiscard]] static bool finish_token(Parser *p) {
  String word = strdup(p->token.data);
  if (word == nullptr) {
    return false;
  }

  if (p->awaiting_file) {
    Vector_Redirect *redirects = &p->command->options.redirects;
    redirects->data[redirects->size - 1].filename = word;
    p->awaiting_file = false;
  } else if (!Vector_String_push_back(&p->command->argv, word)) {
    free(word);
    return false;
  }

  Vector_char_clear(&p->token);
  p->in_token = false;
  return true;
}

// consume signgle quoted token
[[nodiscard]] static size_t take_single_quoted(Parser *p, size_t start) {
  size_t end = start + 1;
  while (p->input[end] != NUL && p->input[end] != SINGLE_QUOTE) {
    end++;
  }

  // unterminated quote
  if (p->input[end] != SINGLE_QUOTE) {
    return 0;
  }

  for (size_t k = start + 1; k < end; k++) {
    if (!Vector_char_push_back(&p->token, p->input[k])) {
      return 0;
    }
  }

  return end + 1;
}

// consume double quoted token
[[nodiscard]] static size_t take_double_quoted(Parser *p, size_t start) {
  const ImmutableString in = p->input;

  size_t end = start + 1;
  while (in[end] != NUL && in[end] != DOUBLE_QUOTE) {
    // `\\` or `\"`
    bool escaped = in[end] == BACKSLASH &&
                   (in[end + 1] == DOUBLE_QUOTE || in[end + 1] == BACKSLASH);
    end += escaped ? 2 : 1;
  }

  // unterminated
  if (in[end] != DOUBLE_QUOTE) {
    return 0;
  }

  size_t k = start + 1;
  while (k < end) {
    bool escaped = in[k] == BACKSLASH &&
                   (in[k + 1] == DOUBLE_QUOTE || in[k + 1] == BACKSLASH);

    if (!Vector_char_push_back(&p->token, in[escaped ? k + 1 : k])) {
      return 0;
    }
    k += escaped ? 2 : 1;
  }

  return end + 1;
}

// consume output redirects > >>
[[nodiscard]] static bool start_output_redirect(Parser *p) {
  Vector_String *argv = &p->command->argv;
  int fd = STDOUT_FILENO;

  if (argv->size > 0 && p->at > 0 &&
      isdigit((unsigned char)p->input[p->at - 1])) {
    String last = argv->data[argv->size - 1];
    if (is_number(last)) {
      fd = atoi(last);
      Vector_String_pop_back(argv);
      free(last);
    }
  }

  FileMode mode = WRITE;
  if (p->input[p->at + 1] == GREATER_THAN) {
    p->at++;
    mode = APPEND;
  }

  Redirect redirect = {.fd = fd, .filename = nullptr, .mode = mode};
  p->at++;
  p->awaiting_file = true;
  return Vector_Redirect_push_back(&p->command->options.redirects, redirect);
}

// consume input redirect <
[[nodiscard]] static bool start_input_redirect(Parser *p) {
  Redirect redirect = {.fd = STDIN_FILENO, .filename = nullptr, .mode = READ};

  p->at++;
  p->awaiting_file = true;
  return Vector_Redirect_push_back(&p->command->options.redirects, redirect);
}

// check command is not empty
static bool command_is_started(const Parser *p) {
  return p->command->argv.size > 0 || p->command->options.redirects.size > 0;
}

// Prase inputs args string and returns the first command and count of bytes
// read
int args_parser(ImmutableString const args, Command *command) {
  Command_clear(command);

  Parser p = {.input = args,
              .at = 0,
              .in_token = false,
              .awaiting_file = false,
              .command = command};

  if (!Vector_char_init(&p.token)) {
    return -1;
  }

  int consumed = 0;

  while (true) {
    const char c = p.input[p.at];

    if (p.in_token && is_token_terminator(c)) {
      if (!finish_token(&p)) {
        return out_of_memory(&p);
      }
    }

    /* --- end of input --- */

    if (c == NUL) {
      if (p.awaiting_file) {
        return parser_fail(
            &p, "unexpected EOF while looking for redirected filename");
      }
      consumed = (int)p.at;
      break;
    }

    /* --- operators: each ends this command --- */

    if (c == SEMICOLON || c == AMP || c == BAR) {
      const bool doubled = c != SEMICOLON && p.input[p.at + 1] == c;

      if (p.awaiting_file) {
        return unexpected_token(&p, operator_text(c, doubled));
      }

      if (c == SEMICOLON) {
        consumed = (int)p.at + 1;
        break;
      }

      // empty command not allowed
      if (!command_is_started(&p)) {
        return unexpected_token(&p, operator_text(c, doubled));
      }

      if (c == AMP) {
        command->options.operation = doubled ? AND : BACKGROUND;
      } else {
        command->options.operation = doubled ? OR : PIPE;
      }
      consumed = (int)p.at + (doubled ? 2 : 1);
      break;
    }

    /* --- separators and redirects --- */

    if (c == WHITE_SPACE || c == TAB_SPACE) {
      p.at++; // ignore
      continue;
    }

    if (c == GREATER_THAN) {
      if (!start_output_redirect(&p)) {
        return out_of_memory(&p);
      }
      continue;
    }

    if (c == LESS_THAN) {
      if (!start_input_redirect(&p)) {
        return out_of_memory(&p);
      }
      continue;
    }

    /* --- ordinary word characters --- */

    p.in_token = true;

    if (c == BACKSLASH && p.input[p.at + 1] != NUL) {
      if (!Vector_char_push_back(&p.token, p.input[p.at + 1])) {
        return out_of_memory(&p);
      }
      p.at += 2;
      continue;
    }

    if (c == SINGLE_QUOTE || c == DOUBLE_QUOTE) {
      const size_t next = (c == SINGLE_QUOTE) ? take_single_quoted(&p, p.at)
                                              : take_double_quoted(&p, p.at);
      if (next == 0) {
        parser_unwind(&p);
        fprintf(stderr, "%s: unexpected EOF while looking for matching `%c'\n",
                progname, c);
        return -1;
      }
      p.at = next;
      continue;
    }

    if (!Vector_char_push_back(&p.token, c)) {
      return out_of_memory(&p);
    }
    p.at++;
  }

  Vector_char_free(&p.token);
  return consumed;
}
