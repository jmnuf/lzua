#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "./build"

#define streq(a, b) (strcmp(a, b) == 0)

void usage(const char *program) {
  printf("Usage: %s [run|build]\n", program);
  printf("    run        ---        Execute program after compiling\n", program);
  printf("    build      ---        Force building of program\n", program);
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char *program_name = shift(argv, argc);
  bool run_requested = false;
  bool build_demanded = false;
  const char *default_dir = NULL;
  while (argc > 0) {
    const char *arg = shift(argv, argc);

    if (streq(arg, "-def")) {
      default_dir = shift(argv, argc);
      build_demanded = true;
      continue;
    }

    if (streq(arg, "run")) {
      run_requested = true;
      continue;
    }
    if (streq(arg, "build")) {
      build_demanded = true;
      continue;
    }
    
    nob_log(ERROR, "Unknown argument provided to build system: %s", arg);
    usage(program_name);
    return 1;
  }

  Cmd cmd = {0};
  if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

  const char *output_path = BUILD_FOLDER"/lzua";
  if (build_demanded || needs_rebuild1(output_path, "./main.c")) {
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, output_path);
    nob_cc_inputs(&cmd, "main.c");
    nob_cc_inputs(&cmd, "./lib/raylib-5.5_linux_amd64/lib/libraylib.a");
    cmd_append(&cmd, "-I""./lib/raylib-5.5_linux_amd64/include/");
    if (default_dir) {
      cmd_append(&cmd, temp_sprintf("-DDEFAULT_DIRECTORY=\"%s\"", default_dir));
    }
    cmd_append(&cmd, "-lm");
    if (!cmd_run(&cmd)) return 1;
  }

  if (run_requested) {
    cmd_append(&cmd, output_path);
    if (!default_dir) {
      const char *home = getenv("HOME");
      String_Builder sb = {0};
      sb_append_cstr(&sb, home);
      sb_append_cstr(&sb, "/games");
      sb_append_null(&sb);
      cmd_append(&cmd, sb.items);
    }
    if (!cmd_run(&cmd)) return 1;
  }

  return 0;
}
