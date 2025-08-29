
#if defined(_MSC_VER) && !defined(__clang__)
#    define nob_cc_flags(cmd) nob_cmd_append(cmd, "/W4", "/nologo", "/wd4244", "/wd4819", "/D_CRT_NONSTDC_NO_WARNINGS")
#    define cc_debug(cmd) nob_cmd_append(cmd, "/Od", "/Ob1", "/DEBUG", "/Zi", "/DBUILD_DEBUG=1")
#else
#    define cc_debug(cmd) nob_cmd_append(cmd, "-O0", "-ggdb", "-DBUILD_DEBUG=1")
#endif // nob_cc_output

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "./build"

#ifdef _WIN32
#  define CC_INCLUDE_FLAG "/I"
#else
#  define CC_INCLUDE_FLAG "-I"
#endif // _WIN32

#define streq(a, b) (strcmp(a, b) == 0)

void usage(const char *program) {
  printf("Usage: %s [run|build] [FLAGS]\n", program);
  printf("Commands\n");
  printf("    run        ---        Execute program after compiling\n");
  printf("    build      ---        Force building of program\n");
  printf("Flags\n");
  printf("    -def <dir> ---        Build program with a default search path for apps\n");
  printf("    -debug     ---        Include debug data in rebuild\n");
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char *program_name = shift(argv, argc);
  bool run_requested = false;
  bool build_demanded = false;
  bool include_debug = false;
  const char *default_dir = NULL;
  while (argc > 0) {
    const char *arg = shift(argv, argc);

    if (streq(arg, "-def")) {
      default_dir = shift(argv, argc);
      build_demanded = true;
      continue;
    }

    if (streq(arg, "-dbg")) {
      include_debug = true;
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
    if (default_dir) {
      cmd_append(&cmd, temp_sprintf("-DDEFAULT_DIRECTORY=\"%s\"", default_dir));
    }
    cmd_append(&cmd, CC_INCLUDE_FLAG"./lib/raylib-5.5/include/");
    if (include_debug) cc_debug(&cmd);
    #ifdef _WIN32
    nob_cc_inputs(&cmd, "./lib/raylib-5.5/win32-msvc16/raylib.lib");
    nob_cc_inputs(&cmd, "opengl32.lib", "msvcrt.lib", "kernel32.lib", "user32.lib", "winmm.lib", "gdi32.lib", "shell32.lib");
    cmd_append(&cmd, "/link", "/NODEFAULTLIB:LIBCMT");
    #else
    nob_cc_inputs(&cmd, "./lib/raylib-5.5/linux-amd64/libraylib.a");
    cmd_append(&cmd, "-lm");
    #endif

    if (!cmd_run(&cmd)) return 1;
    nob_log(INFO, "Built succesfully");
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
