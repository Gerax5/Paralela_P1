#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define CLAMP(x,a,b) ((x)<(a)?(a):((x)>(b)?(b):(x)))

typedef struct { float x,y; } Vec2;
static inline Vec2 v_add(Vec2 a, Vec2 b){ return (Vec2){a.x+b.x,a.y+b.y}; }
static inline Vec2 v_sub(Vec2 a, Vec2 b){ return (Vec2){a.x-b.x,a.y-b.y}; }
static inline Vec2 v_scale(Vec2 a,float s){ return (Vec2){a.x*s,a.y*s}; }
static inline float v_len2(Vec2 a){ return a.x*a.x + a.y*a.y; }
static inline float v_len(Vec2 a){ float L=v_len2(a); return L>0?sqrtf(L):0.0f; }
static inline Vec2 v_norm(Vec2 a){ float L=v_len(a); return (L>1e-6f)? v_scale(a,1.0f/L) : (Vec2){0,0}; }
static inline Vec2 v_perp(Vec2 a){ return (Vec2){-a.y, a.x}; }

typedef struct { float x,y,theta,strength,sep; } Dipole; // dipolo = dos cargas +/- separadas
typedef struct { Vec2 p; } Particle;

// ---------- Color (HSV -> RGB) ----------
static SDL_Color hsv_to_rgb(float h, float s, float v, Uint8 a){
    while(h<0)h+=360; while(h>=360)h-=360;
    float c=v*s, x=c*(1-fabsf(fmodf(h/60.0f,2.0f)-1.0f)), m=v-c;
    float r=0,g=0,b=0;
    if      (h< 60){ r=c; g=x; }
    else if (h<120){ r=x; g=c; }
    else if (h<180){ g=c; b=x; }
    else if (h<240){ g=x; b=c; }
    else if (h<300){ r=x; b=c; }
    else           { r=c; b=x; }
    SDL_Color col={(Uint8)((r+m)*255),(Uint8)((g+m)*255),(Uint8)((b+m)*255),a};
    return col;
}
static void set_col(SDL_Renderer* r, SDL_Color c){ SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); }

// ---------- Triángulo relleno ----------
static void draw_filled_triangle(SDL_Renderer* r, int x0,int y0,int x1,int y1,int x2,int y2, SDL_Color c){
    // orden por y
    if (y1<y0){ int tx=x0,ty=y0; x0=x1;y0=y1; x1=tx;y1=ty; }
    if (y2<y0){ int tx=x0,ty=y0; x0=x2;y0=y2; x2=tx;y2=ty; }
    if (y2<y1){ int tx=x1,ty=y1; x1=x2;y1=y2; x2=tx;y2=ty; }
    set_col(r,c);
    if (y0==y2) return;
    float dx02 = (y2!=y0)? (float)(x2-x0)/(float)(y2-y0):0;
    float dx01 = (y1!=y0)? (float)(x1-x0)/(float)(y1-y0):0;
    float dx12 = (y2!=y1)? (float)(x2-x1)/(float)(y2-y1):0;
    float sx=(float)x0, ex=(float)x0;
    for(int y=y0;y<y1;y++){ SDL_RenderDrawLine(r,(int)roundf(sx),y,(int)roundf(ex),y); sx+=dx02; ex+=dx01; }
    ex=(float)x1;
    for(int y=y1;y<=y2;y++){ SDL_RenderDrawLine(r,(int)roundf(sx),y,(int)roundf(ex),y); sx+=dx02; ex+=dx12; }
}

// ---------- Aleatorio ----------
static float frand01(void){ return (float)rand()/(float)RAND_MAX; }
static float frand_r(float a,float b){ return a + (b-a)*frand01(); }

// ---------- Campo vectorial ----------
typedef enum { FIELD_DIPOLE=0, FIELD_MONO=1, FIELD_SWIRL=2 } FieldType;

static Vec2 contrib_charge(Vec2 p, Vec2 c, float q, float soft, float alpha){
    // E ~ q * (p - c) / (|p-c|^2 + soft)^alpha ; alpha=1 ~ “visual” 1/r^2 en 2D
    Vec2 d = v_sub(p,c);
    float r2 = v_len2(d) + soft;
    float k  = q / powf(r2, alpha);
    return v_scale(d, k);
}

// Helpers 100% C para acumular contribuciones
static inline void add_dipole_acc(Vec2 p, const Dipole* d, Vec2* F, float soft, float alpha){
    float cs = cosf(d->theta), sn = sinf(d->theta);
    Vec2 dir   = (Vec2){ cs, sn };
    Vec2 rPlus = v_add((Vec2){d->x, d->y}, v_scale(dir, +d->sep*0.5f));
    Vec2 rMin  = v_add((Vec2){d->x, d->y}, v_scale(dir, -d->sep*0.5f));
    *F = v_add(*F, contrib_charge(p, rPlus, +d->strength, soft, alpha));
    *F = v_add(*F, contrib_charge(p, rMin , -d->strength, soft, alpha));
}
static inline void add_mono_acc(Vec2 p, const Dipole* d, Vec2* F, float soft, float alpha){
    Vec2 c = (Vec2){ d->x, d->y }; // "monopolo" centrado
    *F = v_add(*F, contrib_charge(p, c, d->strength, soft, alpha));
}

// Campo total en p
static Vec2 field_at(Vec2 p,
                     const Dipole* fixed, int fixedCount,
                     const Dipole* mouseDip,
                     FieldType kind)
{
    const float soft  = 25.0f; // suavizado
    const float alpha = 1.0f;  // decaimiento
    Vec2 F = (Vec2){0,0};

    // Imanes fijos
    for (int i=0; i<fixedCount; ++i){
        if (kind == FIELD_DIPOLE){
            add_dipole_acc(p, &fixed[i], &F, soft, alpha);
        } else if (kind == FIELD_MONO){
            add_mono_acc(p, &fixed[i], &F, soft, alpha);
        } else { // FIELD_SWIRL: perpendicular a la contribución dipolar
            Vec2 before = F;
            add_dipole_acc(p, &fixed[i], &F, soft, alpha);
            Vec2 inc = v_sub(F, before);
            F = v_add(before, v_perp(inc));
        }
    }

    // Dipolo del mouse (opcional)
    if (mouseDip){
        if (kind == FIELD_DIPOLE){
            add_dipole_acc(p, mouseDip, &F, soft, alpha);
        } else if (kind == FIELD_MONO){
            add_mono_acc(p, mouseDip, &F, soft, alpha);
        } else { // FIELD_SWIRL
            Vec2 before = F;
            add_dipole_acc(p, mouseDip, &F, soft, alpha);
            Vec2 inc = v_sub(F, before);
            F = v_add(before, v_perp(inc));
        }
    }

    return F;
}

// ---------- Dibujo del vector como triángulo ----------
static void draw_vector_triangle(SDL_Renderer* ren, Vec2 c, Vec2 v, float scaleLen, float baseW,
                                 float vmaxColor, float hueBase){
    float mag = v_len(v);
    if (mag < 1e-4f) return;
    Vec2 d = v_scale(v_norm(v), scaleLen * CLAMP(mag / 300.0f, 0.25f, 2.5f));
    Vec2 n = v_perp(d);

    // Geometría del triángulo (punta hacia d)
    Vec2 tip  = v_add(c, d);
    Vec2 back = v_add(c, v_scale(d,-0.35f));
    Vec2 lft  = v_add(back, v_scale(v_norm(n), +baseW));
    Vec2 rgt  = v_add(back, v_scale(v_norm(n), -baseW));

    float hue = hueBase + 120.0f * CLAMP(mag / vmaxColor, 0.0f, 1.0f);
    SDL_Color col = hsv_to_rgb(hue, 0.6f, 1.0f, 230);
    draw_filled_triangle(ren, (int)roundf(tip.x),(int)roundf(tip.y),
                              (int)roundf(lft.x),(int)roundf(lft.y),
                              (int)roundf(rgt.x),(int)roundf(rgt.y), col);
}

// ---------- Programa principal ----------
int main(int argc, char** argv){
    // ---------- argumentos: # de partículas (posicional 1) ----------
    int P = 3000; // por defecto
    if (argc >= 2){
        char* endp = NULL;
        long v = strtol(argv[1], &endp, 10);
        if (endp != argv[1] && v >= 0 && v <= 2000000000L){
            P = (int)v;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){
        fprintf(stderr,"SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    srand((unsigned)time(NULL));

    const int W=1280, H=800;
    SDL_Window*  win = SDL_CreateWindow("Magnetic-like Field with Triangles",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W,H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win,-1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!win||!ren){ fprintf(stderr,"SDL_Create: %s\n", SDL_GetError()); if(ren)SDL_DestroyRenderer(ren); if(win)SDL_DestroyWindow(win); SDL_Quit(); return 1; }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // Malla (la dejamos tal cual)
    int cell = 48; // densidad inicial (64/48/32/24)
    int nx = (W+cell-1)/cell, ny=(H+cell-1)/cell;

    // Partículas (usando P de la línea de comandos)
    Particle* parts = (Particle*)malloc(sizeof(Particle)*P);
    for(int i=0;i<P;i++){ parts[i].p=(Vec2){ frand_r(0,W), frand_r(0,H) }; }

    // Imanes fijos
    int cap=16, count=0;
    Dipole* fixed = (Dipole*)malloc(sizeof(Dipole)*cap);

    // Dipolo del mouse
    bool useMouseDip=true;
    Dipole mouse = { W*0.5f, H*0.5f, 0.0f, 8000.0f, 100.0f }; // fuerza y separación
    float mouseSpeed = 1.0f; // respuesta para seguir al cursor
    int mx= W/2, my= H/2; Uint32 mbtns=0;

    // Estado
    bool drawVectors=true, drawParticles=true, trails=true;
    FieldType fieldKind = FIELD_DIPOLE;

    Uint64 freq=SDL_GetPerformanceFrequency(), last=SDL_GetPerformanceCounter();
    double t=0.0; bool running=true;

    while(running){
        // --- input ---
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=false;
            if(e.type==SDL_MOUSEMOTION){ mx=e.motion.x; my=e.motion.y; }
            if(e.type==SDL_MOUSEBUTTONDOWN || e.type==SDL_MOUSEBUTTONUP){ mbtns=SDL_GetMouseState(NULL,NULL); }
            if(e.type==SDL_MOUSEWHEEL){
                if(e.wheel.y>0) mouse.strength *= 1.1f;
                if(e.wheel.y<0) mouse.strength *= 0.9f;
                mouse.strength = CLAMP(mouse.strength, 1000.0f, 1e7f);
            }
            if(e.type==SDL_KEYDOWN){
                SDL_Keycode k=e.key.keysym.sym;
                if(k==SDLK_ESCAPE) running=false;
                if(k==SDLK_q) mouse.theta -= 0.1f;
                if(k==SDLK_e) mouse.theta += 0.1f;

                if(k==SDLK_v) drawVectors = !drawVectors;
                if(k==SDLK_o) drawParticles = !drawParticles;
                if(k==SDLK_t) trails = !trails;
                if(k==SDLK_m) useMouseDip = !useMouseDip;

                if(k==SDLK_f) fieldKind = (FieldType)((fieldKind+1)%3);

                if(k==SDLK_r){
                    for(int i=0;i<P;i++) parts[i].p=(Vec2){ frand_r(0,W), frand_r(0,H) };
                }
                if(k==SDLK_PLUS || k==SDLK_EQUALS){
                    int old=P; P+=1000; parts=(Particle*)realloc(parts,sizeof(Particle)*P);
                    for(int i=old;i<P;i++) parts[i].p=(Vec2){ frand_r(0,W), frand_r(0,H) };
                }
                if(k==SDLK_MINUS){
                    P -= 1000; if(P<0) P=0; parts=(Particle*)realloc(parts,sizeof(Particle)*P);
                }
                if(k==SDLK_g){
                    if(cell==64) cell=48; else if(cell==48) cell=32; else if(cell==32) cell=24; else cell=64;
                    nx=(W+cell-1)/cell; ny=(H+cell-1)/cell;
                }
            }
            if(e.type==SDL_MOUSEBUTTONDOWN){
                if(e.button.button==SDL_BUTTON_LEFT){
                    if(count==cap){ cap*=2; fixed=(Dipole*)realloc(fixed,sizeof(Dipole)*cap); }
                    fixed[count++] = (Dipole){ (float)mx,(float)my, mouse.theta, mouse.strength, mouse.sep };
                }else if(e.button.button==SDL_BUTTON_RIGHT){
                    if(count>0) count--;
                }else if(e.button.button==SDL_BUTTON_MIDDLE){
                    count=0;
                }
            }
        }

        Uint64 now=SDL_GetPerformanceCounter();
        double dt=(double)(now-last)/(double)freq; last=now; t+=dt;
        float fdt = (float)dt;

        // mover suavemente el dipolo del mouse hacia el cursor
        mouse.x = mouse.x + (mx - mouse.x) * CLAMP(mouseSpeed*fdt*10.0f, 0.0f, 1.0f);
        mouse.y = mouse.y + (my - mouse.y) * CLAMP(mouseSpeed*fdt*10.0f, 0.0f, 1.0f);

        // clear / trails
        if(!trails){ set_col(ren,(SDL_Color){8,10,16,255}); SDL_RenderClear(ren); }
        else{ set_col(ren,(SDL_Color){8,10,16,18}); SDL_Rect R={0,0,W,H}; SDL_RenderFillRect(ren,&R); }

        // --- dibujar malla de triángulos (vector field) ---
        if(drawVectors){
            float hueBase = fmodf(210.0f + 60.0f*sinf(0.3f*(float)t), 360.0f);
            for(int gy=0; gy<ny; ++gy){
                for(int gx=0; gx<nx; ++gx){
                    Vec2 c = { gx*cell + cell*0.5f, gy*cell + cell*0.5f };
                    Vec2 v = field_at(c, fixed, count, useMouseDip? &mouse:NULL, fieldKind);
                    draw_vector_triangle(ren, c, v, /*scaleLen*/cell*0.9f, /*baseW*/cell*0.23f,
                                         /*vmaxColor*/6000.0f, hueBase);
                }
            }
        }

        // --- partículas advectadas por el campo ---
        if(drawParticles && P>0){
            set_col(ren, (SDL_Color){255, 255, 255, 200});
            for(int i=0;i<P;i++){
                Vec2 p = parts[i].p;
                Vec2 v = field_at(p, fixed, count, useMouseDip? &mouse:NULL, fieldKind);
                Vec2 dir = v_norm(v);
                float speed = 90.0f + 5000.0f * CLAMP(v_len(v)/8000.0f, 0.0f, 1.0f);
                p = v_add(p, v_scale(dir, speed * fdt));
                // wrap toroidal
                if(p.x<0) p.x+=W; else if(p.x>=W) p.x-=W;
                if(p.y<0) p.y+=H; else if(p.y>=H) p.y-=H;
                parts[i].p = p;
                SDL_RenderDrawPoint(ren, (int)p.x, (int)p.y);
            }
        }

        SDL_RenderPresent(ren);
    }

    free(parts);
    free(fixed);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
