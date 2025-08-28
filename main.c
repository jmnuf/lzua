
#define NOB_IMPLEMENTATION
#include "nob.h"

#ifdef _WIN32
#  define PATH_DELIM '\\'
#else
#  define PATH_DELIM '/'
#endif // _WIN32

#include "raylib.h"
#define RGBa(r, g, b, a) ((Color) { (r), (g), (b), (a) })
#define RGB(r, g, b) RGBa((r), (g), (b), 255)

#define WIDTH 600
#define HEIGHT (WIDTH/16.0*9)

#define streq(a, b) (strcmp((a), (b)) == 0)
#define MAX(a, b) ((b) < (a) ? (a) : (b))

#ifndef DEFAULT_DIRECTORY
#  define DEFAULT_DIRECTORY NULL
#endif

typedef struct { int x, y; } Vector2i;

typedef struct {
  const char *folder;
  const char *name;
  const char *exe;
} Game;

typedef struct {
  Game *items;
  size_t count;
  size_t capacity;
} Games;

bool read_games_dir(const char* games_dir, Games *games) {
  bool result = true;
  Nob_File_Paths children = {0};
  Nob_String_Builder sb = {0};

  nob_sb_append_cstr(&sb, games_dir);
  nob_da_append(&sb, PATH_DELIM);
  nob_sb_append_null(&sb);

  if (!nob_read_entire_dir(sb.items, &children)) nob_return_defer(false);
  sb.count -= 1;

  for (int i = children.count-1; i >= 0; --i) {
    const char *dir = children.items[i];
    if (streq(dir, ".") || streq(dir, "..")) continue;

    size_t save = nob_temp_save();
    Nob_File_Paths files = {0};
    const char *path = nob_temp_sprintf("%s%c%s", games_dir, PATH_DELIM, dir);
    if (nob_get_file_type(path) != NOB_FILE_DIRECTORY) continue;
    
    if (!nob_read_entire_dir(path, &files)) nob_return_defer(false);
    nob_da_foreach(const char *, it, &files) {
      const char *f = *it;
      if (streq(dir, ".") || streq(dir, "..")) continue;
      Nob_String_View f_sv = nob_sv_from_cstr(f);
      if (!nob_sv_end_with(f_sv, ".x86_64") && !nob_sv_end_with(f_sv, ".sh")) continue;

      Nob_String_Builder dir_sb = {0};
      nob_sb_append_buf(&dir_sb, sb.items, sb.count);
      nob_sb_append_cstr(&dir_sb, dir);
      nob_da_append(&dir_sb, PATH_DELIM);
      nob_sb_append_null(&dir_sb);

      nob_da_append(games, ((Game) {
	.folder = dir_sb.items,
	.name = strdup(dir),
	.exe = strdup(f),
      }));
    }

    free(files.items);
    nob_temp_rewind(save);
  }

  nob_da_foreach(Game, it, games) {
    nob_log(NOB_INFO, "Found game: %s/%s", it->name, it->exe);
  }

defer:
  if (sb.items) free(sb.items);
  if (children.items) free(children.items);
  return result;
}


void usage(const char *program) {
  printf("Usage: %s <games-directory>\n", program);
}

char *get_home_path() {
  char *env = NULL;
  #ifdef _WIN32
  // This in theory won't ever fail but it is also theoritcally possible to fail, IDK man windows
  env = getenv("USERPROFILE");
  if (env) return strdup(env);

  Nob_String_Builder sb = {0};
  env = getenv("HOMEDRIVE");
  if (!env) return NULL;
  nob_sb_append_cstr(&sb, env);
  env = getenv("HOMEPATH");
  if (!env) return NULL;
  nob_sb_append_cstr(&sb, env);
  nob_sb_append_null(&sb);
  return sb.items;
  #else
  env = getenv("HOME");
  if (!env) return NULL;
  return strdup(env);
  #endif // _WIN32
}

int main(int argc, const char **argv) {
  const char *program = nob_shift(argv, argc);
  (void) program;

  const char *games_dir = NULL;
  while (argc > 0) {
    const char *arg = nob_shift(argv, argc);
    if (arg[0] != '-' && !games_dir) {
      games_dir = arg;
      continue;
    }

    if (arg[0] == '-') {
      nob_log(NOB_ERROR, "Unknown flag passed: %s", arg);
      usage(program);
    } else {
      nob_log(NOB_ERROR, "Unknown command passed: %s", arg);
      usage(program);
    }
    return 1;
  }

  if (!games_dir) {
    games_dir = DEFAULT_DIRECTORY;
    if (!games_dir) {
      nob_log(NOB_ERROR, "Must pass in the folder of the folders of executables");
      return 1;
    }
  }

  const char *home_dir = get_home_path();
  if (!home_dir) {
    nob_log(NOB_ERROR, "Failed to get the home path from the environment. Cannot load custom configs");
    return 1;
  }

  Nob_Cmd cmd = {0};
  Nob_Procs processes = {
    .items = malloc(sizeof(Nob_Proc)), // In theory there shouldn't be more than one
    .capacity = 1,
    .count = 0,
  };
  Games games = {0};
  if (!read_games_dir(games_dir, &games)) return 1;


  SetConfigFlags(FLAG_WINDOW_RESIZABLE);

  SetTraceLogLevel(LOG_WARNING);
  InitWindow(WIDTH, HEIGHT, "Lzua");
  SetWindowMinSize(WIDTH, HEIGHT);

  nob_log(NOB_INFO, "Initialized window (%d, %d)\n", WIDTH, (int)HEIGHT);

  while (!WindowShouldClose()) {
    if (!nob_procs_wait_and_reset(&processes)) {
      // I don't care if the game process stays a zombie, we don't crash ma boi
      nob_log(NOB_ERROR, "Failure happened while waiting for item");
      processes.count = 0;
    }

    BeginDrawing();
    ClearBackground(RGB(12, 12, 12));
    #define PADDING 10

    Rectangle bounds = {
      .x = PADDING, .y = PADDING,
      .width = GetScreenWidth() - PADDING*2,
      .height = GetScreenHeight() - PADDING*2,
    };
    DrawRectangleRec(bounds, RGB(16, 16, 16));

    float base_game_bounds_size = 100;
    int font_size = 5;
    float x = bounds.x + PADDING;
    float y = bounds.y + PADDING;
    Vector2 mouse = GetMousePosition();
    bool hovering_any = false;

    nob_da_foreach(Game, it, &games) {
      const char *name = it->name;
      int width = MeasureText(name, font_size);

      Rectangle game_bounds = {
	x, y,
	MAX(base_game_bounds_size, width + PADDING*2), base_game_bounds_size
      };
      if (game_bounds.x > bounds.x + PADDING/2 && game_bounds.x + game_bounds.width > bounds.x + bounds.width) {
	x = bounds.x + PADDING;
	y += base_game_bounds_size + PADDING;
	game_bounds.x = x;
	game_bounds.y = y;
      }

      Color bg_color;
      if (CheckCollisionPointRec(mouse, game_bounds)) {
	hovering_any = true;
	SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
	bg_color = RGB(80, 80, 80);
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	  size_t save = nob_temp_save();
	  const char *game_cmd = nob_temp_sprintf("./%s", it->exe);
	  nob_cmd_append(&cmd, game_cmd);
	  if (!nob_cmd_run(&cmd, .async = &processes, .cwd_path = it->folder)) {
	    nob_log(NOB_ERROR, "Failed to fork process to open game: %s", it->name);
	  } else {
	    nob_log(NOB_INFO, "Launched: %s", it->name);
	  }
	  nob_temp_rewind(save);
	}
      } else {
	bg_color = RGB(54, 54, 54);
      }
      DrawRectangleRounded(game_bounds, 0.05, 4, bg_color);
      DrawRectangleRoundedLines(game_bounds, 0.05, 4, RGB(200, 100, 150));


      int text_x = game_bounds.x + (game_bounds.width / 2.0 - width / 2.0);
      int text_y = game_bounds.y + game_bounds.height / 2.0;
      DrawText(it->name, text_x, text_y, font_size, RGB(200, 180, 200));

      x += game_bounds.width + PADDING;
      if (x + base_game_bounds_size >= bounds.x + bounds.width) {
	x = bounds.x + PADDING;
	y += base_game_bounds_size + PADDING;
      }
    }

    if (!hovering_any) {
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    EndDrawing();
  }

  CloseWindow();
  
  return 0;
}

