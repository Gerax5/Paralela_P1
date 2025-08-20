// main.c — Puntos 3D conectados con líneas (patrones en 3D con SDL2)
// Linux/WSL: gcc -O2 -std=c11 main.c $(sdl2-config --cflags --libs) -lm -o patrones3d
// Windows (MSYS2/MinGW64):
//   gcc -O2 -std=c11 main.c -I/mingw64/include/SDL2 -D_REENTRANT \
//       -L/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lm -o patrones3d
// Ejecuta: ./patrones3d [N] [mult]
//   N = # de puntos (opcional, default 300)
//   mult = multiplicador para modo 1 (opcional, default 2)
//
// Controles:
//   1/2/3  -> cambiar modo (Multiplicativa / Estrella / Tejido)
//   ←/→     -> mult-- / mult++ (modo 1)
//   A/Z     -> skip1++ / skip1-- (modo 2/3)
//   S/X     -> skip2++ / skip2-- (modo 3)
//   +/-     -> N += 10 / N -= 10 (20..3000)
//   P       -> toggle puntos
//   L       -> toggle líneas
//   T       -> toggle trails (persistencia)
//   R       -> reset parámetros del modo actual
//   ESC o cerrar ventana -> salir

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CLAMP01(x) ((x)<0?0:((x)>1?1:(x)))

typedef struct { float x,y,z; } Vec3;

// ------------------ Utiles de color (HSV -> RGB) ------------------
static SDL_Color hsv_to_rgb(float h, float s, float v, Uint8 a){
    while (h < 0) h += 360.0f; while (h >= 360.0f) h -= 360.0f;
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h/60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r=0,g=0,b=0;
    if      (h < 60)  { r=c; g=x; }
    else if (h < 120) { r=x; g=c; }
    else if (h < 180) { g=c; b=x; }
    else if (h < 240) { g=x; b=c; }
    else if (h < 300) { r=x; b=c; }
    else              { r=c; b=x; }
    SDL_Color col;
    col.r = (Uint8)((r+m)*255);
    col.g = (Uint8)((g+m)*255);
    col.b = (Uint8)((b+m)*255);
    col.a = a;
    return col;
}

static void set_col(SDL_Renderer* r, SDL_Color c){ SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); }

static SDL_Color shade_color(SDL_Color c, float k){
    SDL_Color o = c;
    o.r = (Uint8)(CLAMP01(k) * c.r);
    o.g = (Uint8)(CLAMP01(k) * c.g);
    o.b = (Uint8)(CLAMP01(k) * c.b);
    return o;
}

static void draw_filled_circle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color col){
    set_col(r,col);
    for (int dy = -radius; dy <= radius; ++dy){
        int y = cy + dy;
        int dx = (int)sqrtf((float)(radius*radius - dy*dy));
        SDL_RenderDrawLine(r, cx - dx, y, cx + dx, y);
    }
}

static void draw_line_thick(SDL_Renderer* r, int x1,int y1,int x2,int y2, int thick, SDL_Color c){
    set_col(r,c);
    float dx = (float)(x2-x1), dy = (float)(y2-y1);
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1e-3f){ SDL_RenderDrawPoint(r,x1,y1); return; }
    float nx = -dy/len, ny = dx/len; 
    int half = thick/2;
    for (int i=-half; i<=half; ++i){
        int ox = (int)roundf(nx*i), oy = (int)roundf(ny*i);
        SDL_RenderDrawLine(r, x1+ox,y1+oy, x2+ox,y2+oy);
    }
}

static Vec3 rot_x(Vec3 v, float a){
    float s = sinf(a), c = cosf(a);
    Vec3 r = { v.x, c*v.y - s*v.z, s*v.y + c*v.z };
    return r;
}
static Vec3 rot_y(Vec3 v, float a){
    float s = sinf(a), c = cosf(a);
    Vec3 r = {  c*v.x + s*v.z, v.y, -s*v.x + c*v.z };
    return r;
}
static Vec3 rot_z(Vec3 v, float a){
    float s = sinf(a), c = cosf(a);
    Vec3 r = { c*v.x - s*v.y, s*v.x + c*v.y, v.z };
    return r;
}

static bool project_perspective(Vec3 v, int W, int H, float fov, float camz,
                                int* sx, int* sy, float* depthZ){
    float z = v.z + camz;            
    if (z <= 1e-3f) return false;     
    float s = fov / z;                
    *sx = W/2 + (int)roundf(v.x * s);
    *sy = H/2 - (int)roundf(v.y * s);
    if (depthZ) *depthZ = z;
    return true;
}

static void make_ring_points(Vec3* pts, int N, float R, float A, float freq, float t){
    for (int i=0;i<N;i++){
        float th = (2.0f*(float)M_PI) * (float)i / (float)N;
        float z  = A * sinf(freq*th + t*1.2f);
        pts[i].x = R * cosf(th);
        pts[i].y = R * sinf(th);
        pts[i].z = z;
    }
}

typedef enum { MODO_MULT = 0, MODO_ESTRELLA = 1, MODO_TEJIDO = 2 } Modo;

static void render_scene(SDL_Renderer* ren, int W, int H, double t,
                         int N, int mult, int skip1, int skip2,
                         Modo modo, bool drawPts, bool drawLines, bool trails)
{
    if (!trails){
        SDL_Color bg = {12, 15, 22, 255};
        set_col(ren, bg); SDL_RenderClear(ren);
    }else{
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        set_col(ren, (SDL_Color){12,15,22,20});
        SDL_Rect r = {0,0,W,H}; SDL_RenderFillRect(ren,&r);
    }

    float R    = fminf((float)W,(float)H) * 0.35f;
    float A    = R * 0.20f;     
    float freq = 3.0f;          
    float camz = R * 3.2f;      
    float fov  = camz * 1.2f;

    float ax = 0.35f + 0.25f*sinf((float)t*0.7f);
    float ay = (float)t*0.55f;
    float az = 0.0f;

    Vec3* base = (Vec3*)malloc(sizeof(Vec3)*N);
    Vec3* rot  = (Vec3*)malloc(sizeof(Vec3)*N);
    if (!base || !rot){ free(base); free(rot); return; }

    make_ring_points(base, N, R, A, freq, (float)t);

    for (int i=0;i<N;i++){
        Vec3 v = base[i];
        v = rot_x(v, ax);
        v = rot_y(v, ay);
        v = rot_z(v, az);
        rot[i] = v;
    }

    SDL_Color colLine = hsv_to_rgb(200.0f, 0.20f, 1.0f, 255); 
    SDL_Color colPts  = hsv_to_rgb(220.0f, 0.35f, 1.0f, 255);

    if (drawLines){
        float hueShift = fmodf(210.0f + 40.0f*sinf((float)t*0.5f), 360.0f);
        colLine = hsv_to_rgb(hueShift, 0.25f, 1.0f, 255);

        switch (modo){
            case MODO_MULT: {
                for (int i=0;i<N;i++){
                    int j = (int)((long long)i * (long long)mult % (long long)N);
                    int x1,y1,x2,y2; float z1,z2;
                    if (project_perspective(rot[i], W,H, fov, camz, &x1,&y1,&z1) &&
                        project_perspective(rot[j], W,H, fov, camz, &x2,&y2,&z2))
                    {
                        float zavg = 0.5f*(z1+z2);
                        float shade = CLAMP01(1.25f - zavg/(camz*1.6f));
                        int thick = (int)fmaxf(1.0f, 3.0f * (camz/zavg)); 
                        draw_line_thick(ren, x1,y1, x2,y2, thick, shade_color(colLine, shade));
                    }
                }
            } break;
            case MODO_ESTRELLA: {
                int s = ((skip1%N)+N)%N; if (s==0) s=1;
                for (int i=0;i<N;i++){
                    int j = (i + s) % N;
                    int x1,y1,x2,y2; float z1,z2;
                    if (project_perspective(rot[i], W,H, fov, camz, &x1,&y1,&z1) &&
                        project_perspective(rot[j], W,H, fov, camz, &x2,&y2,&z2))
                    {
                        float zavg = 0.5f*(z1+z2);
                        float shade = CLAMP01(1.25f - zavg/(camz*1.6f));
                        int thick = (int)fmaxf(1.0f, 3.0f * (camz/zavg));
                        draw_line_thick(ren, x1,y1, x2,y2, thick, shade_color(colLine, shade));
                    }
                }
            } break;
            case MODO_TEJIDO: {
                int s1 = ((skip1%N)+N)%N; if (s1==0) s1=1;
                int s2 = ((skip2%N)+N)%N; if (s2==0) s2=2;
                for (int i=0;i<N;i++){
                    int next = (i+1)%N;
                    int j1   = (i+s1)%N;
                    int j2   = (i+s2)%N;
                    int x1,y1,x2,y2; float z1,z2;

                    if (project_perspective(rot[i], W,H, fov, camz, &x1,&y1,&z1) &&
                        project_perspective(rot[next], W,H, fov, camz, &x2,&y2,&z2))
                    {
                        float zavg = 0.5f*(z1+z2);
                        float shade = CLAMP01(1.2f - zavg/(camz*1.7f));
                        int thick = (int)fmaxf(1.0f, 2.0f * (camz/zavg));
                        draw_line_thick(ren, x1,y1, x2,y2, thick, shade_color(colLine, shade));
                    }
                    if (project_perspective(rot[i], W,H, fov, camz, &x1,&y1,&z1) &&
                        project_perspective(rot[j1], W,H, fov, camz, &x2,&y2,&z2))
                    {
                        float zavg = 0.5f*(z1+z2);
                        float shade = CLAMP01(1.25f - zavg/(camz*1.6f));
                        int thick = (int)fmaxf(1.0f, 3.0f * (camz/zavg));
                        draw_line_thick(ren, x1,y1, x2,y2, thick, shade_color(colLine, shade));
                    }
                    if (project_perspective(rot[i], W,H, fov, camz, &x1,&y1,&z1) &&
                        project_perspective(rot[j2], W,H, fov, camz, &x2,&y2,&z2))
                    {
                        float zavg = 0.5f*(z1+z2);
                        float shade = CLAMP01(1.25f - zavg/(camz*1.6f));
                        int thick = (int)fmaxf(1.0f, 3.0f * (camz/zavg));
                        draw_line_thick(ren, x1,y1, x2,y2, thick, shade_color(colLine, shade));
                    }
                }
            } break;
        }
    }

    if (drawPts){
        for (int i=0;i<N;i++){
            int x,y; float z;
            if (project_perspective(rot[i], W,H, fov, camz, &x,&y,&z)){
                float shade = CLAMP01(1.15f - z/(camz*1.7f));
                int radius = (int)fmaxf(1.0f, 3.0f*(camz/z));
                draw_filled_circle(ren, x, y, radius, shade_color(colPts, shade));
            }
        }
    }

    free(base); free(rot);
}

int main(int argc, char** argv){
    int N = 300;
    int mult = 2;

    if (argc >= 2){
        int tmp = atoi(argv[1]);
        if (tmp >= 20 && tmp <= 3000) N = tmp;
    }
    if (argc >= 3){
        int tmp = atoi(argv[2]);
        if (tmp != 0) mult = tmp;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0){
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const int W=1000, H=700;
    SDL_Window*  win = SDL_CreateWindow("Patrones 3D con puntos y lineas",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren){
        fprintf(stderr, "SDL_Create: %s\n", SDL_GetError());
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter();
    double t = 0.0;
    bool running = true;

    Modo  modo    = MODO_MULT;
    int   skip1   = 7;
    int   skip2   = 31;
    bool  drawPts = true;
    bool  drawLin = true;
    bool  trails  = false;

    while (running){
        SDL_Event e;
        while (SDL_PollEvent(&e)){
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN){
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) running = false;

                if (k == SDLK_1) modo = MODO_MULT;
                if (k == SDLK_2) modo = MODO_ESTRELLA;
                if (k == SDLK_3) modo = MODO_TEJIDO;

                if (k == SDLK_LEFT)  mult = (mult>1? mult-1 : 1);
                if (k == SDLK_RIGHT) mult++;

                if (k == SDLK_a) skip1++;
                if (k == SDLK_z) skip1 = (skip1>1? skip1-1 : 1);
                if (k == SDLK_s) skip2++;
                if (k == SDLK_x) skip2 = (skip2>1? skip2-1 : 1);

                if (k == SDLK_PLUS || k == SDLK_EQUALS){ N += 10; if (N>3000) N=3000; }
                if (k == SDLK_MINUS){ N -= 10; if (N<20) N=20; }

                if (k == SDLK_p) drawPts = !drawPts;
                if (k == SDLK_l) drawLin = !drawLin;
                if (k == SDLK_t) trails  = !trails;

                if (k == SDLK_r){
                    if (modo == MODO_MULT){ mult = 2; }
                    if (modo == MODO_ESTRELLA){ skip1 = 7; }
                    if (modo == MODO_TEJIDO){ skip1 = 7; skip2 = 31; }
                }
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)freq;
        last = now; t += dt;

        render_scene(ren, W, H, t, N, mult, skip1, skip2, modo, drawPts, drawLin, trails);
        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
