
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

#define MIN_WIDTH 600
#define MIN_HEIGHT ((int)(MIN_WIDTH/16.0*9))

#define WIDTH 800
#define HEIGHT ((int)(WIDTH/16.0*9))

#define streq(a, b) (strcmp((a), (b)) == 0)
#define MAX(a, b) ((b) < (a) ? (a) : (b))

#ifndef DEFAULT_DIRECTORY
#  define DEFAULT_DIRECTORY NULL
#endif

#define List(T) \
struct { \
  T *items; \
  size_t count; \
  size_t capacity; \
}

typedef enum {
  BUTTON_STATE_NONE  = 0x00,
  BUTTON_STATE_HOVER = 0x01,
  BUTTON_STATE_CLICK = 0x10,
} Button_State;

typedef struct {
  const char *folder;
  const char *name;
  const char *exe;
} Game;

typedef List(Game) Games;

typedef struct {
  Game data;
  float x, y;
  float width, height;
  int title_width;
} GameButton;


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
      #ifdef _WIN32
      if (!nob_sv_end_with(f_sv, ".exe")) continue;
      #else
      if (!nob_sv_end_with(f_sv, ".x86_64") && !nob_sv_end_with(f_sv, ".sh")) continue;
      #endif // _WIN32

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


#define GENERAL_PADDING 10.0f

#define GAME_BUTTON_HEIGHT 100.0f
#define GAME_BUTTON_FONT_SIZE 20
#define GAME_BUTTON_BASE_COLOR RGB(54, 54, 54)
#define GAME_BUTTON_HOVR_COLOR RGB(80, 80, 80)
#define GAME_BUTTON_PICK_COLOR RGB(180, 80, 80)

void calculate_game_buttons_positions(Rectangle bounds, Games games, GameButton *buttons) {
  memset(buttons, 0, sizeof(GameButton) * games.count);

  for (size_t i = 0; i < games.count; ++i) {
    int text_width = MeasureText(games.items[i].name, GAME_BUTTON_FONT_SIZE);
    buttons[i].data = games.items[i];
    buttons[i].width = MAX(GAME_BUTTON_HEIGHT, text_width + GENERAL_PADDING*2);
    buttons[i].height = GAME_BUTTON_HEIGHT;
    buttons[i].title_width = text_width;
  }
  if (games.count == 0) return;

  size_t save = nob_temp_save();
  GameButton **row = nob_temp_alloc(sizeof(GameButton*)*games.count);
  size_t last_i = 0;

  float bounds_width = bounds.width - bounds.x - GENERAL_PADDING*2;
  float y = bounds.y + GENERAL_PADDING;
  while (last_i < games.count) {
    float row_width = 0;
    size_t row_sz = 0;
    memset(row, 0, sizeof(GameButton*)*games.count);

    for (size_t i = last_i; i < games.count; ++i) {
      if (row_sz == 0) {
        row[row_sz++] = &buttons[i];
        row_width = buttons[i].width;
        continue;
      }

      GameButton gb = buttons[i];
      float alusive_width = row_width + GENERAL_PADDING + gb.width;
      if (alusive_width >= bounds_width) break;
      row[row_sz++] = &buttons[i];
      row_width = alusive_width;
    }

    float x = bounds.x + GENERAL_PADDING + (bounds_width/2.0f - row_width/2.0f);
    for (size_t i = 0; i < row_sz; ++i) {
      row[i]->x = x;
      row[i]->y = y;
      x += row[i]->width + GENERAL_PADDING;
    }

    last_i += row_sz;
    y += GAME_BUTTON_HEIGHT + GENERAL_PADDING;
  }

  nob_temp_rewind(save);
}

Button_State game_button(GameButton *gb) {
  Rectangle bounds = { .x = gb->x, .y = gb->y, .width = gb->width, .height = GAME_BUTTON_HEIGHT };
  Button_State state = BUTTON_STATE_NONE;
  if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
    state = state | BUTTON_STATE_HOVER;
    DrawRectangleRounded(bounds, 0.05f, 4, GAME_BUTTON_HOVR_COLOR);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      state = state | BUTTON_STATE_CLICK;
    }
  } else {
    DrawRectangleRounded(bounds, 0.05f, 4, GAME_BUTTON_BASE_COLOR);
  }
  DrawRectangleRoundedLines(bounds, 0.05f, 4, RGB(200, 100, 150));
  int text_x = (int) (bounds.x + (bounds.width / 2.0 - gb->title_width / 2.0));
  int text_y = (int)(bounds.y + bounds.height / 2.0);
  DrawText(gb->data.name, text_x, text_y, GAME_BUTTON_FONT_SIZE, RGB(200, 180, 200));
  return state;
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
    } else {
      nob_log(NOB_INFO, "Loading default dir: %s\n", games_dir);
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
  GameButton *buttons = nob_temp_alloc(sizeof(GameButton)*games.count);


  SetConfigFlags(FLAG_WINDOW_RESIZABLE);

  SetTraceLogLevel(LOG_WARNING);
  InitWindow(WIDTH, HEIGHT, "Lzua");
  SetWindowMinSize(MIN_WIDTH, MIN_HEIGHT);

  nob_log(NOB_INFO, "Initialized window (%d, %d)", WIDTH, HEIGHT);

  MouseCursor cursor = MOUSE_CURSOR_DEFAULT;
  SetMouseCursor(MOUSE_CURSOR_DEFAULT);

  while (!WindowShouldClose()) {
    if (processes.count > 0) {
      if (!nob_procs_wait_and_reset(&processes)) {
        // I don't care if the game process stays a zombie, we don't crash ma boi
        nob_log(NOB_ERROR, "Failure happened while waiting for item");
        processes.count = 0;
      } else {
        nob_log(NOB_INFO, "Succesfully closed out of game");
      }
    }

    BeginDrawing();
    ClearBackground(RGB(12, 12, 12));

    Rectangle bounds = {
      .x = GENERAL_PADDING, .y = GENERAL_PADDING,
      .width = GetScreenWidth() - GENERAL_PADDING*2,
      .height = GetScreenHeight() - GENERAL_PADDING*2,
    };
    DrawRectangleRec(bounds, RGB(16, 16, 16));

    calculate_game_buttons_positions(bounds, games, buttons);

    bool hovering_any = false;
    bool consumed = false;
    for (GameButton *gb = buttons; gb < buttons + games.count; ++gb) {
      Button_State btn_state = game_button(gb);
      if (btn_state & BUTTON_STATE_HOVER) {
        hovering_any = true;
        if (cursor != MOUSE_CURSOR_POINTING_HAND) {
          cursor = MOUSE_CURSOR_POINTING_HAND;
          SetMouseCursor(cursor);
        }
      }
      if (btn_state & BUTTON_STATE_CLICK && !consumed) {
        consumed = true;
        size_t save = nob_temp_save();
        #ifdef _WIN32
        const char *game_cmd = nob_temp_sprintf("%s%c%s", gb->data.folder, PATH_DELIM, gb->data.exe);
        #else
        const char *game_cmd = nob_temp_sprintf(".%c%s", PATH_DELIM, gb->data.exe);
        #endif // _WIN32
        nob_cmd_append(&cmd, game_cmd);
        if (!nob_cmd_run(&cmd, .async = &processes, .cwd_path = gb->data.folder)) {
          nob_log(NOB_ERROR, "Failed to fork process to open game: %s", gb->data.name);
        } else {
          nob_log(NOB_INFO, "Launched: %s", gb->data.name);
        }
        nob_temp_rewind(save);
      }
    }

    if (!hovering_any && cursor != MOUSE_CURSOR_DEFAULT) {
      cursor = MOUSE_CURSOR_DEFAULT;
      SetMouseCursor(cursor);
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
