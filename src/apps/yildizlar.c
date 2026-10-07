/* aether-yildizlar — Aether ekran koruyucusu: içinden geçilen yıldız alanı (SDL2)
   Herhangi bir tuş, tık veya fare hareketiyle kapanır. */
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define N 900
typedef struct { float x, y, z; } Yildiz;
static Yildiz s[N];
static void yeni(Yildiz *y, int uzak) { y->x = (rand() / (float)RAND_MAX - 0.5f) * 2; y->y = (rand() / (float)RAND_MAX - 0.5f) * 2; y->z = uzak ? 1.0f : (rand() / (float)RAND_MAX) * 0.99f + 0.01f; }
int main(int argc, char **argv) {
    int pencereli = argc > 1 && !strcmp(argv[1], "--pencere");
    srand(time(NULL));
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window *w = SDL_CreateWindow("Aether", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 600,
        pencereli ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!w) return 1;
    int vsync = 1;
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!r) { r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE); vsync = 0; }
    SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < N; i++) yeni(&s[i], 0);
    Uint32 bas = SDL_GetTicks(), onceki = bas; int calis = 1;
    while (calis) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN) calis = 0;
            if (e.type == SDL_MOUSEMOTION && SDL_GetTicks() - bas > 1500 && (abs(e.motion.xrel) + abs(e.motion.yrel)) > 4) calis = 0;
        }
        Uint32 simdi = SDL_GetTicks(); float dt = (simdi - onceki) / 1000.0f; onceki = simdi; if (dt > 0.1f) dt = 0.1f;
        int W, H; SDL_GetRendererOutputSize(r, &W, &H);
        SDL_SetRenderDrawColor(r, 6, 8, 24, 255); SDL_RenderClear(r);
        for (int i = 0; i < N; i++) {
            Yildiz *y = &s[i]; float oz = y->z;
            y->z -= dt * 0.25f;
            if (y->z <= 0.01f) { yeni(y, 1); continue; }
            float k = 0.5f / y->z, ko = 0.5f / oz;
            int x1 = W / 2 + y->x * k * H, y1 = H / 2 + y->y * k * H;
            int x0 = W / 2 + y->x * ko * H, y0 = H / 2 + y->y * ko * H;
            if (x1 < 0 || y1 < 0 || x1 >= W || y1 >= H) { yeni(y, 1); continue; }
            int p = (int)(255 * (1.0f - y->z)); if (p < 40) p = 40;
            SDL_SetRenderDrawColor(r, p * 0.85, p * 0.8, p, 255);
            SDL_RenderDrawLine(r, x0, y0, x1, y1);
            if (y->z < 0.25f) SDL_RenderDrawLine(r, x0 + 1, y0, x1 + 1, y1);
        }
        SDL_RenderPresent(r);
        if (!vsync) SDL_Delay(16);
    }
    SDL_Quit();
    return 0;
}
