#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define CLAMP01(x) ((x)<0?0:((x)>1?1:(x)))

typedef struct { float x,y,z, vx,vy,vz; } Node;
typedef struct { int a,b; } Edge;

typedef struct {
    Edge* data;
    int size;
    int cap;
} EdgeVec;

static void edges_init(EdgeVec* v){ v->data=NULL; v->size=0; v->cap=0; }
static void edges_free(EdgeVec* v){ free(v->data); v->data=NULL; v->size=v->cap=0; }
static void edges_clear(EdgeVec* v){ v->size=0; }
static void edges_reserve(EdgeVec* v, int need){
    if (need <= v->cap) return;
    int newcap = v->cap? v->cap: 1024;
    while (newcap < need) newcap = newcap*2;
    v->data = (Edge*)realloc(v->data, sizeof(Edge)*newcap);
    v->cap = newcap;
}
static void edges_push(EdgeVec* v, Edge e){
    if (v->size == v->cap) edges_reserve(v, v->cap? v->cap*2 : 1024);
    v->data[v->size++] = e;
}

static float frand01(void){ return (float)rand()/(float)RAND_MAX; }
static float frand_range(float a,float b){ return a + (b-a)*frand01(); }

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
static SDL_Color shade(SDL_Color c, float k){
    k = CLAMP01(k);
    SDL_Color o = { (Uint8)(c.r*k), (Uint8)(c.g*k), (Uint8)(c.b*k), c.a };
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
    if (thick <= 1){
        set_col(r,c);
        SDL_RenderDrawLine(r, x1,y1, x2,y2);
        return;
    }
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

static bool project_perspective(float vx,float vy,float vz,
                                int W,int H, float fov, float camz,
                                int* sx,int* sy, float* zout){
    float z = vz + camz;
    if (z <= 1e-3f) return false;
    float s = fov / z;
    *sx = W/2 + (int)roundf(vx * s);
    *sy = H/2 - (int)roundf(vy * s);
    if (zout) *zout = z;
    return true;
}

static void spawn_cloud(Node* nodes, int N, int W, int H, float fov, float zmin, float zmax, int margin){
    for (int i=0;i<N;i++){
        float z = frand_range(zmin, zmax);
        float s = fov / (z + 1e-6f);
        float x_range = (W/2 - margin) / s;
        float y_range = (H/2 - margin) / s;
        nodes[i].x = frand_range(-x_range, x_range);
        nodes[i].y = frand_range(-y_range, y_range);
        nodes[i].z = z;
        nodes[i].vx = frand_range(-30.0f, 30.0f);
        nodes[i].vy = frand_range(-30.0f, 30.0f);
        nodes[i].vz = frand_range(-15.0f, 15.0f);
    }
}
static void update_cloud(Node* n, int N, float dt, int W,int H, float fov,
                         float zmin,float zmax,int margin, bool drift){
    for (int i=0;i<N;i++){
        if (drift){
            n[i].x += n[i].vx * dt;
            n[i].y += n[i].vy * dt;
            n[i].z += n[i].vz * dt;
        }
        if (n[i].z < zmin){ n[i].z = zmin; n[i].vz = fabsf(n[i].vz); }
        if (n[i].z > zmax){ n[i].z = zmax; n[i].vz = -fabsf(n[i].vz); }

        float s = fov / (n[i].z + 1e-6f);
        float x_range = (W/2 - margin) / s;
        float y_range = (H/2 - margin) / s;

        if (n[i].x < -x_range){ n[i].x = -x_range; n[i].vx = fabsf(n[i].vx); }
        if (n[i].x >  x_range){ n[i].x =  x_range; n[i].vx = -fabsf(n[i].vx); }
        if (n[i].y < -y_range){ n[i].y = -y_range; n[i].vy = fabsf(n[i].vy); }
        if (n[i].y >  y_range){ n[i].y =  y_range; n[i].vy = -fabsf(n[i].vy); }
    }
}

typedef struct {
    int gw, gh, cell;
    int *head, *next; 
} Grid;

static void grid_free(Grid* g){
    free(g->head); free(g->next);
    g->head = g->next = NULL; g->gw=g->gh=g->cell=0;
}
static void grid_build(Grid* g, int W,int H, int cell, const int* sx, int N){
    g->cell = cell;
    g->gw = (W + cell - 1) / cell;
    g->gh = (H + cell - 1) / cell;
    int cells = g->gw * g->gh;
    g->head = (int*)malloc(sizeof(int)*cells);
    g->next = (int*)malloc(sizeof(int)*N);
    for (int i=0;i<cells;i++) g->head[i] = -1;
    for (int i=0;i<N;i++){
        int x = sx[i]; if (x<0) x=0; if (x>=W) x=W-1;
        
        g->next[i] = -1;
    }
}
static void grid_build_xy(Grid* g, int W,int H, int cell, const int* sx, const int* sy, int N){
    g->cell = cell;
    g->gw = (W + cell - 1) / cell;
    g->gh = (H + cell - 1) / cell;
    int cells = g->gw * g->gh;
    g->head = (int*)malloc(sizeof(int)*cells);
    g->next = (int*)malloc(sizeof(int)*N);
    for (int i=0;i<cells;i++) g->head[i] = -1;
    for (int i=0;i<N;i++){
        int x = sx[i]; if (x<0) x=0; if (x>=W) x=W-1;
        int y = sy[i]; if (y<0) y=0; if (y>=H) y=H-1;
        int gx = x / g->cell;
        int gy = y / g->cell;
        int id = gy*g->gw + gx;
        g->next[i] = g->head[id];
        g->head[id] = i;
    }
}

static inline void grid_cell_bounds(const Grid* g, int W,int H, int cx,int cy, int* valid){
    *valid = (cx>=0 && cy>=0 && cx<g->gw && cy<g->gh);
}

typedef struct { float d2; int idx; } Cand;
static int cmp_cand(const void* a, const void* b){
    float da = ((const Cand*)a)->d2;
    float db = ((const Cand*)b)->d2;
    return (da<db)? -1 : (da>db)? 1 : 0;
}

static void build_knn_edges_grid(const int* sx,const int* sy,int N,int k,
                                 int W,int H, int cell, EdgeVec* edges, int* out_k_eff)
{
    if (k < 1) k = 1;
    edges_reserve(edges, edges->size + N*k);

    Grid g={0};
    grid_build_xy(&g, W,H, cell, sx, sy, N);

    int cap = 64;
    Cand* tmp = (Cand*)malloc(sizeof(Cand)*cap);

    for (int i=0;i<N;i++){
        int x = sx[i]; int y = sy[i];
        int gx = x / g.cell;
        int gy = y / g.cell;

        int need = k;
        int got = 0;
        int maxR = 6; 
        for (int R=0; R<=maxR && got<need; ++R){
            for (int dy=-R; dy<=R; ++dy){
                for (int dx=-R; dx<=R; ++dx){
                    int cx = gx + dx, cy = gy + dy, valid=0;
                    grid_cell_bounds(&g, W,H, cx,cy, &valid);
                    if (!valid) continue;
                    int head = g.head[cy*g.gw + cx];
                    for (int j=head; j!=-1; j=g.next[j]){
                        if (j==i) continue;
                        if (j <= i) continue;
                        float ddx = (float)(sx[j]-x);
                        float ddy = (float)(sy[j]-y);
                        float d2 = ddx*ddx + ddy*ddy;
                        if (got == cap){
                            cap *= 2;
                            tmp = (Cand*)realloc(tmp, sizeof(Cand)*cap);
                        }
                        tmp[got++] = (Cand){ d2, j };
                    }
                }
            }
            if (got >= need) break;
        }

        if (got == 0) continue;

        if (got > need){
            qsort(tmp, got, sizeof(Cand), cmp_cand);
            got = need;
        }
        for (int t=0; t<got; ++t){
            edges_push(edges, (Edge){ i, tmp[t].idx });
        }
    }

    if (out_k_eff) *out_k_eff = k;
    free(tmp);
    grid_free(&g);
}

typedef struct {
    int* p;
    int* r;
    int n;
    int comps;
} DSU;
static void dsu_init(DSU* d, int n){
    d->p = (int*)malloc(sizeof(int)*n);
    d->r = (int*)malloc(sizeof(int)*n);
    d->n = n; d->comps = n;
    for (int i=0;i<n;i++){ d->p[i]=i; d->r[i]=0; }
}
static int dsu_find(DSU* d, int x){
    while (d->p[x]!=x){ d->p[x] = d->p[d->p[x]]; x = d->p[x]; }
    return x;
}
static bool dsu_union(DSU* d, int a,int b){
    a = dsu_find(d,a); b = dsu_find(d,b);
    if (a==b) return false;
    if (d->r[a] < d->r[b]){ int t=a; a=b; b=t; }
    d->p[b] = a;
    if (d->r[a]==d->r[b]) d->r[a]++;
    d->comps--;
    return true;
}
static void dsu_free(DSU* d){ free(d->p); free(d->r); d->p=d->r=NULL; d->n=d->comps=0; }

static void connect_components_grid(EdgeVec* edges,
                                    const int* sx,const int* sy,int N,
                                    int W,int H,int cell)
{
    DSU d; dsu_init(&d, N);
    for (int e=0; e<edges->size; ++e){
        dsu_union(&d, edges->data[e].a, edges->data[e].b);
    }
    if (d.comps <= 1){ dsu_free(&d); return; }

    Grid g={0}; grid_build_xy(&g, W,H, cell, sx, sy, N);

    int max_iter = N*4; 
    int iter=0;
    while (d.comps > 1 && iter<max_iter){
        iter++;
        int i = rand()%N;
        int ci = dsu_find(&d,i);
        int x = sx[i], y = sy[i];
        int gx = x / g.cell, gy = y / g.cell;

        float bestD2 = 1e30f; int bestJ = -1;
        for (int R=0; R<=8; ++R){
            for (int dy=-R; dy<=R; ++dy){
                for (int dx=-R; dx<=R; ++dx){
                    int cx = gx + dx, cy = gy + dy, valid=0;
                    grid_cell_bounds(&g, W,H, cx,cy, &valid);
                    if (!valid) continue;
                    int head = g.head[cy*g.gw + cx];
                    for (int j=head; j!=-1; j=g.next[j]){
                        if (dsu_find(&d,j) == ci) continue;
                        float ddx=(float)(sx[j]-x), ddy=(float)(sy[j]-y);
                        float d2 = ddx*ddx + ddy*ddy;
                        if (d2 < bestD2){ bestD2=d2; bestJ=j; }
                    }
                }
            }
            if (bestJ!=-1) break;
        }
        if (bestJ!=-1){
            edges_push(edges, (Edge){ i, bestJ });
            dsu_union(&d, i, bestJ);
        }else{
            
            float best=1e30f; int bj=-1;
            for (int j=0;j<N;j++){
                if (dsu_find(&d,j) == ci) continue;
                float dx=(float)(sx[j]-x), dy=(float)(sy[j]-y);
                float d2 = dx*dx + dy*dy;
                if (d2<best){ best=d2; bj=j; }
            }
            if (bj!=-1){
                edges_push(edges, (Edge){ i, bj });
                dsu_union(&d, i, bj);
            }else break;
        }
    }
    grid_free(&g);
    dsu_free(&d);
}

static void build_mst_exact(const int* sx,const int* sy,int N, EdgeVec* edges){
    if (N<=1) return;
    const float INF = 1e30f;
    float* dist = (float*)malloc(sizeof(float)*N);
    int* parent = (int*)malloc(sizeof(int)*N);
    bool* inSet  = (bool*)malloc(sizeof(bool)*N);
    for (int i=0;i<N;i++){ dist[i]=INF; parent[i]=-1; inSet[i]=false; }
    dist[0]=0.0f;
    for (int it=0; it<N-1; ++it){
        int u=-1; float best=INF;
        for (int v=0; v<N; ++v){
            if (!inSet[v] && dist[v]<best){ best=dist[v]; u=v; }
        }
        if (u==-1) break;
        inSet[u]=true;
        for (int v=0; v<N; ++v){
            if (inSet[v] || v==u) continue;
            float dx=(float)(sx[v]-sx[u]), dy=(float)(sy[v]-sy[u]);
            float d2 = dx*dx + dy*dy;
            if (d2 < dist[v]){ dist[v]=d2; parent[v]=u; }
        }
    }
    for (int v=1; v<N; ++v){
        if (parent[v]!=-1){
            edges_push(edges, (Edge){ v, parent[v] });
        }
    }
    free(dist); free(parent); free(inSet);
}

static void render(SDL_Renderer* ren, int W,int H, double t,
                   Node* nodes,int N, float fov,
                   int k, bool useMSTExact,bool useConnect,bool useKNN,
                   bool drawPts,bool drawLines,bool trails, int gridCell)
{
    if (!trails){
        set_col(ren, (SDL_Color){12,15,22,255}); SDL_RenderClear(ren);
    }else{
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        set_col(ren, (SDL_Color){12,15,22,22});
        SDL_Rect r={0,0,W,H}; SDL_RenderFillRect(ren,&r);
    }

    // Proyecta
    int* sx = (int*)malloc(sizeof(int)*N);
    int* sy = (int*)malloc(sizeof(int)*N);
    float* zbuf = (float*)malloc(sizeof(float)*N);
    for (int i=0;i<N;i++){
        int x,y; float z;
        project_perspective(nodes[i].x, nodes[i].y, nodes[i].z, W,H, fov, 0.0f, &x,&y,&z);
        sx[i]=x; sy[i]=y; zbuf[i]=z+1e-6f;
    }

    EdgeVec edges; edges_init(&edges);

    if (useKNN){
        int k_eff=0;
        build_knn_edges_grid(sx,sy,N, k, W,H, gridCell, &edges, &k_eff);
    }
    if (useMSTExact){
        build_mst_exact(sx,sy,N, &edges);
    }else if (useConnect){
        connect_components_grid(&edges, sx,sy,N, W,H, gridCell);
    }

    float hue = fmodf(200.0f + 60.0f*sinf((float)t*0.3f), 360.0f);
    SDL_Color cLine = hsv_to_rgb(hue, 0.25f, 1.0f, 255);
    SDL_Color cPts  = hsv_to_rgb(hue+30.0f, 0.35f, 1.0f, 255);

    int thickBase = (edges.size > 150000 ? 1 : 2);

    if (drawLines){
        for (int e=0; e<edges.size; ++e){
            int i = edges.data[e].a, j = edges.data[e].b;
            float zavg = 0.5f*(zbuf[i]+zbuf[j]);
            float shadeK = CLAMP01(1.25f - (zavg/1200.0f));
            int thick = thickBase; // puedes hacer 1 + (int)(2.0f*(900.0f/zavg));
            draw_line_thick(ren, sx[i],sy[i], sx[j],sy[j], thick, shade(cLine, shadeK));
        }
    }
    if (drawPts){
        for (int i=0;i<N;i++){
            float kshade = CLAMP01(1.2f - (zbuf[i]/1200.0f));
            int radius = 1 + (int)fminf(3.0f, 3.0f * (900.0f / zbuf[i]));
            draw_filled_circle(ren, sx[i], sy[i], radius, shade(cPts, kshade));
        }
    }

    edges_free(&edges);
    free(sx); free(sy); free(zbuf);
}

int main(int argc, char** argv){
    int N = 2000;
    int k = 8;
    if (argc>=2){ long tmp=strtol(argv[1],NULL,10); if (tmp>=1) N=(int)tmp; }
    if (argc>=3){ long tmp=strtol(argv[2],NULL,10); if (tmp>=1) k=(int)tmp; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0){
        fprintf(stderr,"SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    srand((unsigned)time(NULL));

    const int W=1920, H=1080;
    SDL_Window* win = SDL_CreateWindow("Puntos sin límites + kNN por grilla",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W,H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win,-1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!win||!ren){
        fprintf(stderr,"SDL_Create: %s\n", SDL_GetError());
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit(); return 1;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    float fov = 900.0f;
    float zmin = 300.0f, zmax = 900.0f;
    int margin = 8;

    Node* nodes = (Node*)malloc(sizeof(Node)*N);
    spawn_cloud(nodes, N, W,H, fov, zmin, zmax, margin);

    bool running=true;
    bool useMSTExact=false;     
    bool useConnect=true;      
    bool useKNN=true;
    bool drawPts=true, drawLines=true;
    bool trails=false, drift=true;
    int gridCell = 48;          

    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter();
    double t=0.0;

    while (running){
        SDL_Event e;
        while (SDL_PollEvent(&e)){
            if (e.type==SDL_QUIT) running=false;
            if (e.type==SDL_KEYDOWN){
                SDL_Keycode Kc = e.key.keysym.sym;
                if (Kc==SDLK_ESCAPE) running=false;

                if (Kc==SDLK_MINUS){
                    int newN = N-200; if (newN<1) newN=1;
                    if (newN!=N){
                        N=newN; nodes=(Node*)realloc(nodes,sizeof(Node)*N);
                        spawn_cloud(nodes, N, W,H, fov, zmin, zmax, margin);
                    }
                }
                if (Kc==SDLK_PLUS || Kc==SDLK_EQUALS){
                    int newN = N+200;
                    N=newN; nodes=(Node*)realloc(nodes,sizeof(Node)*N);
                    spawn_cloud(nodes, N, W,H, fov, zmin, zmax, margin);
                }
                if (Kc==SDLK_LEFTBRACKET){ if (k>1) k--; }
                if (Kc==SDLK_RIGHTBRACKET){ k++; }

                if (Kc==SDLK_m) useMSTExact = !useMSTExact;
                if (Kc==SDLK_c) useConnect = !useConnect;
                if (Kc==SDLK_k) useKNN = !useKNN;
                if (Kc==SDLK_p) drawPts = !drawPts;
                if (Kc==SDLK_l) drawLines = !drawLines;
                if (Kc==SDLK_t) trails = !trails;
                if (Kc==SDLK_d) drift = !drift;
                if (Kc==SDLK_g){
                    if (gridCell==48) gridCell=32;
                    else if (gridCell==32) gridCell=24;
                    else if (gridCell==24) gridCell=64;
                    else gridCell=48;
                }
                if (Kc==SDLK_SPACE){
                    spawn_cloud(nodes, N, W,H, fov, zmin, zmax, margin);
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)freq;
        last = now; t += dt;

        update_cloud(nodes, N, (float)dt, W,H, fov, zmin, zmax, margin, drift);
        render(ren, W,H, t, nodes, N, fov, k, useMSTExact,useConnect,useKNN,
               drawPts,drawLines,trails, gridCell);
        SDL_RenderPresent(ren);
    }

    free(nodes);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
