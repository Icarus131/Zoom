#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#define WIDTH 800
#define HEIGHT 600
#define NUM_THREADS 4
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>

typedef struct Window *Window;

Window windowCreate(int width, int height);
void windowDestroy(Window window);
bool windowInit(Window window);
void windowSwap(Window window);
struct Window {

  int width;
  int height;

  SDL_Window *window;
  SDL_GLContext context;
};

Window windowCreate(int width, int height) {

  Window window = malloc(sizeof(struct Window));

  window->height = height;
  window->width = width;

  return window;
}

void windowDestroy(Window window) {

  SDL_DestroyWindow(window->window);
  window->window = NULL;

  // Quit SDL subsystems
  SDL_Quit();
  // free(window);
}

bool windowInit(Window window) {

  bool success = true;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {

    printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
    success = false;

  } else {

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window->window = SDL_CreateWindow(
        "zoom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window->width,
        window->height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (window->window == NULL) {

      printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
      success = false;

    } else {

      window->context = SDL_GL_CreateContext(window->window);

      if (window->context == NULL) {
        printf("SDL: OpenGL context could not be created!\nSDL Error: %s\n",
               SDL_GetError());
        success = false;
      } else {

#ifndef __EMSCRIPTEN__

        glewExperimental = GL_TRUE;
        GLenum glewError = glewInit();
        if (glewError != GLEW_OK) {
          printf("GLEW: Error initializing! %s\n",
                 glewGetErrorString(glewError));
        }

#endif

        if (SDL_GL_SetSwapInterval(2) < 0) {
          printf("SDL: Warning: Unable to set VSync!\nSDL Error: %s\n",
                 SDL_GetError());
        }
      }

      SDL_SetRelativeMouseMode(SDL_TRUE);
    }
  }

  return success;
}
void windowSwap(Window window) { SDL_GL_SwapWindow(window->window); }

typedef struct Shader *Shader;

bool shaderInit(Shader *shader, const char *vertex_source,
                const char *fragment_source);
void shaderDestroy(Shader shader);
void shaderUse(Shader shader);

struct Shader {
  GLuint program;
};

bool shaderInit(Shader *shader, const char *vertex_source,
                const char *fragment_source) {

  *shader = malloc(sizeof(struct Shader));

  unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_source, NULL);
  glCompileShader(vertex_shader);

  int success;
  char info_log[512];
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
    printf("ERROR compiling shader: %s\n", info_log);
  }

  unsigned int fragment_shader;
  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_source, NULL);
  glCompileShader(fragment_shader);

  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
    printf("ERROR compiling shader: %s\n", info_log);
  }

  (*shader)->program = glCreateProgram();

  glAttachShader((*shader)->program, vertex_shader);
  glAttachShader((*shader)->program, fragment_shader);
  glLinkProgram((*shader)->program);

  glGetProgramiv((*shader)->program, GL_LINK_STATUS, &success);

  if (!success) {

    glGetProgramInfoLog((*shader)->program, 512, NULL, info_log);
    printf("ERROR compiling shader: %s\n", info_log);

  } else {
    printf("Shader compiled\n");
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
}

void shaderDestroy(Shader shader) {
  glDeleteProgram(shader->program);
  // free(shader);
}

void shaderUse(Shader shader) { glUseProgram(shader->program); }

typedef struct World *World;

struct Position {
  double x;
  double y;
};

struct Ray {
  double depth;
  unsigned int color;
  double angle_of_incidence;
};

typedef struct Position Position;
typedef struct Ray Ray;

World worldCreate(char *map, int width, int height, double scale);
Ray worldCastRay(World world, Position position, double ray_angle,
                 double player_angle);
Position worldGetPlayerPosition(World world);

struct World {
  char *map;
  int map_width;
  int map_height;
  double scale;
};

enum Side { SOUTH, NORTH, EAST, WEST };

World worldCreate(char *map, int width, int height, double scale) {

  World world = malloc(sizeof(struct World));

  world->map = map;
  world->map_width = width;
  world->map_height = height;
  world->scale = scale;

  return world;
}

unsigned int getRayColor(unsigned char symbol) {

  switch (symbol) {
  case 'r':
    return 0xFF2222;
  case 'g':
    return 0x22FF22;
  case 'b':
    return 0x2222FF;
  default:
    return 0x222222;
  }
}

double getAngleOfIncidence(enum Side side, double angle) {

  switch (side) {
  case WEST:
    return M_PI_2 - (angle - M_PI - M_PI_2);
  case EAST:
    return M_PI_2 - (angle - M_PI_2);
  case NORTH:
    return M_PI_2 - angle;
  case SOUTH:
    return M_PI_2 - (angle - M_PI);
  default:
    return 0;
  }
}

enum Side getSideHit(int x, int y, int prev_x, int prev_y) {

  if (x > prev_x) {
    return WEST;
  }
  if (x < prev_x) {
    return EAST;
  }
  if (y > prev_y) {
    return NORTH;
  }
  if (y < prev_y) {
    return SOUTH;
  }
}

double getDepth(Position player_position, int map_x, int map_y, double delta_x,
                double delta_y, double scale, enum Side side) {

  if (delta_x == 0) {

    int add_height = (delta_y < 0) ? 1 : 0;
    return fabs((map_y + add_height) * scale - player_position.y);
  }

  if (delta_y == 0) {

    int add_width = (delta_x < 0) ? 1 : 0;
    return fabs((map_x + add_width) * scale - player_position.x);
  }

  if (side == NORTH || side == SOUTH) {

    double slope = delta_y / delta_x;
    int add_height = (side == SOUTH) ? 1 : 0;

    double y = (map_y + add_height) * scale;
    double x = y / slope + player_position.x - player_position.y / slope;

    return sqrt((player_position.x - x) * (player_position.x - x) +
                (player_position.y - y) * (player_position.y - y));
  }

  if (side == EAST || side == WEST) {

    double slope = delta_x / delta_y;
    int add_width = (side == EAST) ? 1 : 0;

    double x = (map_x + add_width) * scale;
    double y = x / slope + player_position.y - player_position.x / slope;

    return sqrt((player_position.x - x) * (player_position.x - x) +
                (player_position.y - y) * (player_position.y - y));
  }
}

Ray worldCastRay(World world, Position position, double ray_angle,
                 double player_angle) {

  Ray ray;

  int map_x = (int)(position.x / world->scale);
  int map_y = (int)(position.y / world->scale);

  int prev_map_x;
  int prev_map_y;

  double march_x = 0;
  double march_y = 0;

  double detail = 10;

  double delta_x = -sin(ray_angle) / detail;
  double delta_y = cos(ray_angle) / detail;

  char curr;

  do {
    march_x += delta_x;
    march_y += delta_y;

    prev_map_x = map_x;
    prev_map_y = map_y;

    map_x = (int)((position.x + march_x) / world->scale);
    map_y = (int)((position.y + march_y) / world->scale);

    if (map_x < 0 || map_x >= world->map_width || map_y < 0 ||
        map_y >= world->map_height) {

      ray.depth = 1000000;
      ray.color = 0x000000;

      return ray;
    }

    curr = world->map[map_y * world->map_width + map_x];

  } while (curr == ' ' || curr == 'P');

  if ((map_x != prev_map_x) && (map_y != prev_map_y)) {
    return worldCastRay(world, position, ray_angle - 0.0001, player_angle);

  } else {

    enum Side side = getSideHit(map_x, map_y, prev_map_x, prev_map_y);

    double raw_depth =
        getDepth(position, map_x, map_y, delta_x, delta_y, world->scale, side) /
        world->scale;
    ray.depth = fabs(cos(player_angle - ray_angle) * raw_depth);

    ray.color = getRayColor(curr);
    ray.angle_of_incidence = getAngleOfIncidence(side, ray_angle);
  }

  return ray;
}

Position worldGetPlayerPosition(World world) {

  Position position;

  for (int y = 0; y < world->map_height; y++) {
    for (int x = 0; x < world->map_width; x++) {

      if (world->map[y * world->map_width + x] == 'P') {

        position.x = x * world->scale;
        position.y = y * world->scale;

        return position;
      }
    }
  }

  position.x = -1;
  position.y = -1;

  return position;
}

const char *vertex_shader_source =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout (location = 0) in vec4 vertex;\n" // <vec2 position, vec2 texCoords>
    "out vec2 TexCoords;\n"
    "void main() {\n"
    "TexCoords = vertex.zw;\n"
    "gl_Position = vec4(2.0f*vertex.x - 1.0f, 1.0f - 2.0f*vertex.y, 0.0, "
    "1.0);\n"
    "}\0";

const char *fragment_shader_source =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 TexCoords;\n"
    "out vec4 color;\n"
    "uniform sampler2D screenTexture;\n"
    "void main() {\n"
    "color = vec4(texture(screenTexture, vec2(TexCoords.x, TexCoords.y)).rgb, "
    "1.0f);\n"
    "}\0";

const float quadVertexData[] = {
    0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

    0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f};

pthread_t threads[NUM_THREADS];
int thread_args[NUM_THREADS];

World world;
char map[] = "rrrrrrrrrrrrrrrrrrrrrrrrrrrrrr"
             "r                 r          r"
             "r                 r          r"
             "r   P             r          r"
             "r                 r          r"
             "r                 r          r"
             "r                 r          r"
             "r     rrrrrrrrrrrrr     rrrrrr"
             "r                 r          r"
             "r  r              r          r"
             "r                 r          r"
             "r                            r"
             "r                            r"
             "r    rr           rrrrr    rrr"
             "r                 r          r"
             "r                 r          r"
             "r                 r          r"
             "r                 r          r"
             "rrrrrrrrrrrrrrrrrrrrrrrrrrrrr";

const int world_width = 30;
const int world_height = 19;
const double world_scale = 10;

Position player_position;
double player_angle = 0;
const double player_speed = 70;
const double mouse_sensitivity = 20;

const double fov = 100;
double half_fov;
double focus_to_image;

const double max_fog_distance = 20;
const double min_fog_distance = 2;
const unsigned int fog_color = 0x87CEEB;
const unsigned int light_color = 0xFFFFFF;

Window window = NULL;
Shader shader = NULL;
char texture_data[WIDTH * HEIGHT * 3];
GLuint vao = -1;
GLuint screen_texture = -1;

bool quit = false;
SDL_Event event;

bool keydown_w = false;
bool keydown_a = false;
bool keydown_s = false;
bool keydown_d = false;
bool keydown_left = false;
bool keydown_right = false;

int mouse_move_x = 0;

void createScreenTexture() {

  glGenTextures(1, &screen_texture);
  glBindTexture(GL_TEXTURE_2D, screen_texture);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB,
               GL_UNSIGNED_BYTE, 0);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void createQuadVAO() {

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertexData), quadVertexData,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

unsigned int lerpColor(unsigned int color_1, unsigned int color_2,
                       double lin_val) {

  unsigned int newColor = 0x000000;
  unsigned int mask = 0xFF;

  newColor |=
      (unsigned char)(((float)((color_1 >> 16) & mask) * (1 - lin_val)) +
                      ((float)((color_2 >> 16) & mask) * lin_val));
  newColor <<= 8;
  newColor |= (unsigned char)(((float)((color_1 >> 8) & mask) * (1 - lin_val)) +
                              ((float)((color_2 >> 8) & mask) * lin_val));
  newColor <<= 8;
  newColor |= (unsigned char)(((float)((color_1 >> 0) & mask) * (1 - lin_val)) +
                              ((float)((color_2 >> 0) & mask) * lin_val));

  return newColor;
}

double getFogAmount(double depth) {

  return (depth > min_fog_distance)
             ? fmin((depth - min_fog_distance) /
                        (max_fog_distance - min_fog_distance),
                    0.8)
             : 0;
}

void *renderScene(void *thread_num) {

  float thread_div = (float)WIDTH / NUM_THREADS;

  int thread_start = thread_div * *((int *)thread_num);
  int thread_end = thread_div * (*((int *)thread_num) + 1);

  for (int x = thread_start; x < thread_end; x++) {

    Ray ray = worldCastRay(
        world, player_position,
        player_angle + atan((x - (WIDTH / 2)) / focus_to_image), player_angle);

    ray.color =
        lerpColor(light_color, ray.color, sqrt(sin(ray.angle_of_incidence)));

    int wall_height = (int)((HEIGHT / (ray.depth)));

    double fog_amount = getFogAmount(ray.depth);

    if (fog_amount > 0) {

      ray.color = lerpColor(ray.color, fog_color, fog_amount);
    }

    for (int y = 0; y < HEIGHT; y++) {

      if (y > (HEIGHT - wall_height) / 2 &&
          y < wall_height + (HEIGHT - wall_height) / 2) {

        texture_data[(y * WIDTH + x) * 3 + 0] = ray.color >> 16;
        texture_data[(y * WIDTH + x) * 3 + 1] = ray.color >> 8;
        texture_data[(y * WIDTH + x) * 3 + 2] = ray.color >> 0;

      } else {

        if (y < HEIGHT / 2) {

          texture_data[(y * WIDTH + x) * 3 + 0] = 0x87;
          texture_data[(y * WIDTH + x) * 3 + 1] = 0xCE;
          texture_data[(y * WIDTH + x) * 3 + 2] = 0xEB;

        } else {

          double floor_depth = HEIGHT / ((y - HEIGHT / 2.0f) * 2.0f);
          int floor_color = 0x635244;

          double floor_fog = getFogAmount(floor_depth);

          if (floor_fog > 0) {

            floor_color = lerpColor(floor_color, fog_color, floor_fog);
          }

          texture_data[(y * WIDTH + x) * 3 + 0] = floor_color >> 16;
          texture_data[(y * WIDTH + x) * 3 + 1] = floor_color >> 8;
          texture_data[(y * WIDTH + x) * 3 + 2] = floor_color >> 0;
        }
      }
    }
  }

  pthread_exit(0);
}

void render() {

  for (int it = 0; it < NUM_THREADS; it++) {
    pthread_create(&(threads[it]), NULL, renderScene,
                   (void *)(&(thread_args[it])));
  }

  for (int it = 0; it < NUM_THREADS; it++) {
    pthread_join(threads[it], NULL);
  }

  glBindTexture(GL_TEXTURE_2D, screen_texture);

  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGB,
                  GL_UNSIGNED_BYTE, texture_data);

  glClear(GL_COLOR_BUFFER_BIT);
  shaderUse(shader);

  glBindVertexArray(vao);
  glBindTexture(GL_TEXTURE_2D, screen_texture);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glBindVertexArray(0);
  glUseProgram(0);
  windowSwap(window);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void updatePlayer(uint64_t delta) {

  player_angle += mouse_move_x / 1000.0 * mouse_sensitivity;

  if (keydown_left != keydown_right) {

    float change = (keydown_left) ? -1 : 1;
    float arrow_speed = 3.5f;
    player_angle += (delta / 1000.0) * change * arrow_speed;
  }

  float x_fraction;
  float y_fraction;

  float mult = (delta / 1000.0) * player_speed;

  if ((keydown_w != keydown_s)) {

    mult = keydown_s ? mult * -1 : mult;

    x_fraction = -sin(player_angle);
    y_fraction = cos(player_angle);

    player_position.x += x_fraction * mult;
    player_position.y += y_fraction * mult;
  }

  if ((keydown_a != keydown_d)) {

    mult = keydown_s ? mult * -1 : mult;

    float turn_angle = keydown_d ? M_PI_2 : -M_PI_2;

    x_fraction = -sin(player_angle + turn_angle);
    y_fraction = cos(player_angle + turn_angle);

    player_position.x += x_fraction * mult;
    player_position.y += y_fraction * mult;
  }
}

void pollEvents() {

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      printf("Quit event received\n");
      quit = true;
      break;

    case SDL_WINDOWEVENT:

      if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        glViewport(0, 0, event.window.data1, event.window.data2);
      }
      break;
    case SDL_MOUSEMOTION:

      mouse_move_x = event.motion.xrel;
      break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {

      case SDLK_ESCAPE:
        quit = true;
        break;

      case SDLK_w:
        keydown_w = true;
        break;

      case SDLK_a:
        keydown_a = true;
        break;

      case SDLK_s:
        keydown_s = true;
        break;

      case SDLK_d:
        keydown_d = true;
        break;

      case SDLK_LEFT:
        keydown_left = true;
        break;

      case SDLK_RIGHT:
        keydown_right = true;
        break;
      }
      break;

    case SDL_KEYUP:
      switch (event.key.keysym.sym) {

      case SDLK_w:
        keydown_w = false;
        break;

      case SDLK_a:
        keydown_a = false;
        break;

      case SDLK_s:
        keydown_s = false;
        break;

      case SDLK_d:
        keydown_d = false;
        break;

      case SDLK_LEFT:
        keydown_left = false;
        break;

      case SDLK_RIGHT:
        keydown_right = false;
        break;
      }
      break;

    default:
      break;
    }
  }
}

int main(int argv, char **args) {

  window = windowCreate(WIDTH, HEIGHT);
  int t = 0;

  world = worldCreate(map, world_width, world_height, world_scale);
  player_position = worldGetPlayerPosition(world);

  for (int it = 0; it < NUM_THREADS; it++) {
    thread_args[it] = it;
  }

  if (!windowInit(window)) {

    printf("Failed to initialize SDL\n");

  } else {

    printf("SDL initialized\n");

    shaderInit(&shader, vertex_shader_source, fragment_shader_source);
    createScreenTexture();

    glClearColor(0.5, 0.2, 0.5, 1.0);
    createQuadVAO();

    half_fov = (fov / 180.0f * M_PI) / 2.0f;
    focus_to_image = (WIDTH / 2) / tan(half_fov);

    uint64_t last_frame = SDL_GetTicks64();
    uint64_t current_frame = SDL_GetTicks64();
    uint64_t delta_time = 0;

    while (!quit) {

      current_frame = SDL_GetTicks64();
      delta_time = current_frame - last_frame;
      last_frame = current_frame;

      pollEvents();
      updatePlayer(delta_time);

      render();

      mouse_move_x = 0;
    }
  }

  glDeleteBuffers(1, &vao);
  glDeleteTextures(1, &screen_texture);

  windowDestroy(window);
  shaderDestroy(shader);

  return 0;
}
