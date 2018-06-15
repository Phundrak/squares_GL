#include <GL4D/gl4df.h>
#include <GL4D/gl4dp.h>
#include <GL4D/gl4du.h>
#include <GL4D/gl4duw_SDL2.h>
#include <GLFW/glfw3.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <assert.h>
#include <fftw3.h>
#include <math.h>
#include <stdio.h>

/*****************************************************************************/
/*                                 constants                                 */
/*****************************************************************************/

#define ECHANTILLONS 1024
#define LIMIT_BASS 10
#define LIMIT_HIGH 500
#define END_CREDITS 14700.0
#define END_MUSIC 302000.0

/*****************************************************************************/
/*                                 functions                                 */
/*****************************************************************************/

/* assimp functions **********************************************************/
extern void assimpInit(const char *filename);
extern void assimpDrawScene(void);
extern void assimpQuit(void);

/* audio functions ***********************************************************/
static void initAudio(const char *filename);
static void mixCallback(void *udata, Uint8 *stream, int len);

/* general functions *********************************************************/
static void init(void);
static void resize(int w, int h);
static void loadTexture(GLuint id, const char *filename);
static void initText(GLuint *ptId, const char *text);
static void draw(void);
static void quit(void);

/*****************************************************************************/
/*                                 variables                                 */
/*****************************************************************************/

/* textures ******************************************************************/
static GLuint _tId = 0; /* texture pour les carrés */

/* OpenGL and GL4D ***********************************************************/
static int _wW = 800, _wH = 800;
static GLuint _pId = 0, _pId2 = 0, _pId3 = 0; /* id programme GLSL */
static GLuint _cube1 = 0, _cube2 = 0, _cube3 = 0, _cube = 0, _quad = 0,
              _textTexId = 0;

/* audio *********************************************************************/
static Sint16 _hauteurs[ECHANTILLONS]; /* résultat de l'analyse FFT */
/* pointeur vers la musique chargée par SDL_Mixer */
static Mix_Music *_mmusic = NULL;
/* données entrées/sorties pour la lib fftw */
static fftw_complex *_in4fftw = NULL, *_out4fftw = NULL;
/* donnée à précalculée utile à la lib fftw */
static fftw_plan _plan4fftw = NULL;

/*****************************************************************************/
/*                                                                           */
/*                                                                           */
/*                               implementation                              */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/

int main(int argc, char **argv) {
  if (!gl4duwCreateWindow(argc, argv, "GL4Dummies", 0, 0, _wW, _wH,
                          GL4DW_RESIZABLE | GL4DW_SHOWN))
    return 1;

  assimpInit("models/ALYS_ShapeChange.obj");
  init();
  atexit(quit);
  gl4duwResizeFunc(resize);
  gl4duwDisplayFunc(draw);
  gl4duwMainLoop();
  return 0;
}

/* init de OpenGL */
static void init(void) {
  /* shaders *****************************************************************/
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.0824f, 0.0824f, 0.0824f, 0.0f);
  _pId =
      gl4duCreateProgram("<vs>shaders/model.vs", "<fs>shaders/model.fs", NULL);
  _pId2 =
      gl4duCreateProgram("<vs>shaders/model.vs", "<fs>shaders/model.fs", NULL);
  _pId3 = gl4duCreateProgram("<vs>shaders/credits.vs", "<fs>shaders/credits.fs",
                             NULL);
  gl4duGenMatrix(GL_FLOAT, "modelViewMatrix");
  gl4duGenMatrix(GL_FLOAT, "projectionMatrix");
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  resize(_wW, _wH);

  /* textures ****************************************************************/
  glGenTextures(1, &_tId);
  loadTexture(_tId, "images/square.jpg");

  /* objets 3D ***************************************************************/
  _cube2 = gl4dgGenCubef();
  _cube = gl4dgGenCubef();
  _cube3 = gl4dgGenCubef();
  _cube1 = gl4dgGenCubef();

  /* audio *******************************************************************/
  _in4fftw = fftw_malloc(ECHANTILLONS * sizeof *_in4fftw);
  memset(_in4fftw, 0, ECHANTILLONS * sizeof *_in4fftw);
  assert(_in4fftw);
  _out4fftw = fftw_malloc(ECHANTILLONS * sizeof *_out4fftw);
  assert(_out4fftw);
  _plan4fftw = fftw_plan_dft_1d(ECHANTILLONS, _in4fftw, _out4fftw, FFTW_FORWARD,
                                FFTW_ESTIMATE);
  assert(_plan4fftw);
  initAudio("audio/musique.mp3");

  /* text ********************************************************************/
  _quad = gl4dgGenQuadf();
  initText(&_textTexId, "        Modèle 3D :\n"
                        "Personnage d'ALYS par VoxWave\n"
                        "Modèle 3D d'ALYS par YoiStyle\n"
                        "\n      Musique :\n"
                        "\"Squares\" par apol-P\n"
                        "\n      Animation OpenGL :\n"
                        "Lucien Cartier");
}

static void loadTexture(GLuint id, const char *filename) {
  SDL_Surface *t;
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  if ((t = IMG_Load(filename)) != NULL) {
#ifdef __APPLE__
    int mode = t->format->BytesPerPixel == 4 ? GL_BGRA : GL_BGR;
#else
    int mode = t->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB;
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, mode,
                 GL_UNSIGNED_BYTE, t->pixels);
    SDL_FreeSurface(t);
  } else {
    fprintf(stderr, "can't open file %s : %s\n", filename, SDL_GetError());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 NULL);
  }
}

static void initText(GLuint *ptId, const char *text) {
  static int firstTime = 1;
  SDL_Color c = {245, 245, 245, 255};
  SDL_Surface *d, *s;
  TTF_Font *font = NULL;
  if (firstTime) {
    /* initialisation de la bibliothèque SDL2 ttf */
    if (TTF_Init() == -1) {
      fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
      exit(2);
    }
    firstTime = 0;
  }
  if (*ptId == 0) {
    /* initialisation de la texture côté OpenGL */
    glGenTextures(1, ptId);
    glBindTexture(GL_TEXTURE_2D, *ptId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  /* chargement de la font */
  if (!(font = TTF_OpenFont("DejaVuSans-Bold.ttf", 128))) {
    fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
    return;
  }
  /* création d'une surface SDL avec le texte */
  d = TTF_RenderUTF8_Blended_Wrapped(font, text, c, 2048);
  if (d == NULL) {
    TTF_CloseFont(font);
    fprintf(stderr, "Erreur lors du TTF_RenderText\n");
    return;
  }
  /* copie de la surface SDL vers une seconde aux spécifications qui
   * correspondent au format OpenGL */
  s = SDL_CreateRGBSurface(0, d->w, d->h, 32, R_MASK, G_MASK, B_MASK, A_MASK);
  assert(s);
  SDL_BlitSurface(d, NULL, s, NULL);
  SDL_FreeSurface(d);
  /* transfert vers la texture OpenGL */
  glBindTexture(GL_TEXTURE_2D, *ptId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, s->pixels);
  fprintf(stderr, "Dimensions de la texture : %d %d\n", s->w, s->h);
  SDL_FreeSurface(s);
  TTF_CloseFont(font);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void initAudio(const char *filename) {
#if defined(__APPLE__)
  int mult = 1;
#else
  int mult = 2;
#endif
  int mixFlags = MIX_INIT_MP3, res;
  res = Mix_Init(mixFlags);
  if ((res & mixFlags) != mixFlags) {
    fprintf(stderr, "Mix_Init: Erreur lors de l'initialisation de la "
                    "bibliothèque SDL_Mixer\n");
    fprintf(stderr, "Mix_Init: %s\n", Mix_GetError());
  }
  if (Mix_OpenAudio(44100, AUDIO_S16LSB, 1, mult * ECHANTILLONS) < 0)
    exit(4);
  if (!(_mmusic = Mix_LoadMUS(filename))) {
    fprintf(stderr, "Erreur lors du Mix_LoadMUS: %s\n", Mix_GetError());
    exit(5);
  }
  Mix_SetPostMix(mixCallback, NULL);
  if (!Mix_PlayingMusic())
    Mix_PlayMusic(_mmusic, 1);
}

static void mixCallback(void *udata, Uint8 *stream, int len) {
  if (_plan4fftw) {
    int i, j, l = MIN(len >> 1, ECHANTILLONS);
    Sint16 *d = (Sint16 *)stream;
    for (i = 0; i < l; i++)
      _in4fftw[i][0] = d[i] / ((1 << 15) - 1.0);
    fftw_execute(_plan4fftw);
    for (i = 0; i<l>> 2; i++) {
      _hauteurs[4 * i] = (int)(sqrt(_out4fftw[i][0] * _out4fftw[i][0] +
                                    _out4fftw[i][1] * _out4fftw[i][1]) *
                               exp(2.0 * i / (double)(l / 4.0)));
      for (j = 1; j < 4; j++)
        _hauteurs[4 * i + j] = MIN(_hauteurs[4 * i], 255);
    }
  }
}

static void resize(int w, int h) {
  _wW = w;
  _wH = h;
  glViewport(0, 0, _wW, _wH);
  gl4duBindMatrix("projectionMatrix");
  gl4duLoadIdentityf();
  gl4duFrustumf(-0.5, 0.5, -0.5 * _wH / _wW, 0.5 * _wH / _wW, 1.0, 1000.0);
  gl4duBindMatrix("modelViewMatrix");
}

static void draw(void) {

  static GLfloat xz = 0, y = 0, shiftx = 0, shifty = 0, shiftz = 0,
                 rot_camera = 0, mod_shift = 0;
  static GLfloat basses = 0, high = 0, volume = 0;
  const float shift_coef = 0.02f;
  GLfloat lum[4] = {0.0, 0.0, 5.0, 1.0};
  static GLfloat t0 = -1;
  GLfloat t, d, time;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  time = SDL_GetTicks();

  /***************************************************************************/
  /*                              analyse audio                              */
  /***************************************************************************/

  for (int i = 0; i < ECHANTILLONS; ++i) {
    volume += (float)_hauteurs[i];
  }
  volume /= (float)ECHANTILLONS;
  printf("time %f\tvolume %f\n", time, volume);

  if(time > END_CREDITS && volume == 0.0)
    exit(0);

  for (int i = 0; i < LIMIT_BASS; ++i) {
    basses += (float)_hauteurs[i];
  }
  basses /= (float)LIMIT_BASS;
  xz += basses * 0.05;
  y += basses * 0.1;

  for (int i = LIMIT_HIGH; i < ECHANTILLONS; ++i) {
    high += (float)_hauteurs[i];
  }
  high /= (float)(ECHANTILLONS - LIMIT_HIGH);

  /***************************************************************************/
  /*                                    3D                                   */
  /***************************************************************************/

  shiftx = ((int)mod_shift % 6 == 0) ? high * shift_coef : shiftx;
  shifty = ((int)mod_shift % 6 == 1) ? high * shift_coef : shifty;
  shiftz = ((int)mod_shift % 6 == 2) ? high * shift_coef : shiftz;
  shiftx = ((int)mod_shift % 6 == 3) ? -high * shift_coef : shiftx;
  shifty = ((int)mod_shift % 6 == 4) ? -high * shift_coef : shifty;
  shiftz = ((int)mod_shift % 6 == 5) ? -high * shift_coef : shiftz;

  /* squares *****************************************************************/
  glUseProgram(_pId);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _tId);
  glDisable(GL_BLEND);
  glUniform1i(glGetUniformLocation(_pId, "tex"), 0);

  gl4duBindMatrix("modelViewMatrix");
  gl4duLoadIdentityf();

  gl4duTranslatef(0, -5, -20);
  gl4duRotatef(sin(rot_camera * 0.01) * 40, 0, -1, -0.25);
  gl4duRotatef(20, 1, 0, 0);

  gl4duPushMatrix();
  {
    gl4duTranslatef(shiftx - 1, shifty - 1, shiftz + 1);
    gl4duRotatef(-xz, 1, 0, 1);
    gl4duRotatef(y, 0, 1, 0);
    gl4duSendMatrices();
  }
  gl4duPopMatrix();
  gl4dgDraw(_cube1);

  gl4duPushMatrix();
  {
    gl4duTranslatef(shiftx + 1, shifty + 1.5f, shiftz + 1);
    gl4duRotatef(-xz, 1, 0, 1);
    gl4duRotatef(y, 0, 1, 0);
    gl4duSendMatrices();
  }
  gl4duPopMatrix();
  gl4dgDraw(_cube2);

  gl4duPushMatrix();
  {
    gl4duTranslatef(shiftx + 1, shifty - 1, shiftz);
    gl4duScalef(0.8, 0.8, 0.8);
    gl4duRotatef(-xz, 1, 0, 1);
    gl4duRotatef(y, 0, 1, 0);
    gl4duSendMatrices();
  }
  gl4duPopMatrix();
  gl4dgDraw(_cube3);

  gl4duPushMatrix();
  {
    gl4duTranslatef(shiftx, shifty, shiftz + 3);
    gl4duScalef(0.5f, 0.5f, 0.5f);
    gl4duRotatef(-xz, 1, 0, 1);
    gl4duRotatef(y, 0, 1, 0);
    gl4duSendMatrices();
  }
  gl4duPopMatrix();
  gl4dgDraw(_cube);

  gl4dfBlur(0, 0, (int)basses / 20, 1, 0, GL_FALSE);
  gl4duTranslatef(-0.7f, -20, -8);
  gl4duScalef(70, 70, 70);

  gl4duSendMatrices();

  /* credits *****************************************************************/
  if (t0 < 0.0f)
    t0 = SDL_GetTicks();
  if(time <= END_CREDITS) {
    glUseProgram(_pId3);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _textTexId);
    glUniform1i(glGetUniformLocation(_pId3, "inv"), 1);
    glUniform1i(glGetUniformLocation(_pId3, "tex"), 0);
    glUniform1f(glGetUniformLocation(_pId3, "alpha"),
                1.0f - fabsf(cos((time / 14800.0) * M_PI)));
    gl4duBindMatrix("modelViewMatrix");
    gl4duLoadIdentityf();
    gl4duPushMatrix();
    {
      gl4duTranslatef(-0.4, 0.4, -3);
      gl4duScalef(1.0f, 1.0f, 1.0f);
      gl4duSendMatrices();
    }
    gl4duPopMatrix();
    gl4dgDraw(_quad);
    glUseProgram(0);
  }

  /* ALYS ********************************************************************/

  glUseProgram(_pId2);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUniform4fv(glGetUniformLocation(_pId2, "lumpos"), 1, lum);
  glEnable(GL_CULL_FACE);
  gl4duPushMatrix();
  {

    gl4duSendMatrices();
  }
  gl4duPopMatrix();
  gl4duRotatef(180, 0, 1, 0);
  if(time > END_CREDITS)
    assimpDrawScene();
  gl4duSendMatrices();

  xz += 2;
  rot_camera += 0.3;
  mod_shift += 0.07;
}

static void quit(void) {
  if (_mmusic) {
    if (Mix_PlayingMusic())
      Mix_HaltMusic();
    Mix_FreeMusic(_mmusic);
    _mmusic = NULL;
  }
  Mix_CloseAudio();
  Mix_Quit();
  if (_plan4fftw) {
    fftw_destroy_plan(_plan4fftw);
    _plan4fftw = NULL;
  }
  if (_in4fftw) {
    fftw_free(_in4fftw);
    _in4fftw = NULL;
  }
  if (_out4fftw) {
    fftw_free(_out4fftw);
    _out4fftw = NULL;
  }
  if (_textTexId) {
    glDeleteTextures(1, &_textTexId);
    _textTexId = 0;
  }
  assimpQuit();
  gl4duClean(GL4DU_ALL);
}
