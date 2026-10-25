#pragma once

#include "include/commons.h"

typedef enum : unsigned char {
  READ,
  WRITE,
  APPEND,
} FileMode;

typedef struct {
  int fd;
  String filename;
  FileMode mode;
} Redirect;

static inline void delete_redirect(Redirect redirect) {
  free(redirect.filename);
}

VECTOR_DEFINE(Redirect, delete_redirect)

// Options

typedef enum : unsigned char {
  NONE,
  BACKGROUND,
  AND,
  OR,
  PIPE,
} Operation;

typedef struct {
  Vector_Redirect redirects;
  Operation operation;
} Options;

static inline Options *Options_new(void) {
  Options *options = calloc(1, sizeof(Options));
  if (!options) {
    return nullptr;
  }

  if (!Vector_Redirect_init(&options->redirects)) {
    free(options);
    return nullptr;
  }

  return options;
}

static inline bool Options_init(Options *restrict const options) {
  options->operation = NONE;
  return Vector_Redirect_init(&options->redirects);
}

static inline void Options_free(Options *restrict const options_ref) {
  Vector_Redirect_free(&options_ref->redirects);
}

static inline void Options_delete(Options *options_p) {
  Vector_Redirect_free(&options_p->redirects);
  free(options_p);
}

static void Options_clear(Options *restrict const options_ref) {
  options_ref->operation = NONE;
  Vector_Redirect_clear(&options_ref->redirects);
}

// Command

typedef struct {
  Vector_String argv;
  Options options;
} Command;

static inline Command *Command_new(void) {
  Command *command = calloc(1, sizeof(Command));
  if (!command) {
    return nullptr;
  }

  if (!Vector_String_init(&command->argv)) {
    free(command);
    return nullptr;
  }

  if (!Options_init(&command->options)) {
    Vector_String_free(&command->argv);
    free(command);
    return nullptr;
  }

  return command;
}

static inline void Command_free(Command *restrict const command_ref) {
  Vector_String_free(&command_ref->argv);
  Options_free(&command_ref->options);
}

static inline void Command_delete(Command *command_p) {
  Vector_String_free(&command_p->argv);
  Options_free(&command_p->options);
  free(command_p);
}

static inline void Command_clear(Command *restrict const command_ref) {
  Vector_String_clear(&command_ref->argv);
  Options_clear(&command_ref->options);
}

static inline void Command_destroy(Command command) { Command_free(&command); }

VECTOR_DEFINE(Command, Command_destroy)
