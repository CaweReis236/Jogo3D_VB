// VECTOR SEARCH 0.8 - busca, construcao defensiva, escadas, andares e cameras
// Raylib 6.x | C11
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define CHUNK_SIZE 12.0f
#define HALF_CHUNK 6.0f
#define RENDER_RADIUS 1
#define GRID_N 8
#define GRID_MIN (-4)
#define GRID_MAX 3
#define LEVEL_H 6.0f
#define WORLD_MIN (-54.0f)
#define WORLD_MAX (42.0f)
#define SAVE_FILE "vector_search_save.dat"
#define LEGACY_SAVE_FILE "simulador_save.dat"
#define WALL_H 3.5f
#define WALL_T 0.18f
#define DOOR_HALF 1.6f
#define PLAYER_RADIUS 0.30f
#define PLAYER_HEIGHT 1.65f
#define EYE_H 1.55f
#define GRAVITY 19.0f
#define JUMP_SPEED 7.4f
#define BLOCK_W 1.40f
#define BLOCK_H 0.80f
#define MAX_NEAR 4096
#define MODEL_FILE "assets/player.obj"
#define PREVIEW_W 540
#define PREVIEW_H 405
#define KEYCAM_W 420
#define KEYCAM_H 252
#define PHYSICS_STEP 0.04f
#define MAX_REMOVED 8192
#define MAX_PLACED 4096
#define REACH 6.5f
#define HOLE_HALF 2.25f
#define HUNTER_RADIUS 0.36f
#define HUNTER_H 1.25f
#define HUNTER_SPEED 2.0f

static uint32_t worldSeed = 0;
static unsigned char visited[GRID_N][GRID_N][GRID_N];
static unsigned char keysFound[6];
static unsigned char keysInserted[6];
static int keyRooms[6][3];
static int currentLayer = 0;
static int mapLayer = 0;
static int totalKeys = 0;
static bool mapOpen = false;
static char message[128] = "";
static float messageTime = 0;
static bool seedFromArgs = false;

static const Color NEON = {0, 255, 75, 255};
static const Color DIM = {0, 85, 27, 255};
static const Color COR_BLOCO = {65, 150, 245, 255};
static const Color HIGHLIGHT = {130, 215, 255, 255};
static const Color BAD = {255, 75, 75, 255};
static const Color HUNTER_COLOR = {190, 58, 235, 255};

typedef struct {
    Vector3 pos, size;
    int cx, cz, slot, level;
    int placedIndex;  // -1 para gerado proceduralmente
} Block;

typedef struct { int cx, cz, slot, level; } Removed;
typedef struct { Block block; bool active; } Placed;
static Removed removed[MAX_REMOVED];
static int removedCount = 0;
static Placed placed[MAX_PLACED];
static int placedCount = 0;
static Block nearby[MAX_NEAR];
static int nearbyCount = 0;
static Block carriedOrigin;
static Vector3 hunterPos;
static int hunterLayer=0;
static float hunterCooldown=2.5f;
static float hunterSeal[6]={0};
static bool hunterEnabled=true;
static bool hunterSpawned=false;

static float MinF(float a, float b) { return a < b ? a : b; }
static float MaxF(float a, float b) { return a > b ? a : b; }
static float ClampF(float x, float a, float b) { return MaxF(a, MinF(x,b)); }
static float Snap(float x) { return roundf(x * 2.0f) / 2.0f; }
static int ChunkOf(float x) { return (int)floorf((x + HALF_CHUNK) / CHUNK_SIZE); }

static uint32_t Mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
// Blocos coloridos: saturacao alta, matiz constante para a mesma seed e bloco.
static Color BlockColor(Block b) {
    uint32_t h=Mix(worldSeed ^ (uint32_t)(b.cx+32)*14556247u ^
        (uint32_t)(b.cz+32)*9244171u ^ (uint32_t)(b.slot+13)*11258999u ^
        (uint32_t)(b.level+3)*18791623u);
    float hue=(float)(h%360u);
    if(b.slot>=100) return ColorFromHSV(hue,0.74f,0.88f);
    return ColorFromHSV(hue,0.90f,0.97f);
}
static uint32_t NewSeed(void)
{
    return ((uint32_t)GetRandomValue(0,65535) << 16) |
            (uint32_t)GetRandomValue(0,65535);
}
static bool InGrid(int c) { return c >= GRID_MIN && c <= GRID_MAX; }
static int GridIndex(int c) { return c - GRID_MIN; }
static int LayerOf(float y) { return (int)floorf((y+0.05f)/LEVEL_H); }
static float FloorY(int layer) { return layer*LEVEL_H; }
static uint32_t Hash3(int x,int y,int z,int slot) {
    return Mix(worldSeed ^ (uint32_t)(x+100)*73856093u ^
        (uint32_t)(y+100)*19349663u ^ (uint32_t)(z+100)*83492791u ^
        (uint32_t)(slot+1)*2654435761u);
}
static void Note(const char *s) { snprintf(message,sizeof message,"%s",s); messageTime=3.5f; }
static void SetKeys(void) {
    memset(keysFound,0,sizeof keysFound); memset(keysInserted,0,sizeof keysInserted); totalKeys=0;
    for(int k=0;k<6;k++) {
        int attempt=0,x,y,z; bool unique;
        do {
            uint32_t h=Hash3(k,attempt,17,9);
            x=GRID_MIN+(int)(h%GRID_N);
            y=(int)((h>>8)%GRID_N);
            z=GRID_MIN+(int)((h>>16)%GRID_N);
            unique=!(x==0 && y==0 && z==0) && (abs(x)+abs(z)+y>=4);
            for(int j=0;j<k;j++) if(keyRooms[j][0]==x && keyRooms[j][1]==y && keyRooms[j][2]==z) unique=false;
            attempt++;
        } while(!unique && attempt<1000);
        keyRooms[k][0]=x; keyRooms[k][1]=y; keyRooms[k][2]=z;
    }
}
static bool WasRemoved(Block b)
{
    for (int i = 0; i < removedCount; i++) {
        Removed r = removed[i];
        if (r.cx == b.cx && r.cz == b.cz && r.slot == b.slot && r.level == b.level)
            return true;
    }
    return false;
}
static void ResetEdits(void)
{
    removedCount = 0;
    placedCount = 0;
}
static bool RemoveBlock(Block b)
{
    if (b.placedIndex >= 0) {
        if (b.placedIndex < placedCount) {
            placed[b.placedIndex].active = false;
            return true;
        }
        return false;
    }
    if (removedCount >= MAX_REMOVED) return false;
    removed[removedCount++] = (Removed){b.cx,b.cz,b.slot,b.level};
    return true;
}
static bool AddPlaced(Block b)
{
    for (int i = 0; i < placedCount; i++) {
        if (!placed[i].active) {
            b.placedIndex = i;
            placed[i] = (Placed){b,true};
            return true;
        }
    }
    if (placedCount >= MAX_PLACED) return false;
    b.placedIndex = placedCount;
    placed[placedCount++] = (Placed){b,true};
    return true;
}
static void PushNearby(Block b)
{
    if (nearbyCount < MAX_NEAR) nearby[nearbyCount++] = b;
}

// Escadaria de blocos macicos em torno do poco central. Cada degrau sobe
// 0,75 m, menos do que a altura maxima do salto. Nao sao removiveis:
// retirar um desses blocos poderia prender definitivamente o jogador.
static void GenerateStairs(int cx,int cz,int layer) {
    if(layer>=GRID_N-1) return;
    const Vector2 path[8]={
        {2.42f,-0.45f},{2.27f,1.17f},{0.95f,2.29f},{-0.64f,2.32f},
        {-1.72f,1.40f},{-1.70f,0.18f},{-1.35f,-1.06f},{0.10f,-1.57f}
    };
    float base=FloorY(layer);
    for(int j=0;j<8;j++) {
        float tall=0.75f*(float)(j+1);
        Block b={0};b.cx=cx;b.cz=cz;b.slot=100+j;b.level=layer*16+j;
        b.placedIndex=-1;
        b.size=(Vector3){1.45f,0.45f,1.45f};
        b.pos=(Vector3){cx*CHUNK_SIZE+path[j].x,base+tall-0.225f,
                         cz*CHUNK_SIZE+path[j].y};
        PushNearby(b);
    }
}
static void GenerateChunk(int cx, int cz, int layer)
{
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=GRID_N) return;
    // Arquitetura fixa do acesso vertical.
    GenerateStairs(cx,cz,layer);
    // Blocos soltos para construir barreiras contra o Coletor.
    // Mantemos o poco e os corredores principais desobstruidos.
    if(cx==0 && cz==0 && layer==0) return;
    for(int slot=0;slot<9;slot++) {
        uint32_t h=Hash3(cx,layer,cz,slot);
        Block b={0};b.cx=cx;b.cz=cz;b.slot=slot;b.level=layer*16+slot;
        b.placedIndex=-1;
        float sx=((float)((h>>2)%7)-3.0f)*1.35f;
        float sz=((float)((h>>9)%7)-3.0f)*1.35f;
        if(fabsf(sx)<3.1f && fabsf(sz)<3.1f) continue;
        if(fabsf(sx)>2.6f && fabsf(sz)<2.5f) continue;
        if(fabsf(sz)>2.6f && fabsf(sx)<2.5f) continue;
        b.size=(Vector3){BLOCK_W,BLOCK_H,BLOCK_W};
        b.pos=(Vector3){cx*CHUNK_SIZE+sx,FloorY(layer)+BLOCK_H/2,
                          cz*CHUNK_SIZE+sz};
        if(!WasRemoved(b)) PushNearby(b);
    }
}
// Recria geometria apenas perto do jogador; as alteracoes persistem separadas.
static void RebuildNearby(Vector3 player)
{
    nearbyCount = 0;
    int layer=LayerOf(player.y);
    int cx = ChunkOf(player.x), cz = ChunkOf(player.z);
    for (int z = cz - RENDER_RADIUS - 1; z <= cz + RENDER_RADIUS + 1; z++)
        for (int x = cx - RENDER_RADIUS - 1; x <= cx + RENDER_RADIUS + 1; x++)
            for(int l=layer-1;l<=layer+1;l++) GenerateChunk(x,z,l);
    for (int i = 0; i < placedCount; i++) {
        if (placed[i].active &&
            abs(ChunkOf(placed[i].block.pos.x) - cx) <= RENDER_RADIUS + 1 &&
            abs(ChunkOf(placed[i].block.pos.z) - cz) <= RENDER_RADIUS + 1 &&
            abs(LayerOf(placed[i].block.pos.y)-layer)<=1)
            PushNearby(placed[i].block);
    }
}
static BoundingBox Box(Block b)
{
    Vector3 h = Vector3Scale(b.size,0.5f);
    return (BoundingBox){ Vector3Subtract(b.pos,h), Vector3Add(b.pos,h) };
}
static BoundingBox PlayerBox(Vector3 feet)
{
    return (BoundingBox){
        {feet.x-PLAYER_RADIUS,feet.y+0.03f,feet.z-PLAYER_RADIUS},
        {feet.x+PLAYER_RADIUS,feet.y+PLAYER_HEIGHT,feet.z+PLAYER_RADIUS}
    };
}
static bool Overlaps(BoundingBox a, BoundingBox b)
{
    const float e = 0.002f;
    return a.min.x < b.max.x-e && a.max.x > b.min.x+e &&
           a.min.y < b.max.y-e && a.max.y > b.min.y+e &&
           a.min.z < b.max.z-e && a.max.z > b.min.z+e;
}
// Os quatro limites da sala usam a mesma malha logica no desenho e colisao.
// Nas bordas, o muro e INTEIRO: nao existe passagem para fora do mapa.
static int WallsFor(int cx,int cz,int layer, BoundingBox out[8]) {
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=GRID_N) return 0;
    float mx=cx*CHUNK_SIZE,mz=cz*CHUNK_SIZE;
    float west=mx-HALF_CHUNK,north=mz-HALF_CHUNK,east=mx+HALF_CHUNK,south=mz+HALF_CHUNK;
    float fy=FloorY(layer),top=fy+WALL_H;
    int n=0;
    if(cx==GRID_MIN) out[n++]=(BoundingBox){{west-WALL_T/2,fy,north},{west+WALL_T/2,top,south}};
    else {
        out[n++]=(BoundingBox){{west-WALL_T/2,fy,north},{west+WALL_T/2,top,mz-DOOR_HALF}};
        out[n++]=(BoundingBox){{west-WALL_T/2,fy,mz+DOOR_HALF},{west+WALL_T/2,top,south}};
    }
    if(cz==GRID_MIN) out[n++]=(BoundingBox){{west,fy,north-WALL_T/2},{east,top,north+WALL_T/2}};
    else {
        out[n++]=(BoundingBox){{west,fy,north-WALL_T/2},{mx-DOOR_HALF,top,north+WALL_T/2}};
        out[n++]=(BoundingBox){{mx+DOOR_HALF,fy,north-WALL_T/2},{east,top,north+WALL_T/2}};
    }
    if(cx==GRID_MAX) out[n++]=(BoundingBox){{east-WALL_T/2,fy,north},{east+WALL_T/2,top,south}};
    if(cz==GRID_MAX) out[n++]=(BoundingBox){{west,fy,south-WALL_T/2},{east,top,south+WALL_T/2}};
    return n;
}
// Malha geometrica compartilhada por pisos e testes de colisao.
static int FloorParts(int cx,int cz,int layer,BoundingBox parts[4]) {
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=GRID_N)return 0;
    float x=cx*CHUNK_SIZE,z=cz*CHUNK_SIZE,y=FloorY(layer);
    int n=0;
    if(layer==0) {
        parts[n++]=(BoundingBox){{x-HALF_CHUNK,y-0.17f,z-HALF_CHUNK},
                                 {x+HALF_CHUNK,y-0.02f,z+HALF_CHUNK}};
    } else {
        parts[n++]=(BoundingBox){{x-HALF_CHUNK,y-0.17f,z-HALF_CHUNK},
                                 {x-HOLE_HALF,y-0.02f,z+HALF_CHUNK}};
        parts[n++]=(BoundingBox){{x+HOLE_HALF,y-0.17f,z-HALF_CHUNK},
                                 {x+HALF_CHUNK,y-0.02f,z+HALF_CHUNK}};
        parts[n++]=(BoundingBox){{x-HOLE_HALF,y-0.17f,z-HALF_CHUNK},
                                 {x+HOLE_HALF,y-0.02f,z-HOLE_HALF}};
        parts[n++]=(BoundingBox){{x-HOLE_HALF,y-0.17f,z+HOLE_HALF},
                                 {x+HOLE_HALF,y-0.02f,z+HALF_CHUNK}};
    }
    return n;
}
static bool FloorHit(BoundingBox body) {
    int cx=ChunkOf((body.min.x+body.max.x)/2);
    int cz=ChunkOf((body.min.z+body.max.z)/2);
    for(int l=0;l<GRID_N;l++) {
        if(body.max.y<FloorY(l)-0.17f || body.min.y>FloorY(l)-0.02f)continue;
        for(int x=cx-1;x<=cx+1;x++) for(int z=cz-1;z<=cz+1;z++) {
            BoundingBox parts[4];int n=FloorParts(x,z,l,parts);
            for(int i=0;i<n;i++)if(Overlaps(body,parts[i]))return true;
        }
    }
    return false;
}
static bool WallsHit(BoundingBox body) {
    if(body.min.x < WORLD_MIN || body.max.x > WORLD_MAX ||
       body.min.z < WORLD_MIN || body.max.z > WORLD_MAX ||
       body.min.y < -0.1f || body.max.y > GRID_N*LEVEL_H-0.01f) return true;
    int cx=ChunkOf((body.min.x+body.max.x)/2),cz=ChunkOf((body.min.z+body.max.z)/2);
    for(int l=0;l<GRID_N;l++) {
        if(body.max.y<FloorY(l) || body.min.y>FloorY(l)+WALL_H) continue;
        for(int z=cz-1;z<=cz+1;z++) for(int x=cx-1;x<=cx+1;x++) {
            BoundingBox w[8];int count=WallsFor(x,z,l,w);
            for(int k=0;k<count;k++) if(Overlaps(body,w[k])) return true;
        }
    }
    return false;
}
static bool PlayerCollides(Vector3 feet) {
    BoundingBox p=PlayerBox(feet);
    if(WallsHit(p) || FloorHit(p)) return true;
    for(int i=0;i<nearbyCount;i++) if(Overlaps(p,Box(nearby[i]))) return true;
    return false;
}
// Piso pleno no andar 0; nos superiores, um buraco quadrado central permite
// descer. Todas as salas tem sua propria abertura.
static bool FloorSolid(int layer, float x,float z) {
    if(layer<0 || layer>=GRID_N) return false;
    if(layer==0) return true;
    int cx=ChunkOf(x),cz=ChunkOf(z);
    if(!InGrid(cx)||!InGrid(cz)) return false;
    float lx=x-cx*CHUNK_SIZE,lz=z-cz*CHUNK_SIZE;
    return !(fabsf(lx)<HOLE_HALF && fabsf(lz)<HOLE_HALF);
}
static bool OnBlock(Vector3 feet) {
    for(int i=0;i<nearbyCount;i++) {
        BoundingBox b=Box(nearby[i]);
        if(fabsf(feet.y-b.max.y)<0.055f &&
           feet.x+PLAYER_RADIUS>b.min.x && feet.x-PLAYER_RADIUS<b.max.x &&
           feet.z+PLAYER_RADIUS>b.min.z && feet.z-PLAYER_RADIUS<b.max.z) return true;
    }
    return false;
}
static bool HasSupport(Vector3 p) {
    for(int l=0;l<GRID_N;l++) {
        if(fabsf(p.y-FloorY(l))<0.035f && FloorSolid(l,p.x,p.z)) return true;
    }
    return OnBlock(p);
}
// Fisica por subpassos: 2 eixos horizontais independentes, depois Y.
// A descida cruza piso apenas se houver piso; um buraco e realmente vazado.
static void MovePlayerSolid(Vector3 *p,float dx,float dz,float *vy,float dt,bool *grounded) {
    float longest=MaxF(MaxF(fabsf(dx),fabsf(dz)),fabsf(*vy*dt)+GRAVITY*dt*dt);
    int steps=(int)ceilf(longest/PHYSICS_STEP);
    if(steps<1) steps=1;
    if(steps>128) steps=128;
    float h=dt/(float)steps;
    for(int i=0;i<steps;i++) {
        Vector3 test=*p;test.x+=dx/steps;
        if(!PlayerCollides(test))p->x=test.x;
        test=*p;test.z+=dz/steps;
        if(!PlayerCollides(test))p->z=test.z;
        if(*grounded && !HasSupport(*p)) *grounded=false;
        if(*grounded)continue;
        *vy-=GRAVITY*h;
        float ny=p->y+*vy*h;
        if(*vy<=0) {
            bool landed=false;float landing=-1e8f;
            for(int l=0;l<GRID_N;l++) {
                float f=FloorY(l);
                if(p->y>=f-0.005f && ny<=f && FloorSolid(l,p->x,p->z) && f>landing){landing=f;landed=true;}
            }
            for(int k=0;k<nearbyCount;k++) {
                BoundingBox box=Box(nearby[k]);
                if(p->x+PLAYER_RADIUS <= box.min.x || p->x-PLAYER_RADIUS>=box.max.x ||
                   p->z+PLAYER_RADIUS <= box.min.z || p->z-PLAYER_RADIUS>=box.max.z)continue;
                if(p->y>=box.max.y-0.005f && ny<=box.max.y && box.max.y>landing){landing=box.max.y;landed=true;}
            }
            if(landed){p->y=landing;*vy=0;*grounded=true;continue;}
        } else {
            // O teto de cada andar e macico, exceto no poco.
            bool hitCeiling=false;
            for(int l=1;l<GRID_N;l++) {
                float under=FloorY(l)-0.17f;
                if(p->y+PLAYER_HEIGHT<=under && ny+PLAYER_HEIGHT>=under &&
                   FloorSolid(l,p->x,p->z)) {
                    ny=under-PLAYER_HEIGHT;hitCeiling=true;break;
                }
            }
            if(hitCeiling)*vy=0;
        }
        test=*p;test.y=ny;
        if(!PlayerCollides(test))p->y=ny;
        else { *vy=0; if(ny < p->y)*grounded=true; }
    }
}
// Cada sala recebe uma cor suave e ESTAVEL, calculada a partir da seed.
// As diferencas entre canais RGB sao pequenas: cores pouco saturadas.
static Color RoomColor(int cx, int cz, int layer)
{
    static const Color palette[] = {
        {109, 120, 128, 255}, // azul acinzentado
        {117, 111, 129, 255}, // lavanda discreta
        {113, 125, 109, 255}, // verde salvia
        {129, 113, 107, 255}, // argila
        {110, 125, 123, 255}, // verde petroleo apagado
        {133, 123, 110, 255}, // areia
        {122, 112, 122, 255}, // malva acinzentado
        {114, 121, 134, 255}, // ardossia
        {128, 118, 121, 255}, // rosa acinzentado
        {122, 127, 111, 255}, // oliva palido
        {111, 122, 136, 255}, // azul de aco
        {121, 114, 109, 255}, // pedra
    };
    uint32_t h = Hash3(cx,layer,cz,57);
    return palette[h % (sizeof(palette) / sizeof(palette[0]))];
}
static Color Shade(Color c, float factor)
{
    return (Color){
        (unsigned char)ClampF(c.r * factor, 0.0f, 255.0f),
        (unsigned char)ClampF(c.g * factor, 0.0f, 255.0f),
        (unsigned char)ClampF(c.b * factor, 0.0f, 255.0f),
        255
    };
}

// Retangulos do piso separados pelo poco central.
static void DrawFloor(int cx,int cz,int layer,Color color,bool wire) {
    BoundingBox parts[4];int n=FloorParts(cx,cz,layer,parts);
    for(int i=0;i<n;i++) {
        BoundingBox b=parts[i];
        Vector3 center=Vector3Scale(Vector3Add(b.min,b.max),0.5f);
        Vector3 size=Vector3Subtract(b.max,b.min);
        if(wire)DrawCubeWires(center,size.x,size.y,size.z,color);
        else DrawCube(center,size.x,size.y,size.z,color);
    }
}
static void DrawWallBox(BoundingBox b, Color c,bool wire) {
    Vector3 ctr=Vector3Scale(Vector3Add(b.min,b.max),0.5f);
    Vector3 size=Vector3Subtract(b.max,b.min);
    if(wire) DrawCubeWires(ctr,size.x,size.y,size.z,c);
    else DrawCube(ctr,size.x,size.y,size.z,c);
}
static void DrawFilledRoom(int cx,int cz,int layer) {
    Color base=RoomColor(cx,cz,layer);
    DrawFloor(cx,cz,layer,Shade(base,0.53f),false);
    // O desenho utiliza as MESMAS AABB da colisao.
    BoundingBox walls[8];int n=WallsFor(cx,cz,layer,walls);
    for(int i=0;i<n;i++) DrawWallBox(walls[i],Shade(base,0.88f),false);
    // As bordas leste/sul dos chunks internos pertencem ao vizinho;
    // desenhamos aqui tambem como paredes da sala atual.
    if(cx<GRID_MAX) {
        BoundingBox east[8];int ne=WallsFor(cx+1,cz,layer,east);
        for(int i=0;i<ne;i++) if(fabsf(east[i].min.x-(cx*CHUNK_SIZE+HALF_CHUNK-WALL_T/2))<0.01f)
            DrawWallBox(east[i],Shade(base,0.88f),false);
    }
    if(cz<GRID_MAX) {
        BoundingBox south[8];int ns=WallsFor(cx,cz+1,layer,south);
        for(int i=0;i<ns;i++) if(fabsf(south[i].min.z-(cz*CHUNK_SIZE+HALF_CHUNK-WALL_T/2))<0.01f)
            DrawWallBox(south[i],Shade(base,0.88f),false);
    }
}
// NUNCA desenhar salas verticalmente distantes. So as laterais do
// mesmo andar, com linhas de estilo terminal retro.
static void DrawChunkEnvironment(int cx,int cz,int layer,int acx,int acz,int aly) {
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=GRID_N)return;
    if(cx==acx && cz==acz && layer==aly){DrawFilledRoom(cx,cz,layer);return;}
    DrawFloor(cx,cz,layer,DIM,true);
    float ox=cx*CHUNK_SIZE, oz=cz*CHUNK_SIZE,fy=FloorY(layer)+0.01f;
    // Grade retro realmente desenhada sobre as salas laterais.
    // As linhas sao interrompidas sobre o buraco central dos andares.
    for(int i=-6;i<=6;i++) {
        float k=(float)i;
        if(layer>0 && fabsf(k)<HOLE_HALF) {
            DrawLine3D((Vector3){ox+k,fy,oz-HALF_CHUNK},
                       (Vector3){ox+k,fy,oz-HOLE_HALF},DIM);
            DrawLine3D((Vector3){ox+k,fy,oz+HOLE_HALF},
                       (Vector3){ox+k,fy,oz+HALF_CHUNK},DIM);
            DrawLine3D((Vector3){ox-HALF_CHUNK,fy,oz+k},
                       (Vector3){ox-HOLE_HALF,fy,oz+k},DIM);
            DrawLine3D((Vector3){ox+HOLE_HALF,fy,oz+k},
                       (Vector3){ox+HALF_CHUNK,fy,oz+k},DIM);
        } else {
            DrawLine3D((Vector3){ox+k,fy,oz-HALF_CHUNK},
                       (Vector3){ox+k,fy,oz+HALF_CHUNK},DIM);
            DrawLine3D((Vector3){ox-HALF_CHUNK,fy,oz+k},
                       (Vector3){ox+HALF_CHUNK,fy,oz+k},DIM);
        }
    }
    BoundingBox walls[8];int n=WallsFor(cx,cz,layer,walls);
    for(int i=0;i<n;i++) DrawWallBox(walls[i],NEON,true);
}
static Ray AimRay(Camera3D camera)
{
    Vector3 d=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    return (Ray){camera.position,d};
}
// Seleciona a caixa visada, limitada pela distancia de interacao.
static int RayBlock(Ray ray, RayCollision *bestHit)
{
    int found=-1;
    float dist=REACH;
    for (int i=0;i<nearbyCount;i++) {
        if(nearby[i].slot>=100 && nearby[i].placedIndex<0) continue;
        RayCollision h=GetRayCollisionBox(ray,Box(nearby[i]));
        if (h.hit && h.distance>0.05f && h.distance<dist) {
            found=i; dist=h.distance; *bestHit=h;
        }
    }
    return found;
}
// Verifica posicionamento em qualquer piso que a caixa atravessa, incluindo
// paredes da borda; as verificacoes de colisao sao compartilhadas com o player.
static bool CanPlace(Block b,Vector3 player) {
    BoundingBox dest=Box(b);
    if(dest.min.x<WORLD_MIN+0.12f ||dest.max.x>WORLD_MAX-0.12f ||
       dest.min.z<WORLD_MIN+0.12f ||dest.max.z>WORLD_MAX-0.12f ||
       dest.min.y<-0.001f ||dest.max.y>GRID_N*LEVEL_H-0.1f) return false;
    if(Overlaps(dest,PlayerBox(player)) || WallsHit(dest) || FloorHit(dest)) return false;
    for(int i=0;i<nearbyCount;i++)if(Overlaps(dest,Box(nearby[i])))return false;
    // impedir criar um bloco que ocupa o piso macico de qualquer andar
    for(int l=1;l<GRID_N;l++) {
        float f=FloorY(l);
        if(dest.min.y<f && dest.max.y>f-0.17f && FloorSolid(l,b.pos.x,b.pos.z))return false;
    }
    return true;
}
static Block NewLooseBlock(Vector3 position) {
    Block b={0};b.size=(Vector3){BLOCK_W,BLOCK_H,BLOCK_W};
    b.pos=position;b.placedIndex=-1;b.cx=ChunkOf(position.x);b.cz=ChunkOf(position.z);
    b.slot=1;b.level=LayerOf(position.y)*16;return b;
}
// Prioriza a face visada e cai em varias tentativas seguras ao redor.
// O clique nao fica preso num unico ponto invalido ao apontar para uma parede.
static bool SuggestPlacement(Ray ray,RayCollision hit,int hitIndex,Vector3 player,Block *result) {
    float base=FloorY(LayerOf(player.y));
    if(hitIndex>=0) {
        Block t=nearby[hitIndex];Vector3 n=hit.normal;
        Block b=NewLooseBlock((Vector3){
            t.pos.x+n.x*(t.size.x+BLOCK_W)/2,
            t.pos.y+n.y*(t.size.y+BLOCK_H)/2,
            t.pos.z+n.z*(t.size.z+BLOCK_W)/2});
        if(CanPlace(b,player)){*result=b;return true;}
    }
    // Intersecao com plano do piso; para mirar pisos mesmo sem blocos.
    if(ray.direction.y < -0.01f) {
        float t=(base-ray.position.y)/ray.direction.y;
        if(t>0 && t<REACH) {
            Vector3 pos=Vector3Add(ray.position,Vector3Scale(ray.direction,t));
            Block b=NewLooseBlock((Vector3){Snap(pos.x),base+BLOCK_H/2,Snap(pos.z)});
            if(CanPlace(b,player)){*result=b;return true;}
        }
    }
    // Primeiro tenta adiante; depois percorre um anel com espacos livres.
    for(int ring=0;ring<6;ring++) {
        float r=1.6f+0.44f*ring;
        for(int j=0;j<16;j++) {
            float angle=atan2f(ray.direction.z,ray.direction.x)+(float)j*(2*PI/16);
            Block b=NewLooseBlock((Vector3){Snap(player.x+cosf(angle)*r),
                           base+BLOCK_H/2,Snap(player.z+sinf(angle)*r)});
            if(CanPlace(b,player)){*result=b;return true;}
        }
    }
    return false;
}
// O V cancela uma coleta impossivel, restaurando o bloco original.
// Isso impede soft-locks em corredores lotados.
static void CancelCarry(void) {
    if(carriedOrigin.placedIndex>=0) {
        if(carriedOrigin.placedIndex<placedCount)
            placed[carriedOrigin.placedIndex].active=true;
    } else {
        for(int i=removedCount-1;i>=0;i--) {
            Removed r=removed[i];
            if(r.cx==carriedOrigin.cx&&r.cz==carriedOrigin.cz&&
               r.slot==carriedOrigin.slot&&r.level==carriedOrigin.level){
                removed[i]=removed[--removedCount];break;
            }
        }
    }
}
static void ResetPlayer(Vector3 *player,float *vy,float *yaw,float *pitch,bool *grounded,bool *carrying)
{
    *player=(Vector3){0,0,0}; *vy=0;
    *yaw=0; *pitch=0; *grounded=true; *carrying=false;
}
// Sem elevadores: apenas as escadas formadas de blocos na sala.
static void DrawHex(Vector3 p,float radius,Color c) {
    for(int i=0;i<6;i++) {
        float a=i*PI/3.0f,b=(i+1)*PI/3.0f;
        Vector3 v={p.x+cosf(a)*radius,p.y,p.z+sinf(a)*radius};
        Vector3 w={p.x+cosf(b)*radius,p.y,p.z+sinf(b)*radius};
        DrawLine3D(v,w,c);
    }
}
static void DrawKey(Vector3 p, Color c) {
    Vector3 a={p.x,p.y+0.60f,p.z};
    Vector3 b={p.x-0.42f,p.y-0.15f,p.z};
    Vector3 d={p.x+0.42f,p.y-0.15f,p.z};
    DrawTriangle3D(a,b,d,c);
    DrawTriangle3D(d,b,a,c);
    DrawLine3D(a,b,WHITE); DrawLine3D(b,d,WHITE); DrawLine3D(d,a,WHITE);
}
static void DrawArtifacts(int cx,int cz,int layer) {
    if(layer==0 && cx==0 && cz==0) {
        DrawHex((Vector3){0,0.07f,0},2.0f,(Color){230,195,90,255});
        for(int i=0;i<6;i++) {
            float a=(i+0.5f)*2*PI/6;
            Vector3 p={cosf(a)*1.25f,0.13f,sinf(a)*1.25f};
            DrawHex(p,0.36f,keysInserted[i]?(Color){235,215,90,255}:GRAY);
            if(keysInserted[i]) DrawKey((Vector3){p.x,0.4f,p.z},GOLD);
        }
    }
    for(int i=0;i<6;i++) {
        if(!keysFound[i] && cx==keyRooms[i][0] && cz==keyRooms[i][2] && layer==keyRooms[i][1]) {
            DrawKey((Vector3){cx*CHUNK_SIZE+2.0f,FloorY(layer)+1.1f,cz*CHUNK_SIZE+2.0f},
                    hunterSeal[i]>0?(Color){145,60,210,255}:(Color){255,215,105,255});
        }
    }
}
static bool InteractKeys(Vector3 p) {
    for(int i=0;i<6;i++) {
        if(keysFound[i]) continue;
        Vector3 kp={keyRooms[i][0]*CHUNK_SIZE+2.0f,FloorY(keyRooms[i][1])+1.1f,keyRooms[i][2]*CHUNK_SIZE+2.0f};
        if(Vector3Distance(p,kp)<2.8f) {
            if(hunterSeal[i]>0.0f){ Note("Chave lacrada pelo Coletor: volte quando o lacre apagar."); return true; }
            keysFound[i]=1; totalKeys++;
            Note("TRIANGULO COLETADO! Volte ao hexagono central.");
            return true;
        }
    }
    if(Vector3Distance(p,(Vector3){0,0,0})<3.0f) {
        int inserted=0;
        for(int i=0;i<6;i++) {
            if(keysFound[i] && !keysInserted[i]) keysInserted[i]=1;
            if(keysInserted[i]) inserted++;
        }
        Note(inserted==6?"HEXAGONO COMPLETO! AS SEIS CHAVES FORAM ENCAIXADAS!":"Chaves coletadas encaixadas no hexagono.");
        return true;
    }
    return false;
}
// O COLETOR persegue chaves ou jogador. Nao atravessa portas bloqueadas
// com blocos: barricadas fazem parte da estrategia de exploracao.
static void SpawnHunter(Vector3 player) {
    hunterLayer=LayerOf(player.y);
    int cx=ChunkOf(player.x),cz=ChunkOf(player.z);
    int tx=cx+3; if(tx>GRID_MAX)tx=cx-3;
    int tz=cz+2; if(tz>GRID_MAX)tz=cz-2;
    hunterPos=(Vector3){tx*CHUNK_SIZE,FloorY(hunterLayer),tz*CHUNK_SIZE};
    hunterCooldown=3.0f;hunterSpawned=true;
}
static bool HunterCollides(Vector3 p) {
    BoundingBox box={{p.x-HUNTER_RADIUS,p.y+0.04f,p.z-HUNTER_RADIUS},
                     {p.x+HUNTER_RADIUS,p.y+HUNTER_H,p.z+HUNTER_RADIUS}};
    if(WallsHit(box)||FloorHit(box))return true;
    for(int i=0;i<nearbyCount;i++) {
        // O Coletor atravessa a escadaria fixa, mas NAO as barricadas
        // e os blocos soltos que o jogador reposiciona.
        if(nearby[i].slot>=100 && nearby[i].placedIndex<0) continue;
        if(Overlaps(box,Box(nearby[i])))return true;
    }
    return false;
}
static void UpdateHunter(float dt,Vector3 player) {
    if(!hunterEnabled)return;
    for(int i=0;i<6;i++)hunterSeal[i]=MaxF(0,hunterSeal[i]-dt);
    if(!hunterSpawned||hunterLayer!=LayerOf(player.y))SpawnHunter(player);
    hunterCooldown=MaxF(0,hunterCooldown-dt);
    Vector3 target=player;
    // Havendo chave coletada ainda nao encaixada, o Coletor a persegue.
    bool hasCargo=false;
    for(int i=0;i<6;i++)if(keysFound[i]&&!keysInserted[i])hasCargo=true;
    if(!hasCargo) {
        float best=1e30f;int kbest=-1;
        for(int k=0;k<6;k++) {
            if(keysFound[k]||keyRooms[k][1]!=hunterLayer||hunterSeal[k]>0)continue;
            Vector3 kp={keyRooms[k][0]*CHUNK_SIZE+2,
                        FloorY(hunterLayer),keyRooms[k][2]*CHUNK_SIZE+2};
            float d=Vector3Distance(hunterPos,kp);
            if(d<best){best=d;kbest=k;target=kp;}
        }
        (void)kbest;
    }
    int ecx=ChunkOf(hunterPos.x),ecz=ChunkOf(hunterPos.z);
    int tcx=ChunkOf(target.x),tcz=ChunkOf(target.z);
    // Navigation via centros de portas. A colisao fisica, nao a IA,
    // determina se uma barricada de blocos permite passagem.
    Vector3 waypoint=target;
    if(ecx!=tcx) {
        waypoint.x=ecx*CHUNK_SIZE+(tcx>ecx?HALF_CHUNK+1:-HALF_CHUNK-1);
        waypoint.z=ecz*CHUNK_SIZE;
    } else if(ecz!=tcz) {
        waypoint.x=ecx*CHUNK_SIZE;
        waypoint.z=ecz*CHUNK_SIZE+(tcz>ecz?HALF_CHUNK+1:-HALF_CHUNK-1);
    }
    Vector3 move={waypoint.x-hunterPos.x,0,waypoint.z-hunterPos.z};
    float dist=sqrtf(move.x*move.x+move.z*move.z);
    // Grava blocos do chunk do Coletor, mesmo quando o jogador esta longe.
    RebuildNearby(hunterPos);
    if(dist>0.06f) {
        float speed=MinF(HUNTER_SPEED*dt,dist)/dist;
        // Mesmo sistema sub-step do player: inimigo nao atravessa paredes.
        int n=(int)ceilf(HUNTER_SPEED*dt/0.04f);if(n<1)n=1;
        for(int j=0;j<n;j++) {
            Vector3 trial=hunterPos;trial.x+=move.x*speed/n;
            if(!HunterCollides(trial))hunterPos.x=trial.x;
            trial=hunterPos;trial.z+=move.z*speed/n;
            if(!HunterCollides(trial))hunterPos.z=trial.z;
        }
    }
    // Contato com o jogador: devolve uma chave a sala original.
    if(hunterCooldown<=0 && Vector3Distance(hunterPos,player)<1.25f) {
        for(int k=5;k>=0;k--) if(keysFound[k]&&!keysInserted[k]) {
            keysFound[k]=0;totalKeys--;hunterCooldown=12.0f;
            Note("O COLETOR ROUBOU UMA CHAVE! Bloqueie suas portas com blocos.");
            break;
        }
    }
    // Uma chave no chao pode ser lacrada temporariamente.
    if(hunterCooldown<=0)for(int k=0;k<6;k++) {
        if(keysFound[k]||keyRooms[k][1]!=hunterLayer||hunterSeal[k]>0)continue;
        Vector3 kp={keyRooms[k][0]*CHUNK_SIZE+2,FloorY(hunterLayer),keyRooms[k][2]*CHUNK_SIZE+2};
        if(Vector3Distance(hunterPos,kp)<1.7f){
            hunterSeal[k]=18.0f;hunterCooldown=4.0f;
            Note("O COLETOR LACROU UMA CHAVE POR 18 SEGUNDOS.");
            break;
        }
    }
    RebuildNearby(player);
}
static void DrawHunter(void) {
    if(!hunterEnabled||!hunterSpawned)return;
    DrawSphere((Vector3){hunterPos.x,hunterPos.y+0.78f,hunterPos.z},0.48f,HUNTER_COLOR);
    DrawSphereWires((Vector3){hunterPos.x,hunterPos.y+0.78f,hunterPos.z},0.61f,8,10,(Color){255,125,255,255});
}
static void MarkVisited(Vector3 p) {
    int x=ChunkOf(p.x),z=ChunkOf(p.z),y=LayerOf(p.y);
    if(InGrid(x)&&InGrid(z)&&y>=0&&y<GRID_N) visited[GridIndex(x)][y][GridIndex(z)]=1;
}
static void DrawVisitedCircle(int x,int y,int z) {
    if(!visited[GridIndex(x)][y][GridIndex(z)]) return;
    Vector3 p={x*CHUNK_SIZE-3.8f,FloorY(y)+0.04f,z*CHUNK_SIZE-3.8f};
    DrawCircle3D(p,0.65f,(Vector3){1,0,0},90.0f,(Color){240,210,65,255});
}
static void DrawMap(void) {
    const int cell=54, x0=SCREEN_W/2-4*cell, y0=145;
    DrawRectangle(0,0,SCREEN_W,SCREEN_H,(Color){8,13,21,250});
    DrawText("CARTOGRAFIA // MEMORIA DE EXPLORACAO",x0,60,23,SKYBLUE);
    DrawText(TextFormat("CAMADA CONSULTADA %d/7  |  RODA DO MOUSE: ALTERAR CAMADA",mapLayer),x0,98,17,LIGHTGRAY);
    for(int z=0;z<GRID_N;z++) for(int x=0;x<GRID_N;x++) {
        int xx=x0+x*cell,yy=y0+z*cell;
        bool seen=visited[x][mapLayer][z]!=0;
        DrawRectangle(xx,yy,cell-3,cell-3,seen?(Color){66,82,87,255}:(Color){22,27,37,255});
        DrawRectangleLines(xx,yy,cell-3,cell-3,(Color){83,91,105,255});
        if(seen) DrawCircle(xx+cell/2,yy+cell/2,5,(Color){235,208,70,255});
    }
    DrawText("Amarelo = sala ja visitada. Sem indicador da sua posicao.",x0,y0+GRID_N*cell+20,17,LIGHTGRAY);
    DrawText("BOTAO DIREITO: FECHAR | Movimento bloqueado enquanto o mapa esta aberto",x0,y0+GRID_N*cell+48,16,LIGHTGRAY);
    DrawText(TextFormat("CHAVES %d/6",totalKeys),x0,y0+GRID_N*cell+80,19,GOLD);
}
// OBJ substituivel em assets/player.obj: geometria ja na escala do jogador
// (pe no Y=0, altura ~1.65). A colisao e fixa, independentemente do OBJ.
static bool LoadAvatar(Model *avatar)
{
    if(!FileExists(MODEL_FILE)) return false;
    *avatar=LoadModel(MODEL_FILE);
    return avatar->meshCount>0 && avatar->meshes!=NULL;
}
static void DrawAvatar(const Model *avatar, bool loaded, Vector3 feet, float yaw)
{
    Color tint={180,225,232,255};
    if(loaded) {
        DrawModelEx(*avatar,feet,(Vector3){0,1,0},-yaw*RAD2DEG,(Vector3){1,1,1},tint);
    } else {
        // Boneco reserva se o OBJ tiver sido removido ou estiver invalido.
        DrawCube((Vector3){feet.x,feet.y+0.96f,feet.z},0.41f,0.64f,0.25f,tint);
        DrawCube((Vector3){feet.x,feet.y+1.47f,feet.z},0.33f,0.34f,0.33f,tint);
        DrawCube((Vector3){feet.x-0.12f,feet.y+0.35f,feet.z},0.16f,0.68f,0.19f,tint);
        DrawCube((Vector3){feet.x+0.12f,feet.y+0.35f,feet.z},0.16f,0.68f,0.19f,tint);
    }
}
// O primeiro OBJ aparece no menu, antes de iniciar ou carregar jogo.
static void RenderMenuPreview(RenderTexture2D texture,const Model *avatar,bool loaded)
{
    Camera3D preview={0};
    preview.position=(Vector3){2.9f,2.8f,4.6f};
    preview.target=(Vector3){0,1.0f,0};
    preview.up=(Vector3){0,1,0};
    preview.fovy=54;
    preview.projection=CAMERA_PERSPECTIVE;
    BeginTextureMode(texture);
    ClearBackground((Color){10,18,25,255});
    BeginMode3D(preview);
    DrawFilledRoom(0,0,0);
    DrawAvatar(avatar,loaded,(Vector3){0,0,0},0);
    // Regua 1,65 m: escala comparavel ao colisor do jogador.
    DrawLine3D((Vector3){0.62f,0,0},(Vector3){0.62f,PLAYER_HEIGHT,0},GOLD);
    DrawCubeWires((Vector3){0.62f,PLAYER_HEIGHT/2,0},0.01f,PLAYER_HEIGHT,0.01f,GOLD);
    DrawCube((Vector3){-2.0f,0.5f,-1.8f},1.0f,1.0f,1.0f,(Color){35,180,255,255});
    EndMode3D();
    EndTextureMode();
}
static int NearestUncollectedKey(Vector3 p)
{
    int result=-1; float best=1e30f;
    for(int i=0;i<6;i++) {
        if(keysFound[i]) continue;
        Vector3 kp={(float)keyRooms[i][0]*CHUNK_SIZE+2.0f,
                    FloorY(keyRooms[i][1])+1.1f,
                    (float)keyRooms[i][2]*CHUNK_SIZE+2.0f};
        Vector3 delta=Vector3Subtract(kp,p);
        float d=Vector3DotProduct(delta,delta);
        if(d<best) {best=d;result=i;}
    }
    return result;
}
// Vista isometrica de uma SALA INTEIRA, sem informar a coordenada dela.
// A camera e independente da visao do jogador.
static void RenderKeyCamera(RenderTexture2D texture,Vector3 player,float yaw,
                            const Model *avatar,bool loaded,int key)
{
    int cx=0,cz=0,layer=0;
    if(key>=0) {
        cx=keyRooms[key][0]; layer=keyRooms[key][1]; cz=keyRooms[key][2];
    }
    float fx=cx*CHUNK_SIZE, fy=FloorY(layer),fz=cz*CHUNK_SIZE;
    Camera3D cam={0};
    cam.position=(Vector3){fx+11.0f,fy+18.0f,fz+12.0f};
    cam.target=(Vector3){fx,fy+0.8f,fz};
    cam.up=(Vector3){0,1,0};
    cam.fovy=20.0f;
    cam.projection=CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(texture);
    ClearBackground((Color){7,11,19,255});
    BeginMode3D(cam);
    DrawFilledRoom(cx,cz,layer);
    DrawVisitedCircle(cx,layer,cz);
    DrawArtifacts(cx,cz,layer);
    // Gera SOMENTE o chunk observado: a chave pode estar muito longe.
    // Usa a memoria temporaria de proximidades sem modificar os saves.
    nearbyCount=0;
    GenerateChunk(cx,cz,layer);
    for(int j=0;j<placedCount;j++) {
        if(placed[j].active && ChunkOf(placed[j].block.pos.x)==cx &&
           ChunkOf(placed[j].block.pos.z)==cz &&
           LayerOf(placed[j].block.pos.y)==layer)
            PushNearby(placed[j].block);
    }
    for(int i=0;i<nearbyCount;i++) {
        Block b=nearby[i];
        if(ChunkOf(b.pos.x)==cx && ChunkOf(b.pos.z)==cz &&
           LayerOf(b.pos.y)==layer)
            DrawCube(b.pos,b.size.x,b.size.y,b.size.z,BlockColor(b));
    }
    if(hunterSpawned && hunterLayer==layer && ChunkOf(hunterPos.x)==cx && ChunkOf(hunterPos.z)==cz)DrawHunter();
    // O jogador aparece nessa transmissao somente quando esta nesta sala.
    if(ChunkOf(player.x)==cx && ChunkOf(player.z)==cz && LayerOf(player.y)==layer)
        DrawAvatar(avatar,loaded,player,yaw);
    EndMode3D();
    EndTextureMode();
    RebuildNearby(player); // restaura proximidades para os proximos eventos
}
static void DrawTexturePanel(Texture2D t,int x,int y,int w,int h)
{
    DrawTexturePro(t,(Rectangle){0,0,(float)t.width,-(float)t.height},
                   (Rectangle){(float)x,(float)y,(float)w,(float)h},
                   (Vector2){0,0},0,WHITE);
    DrawRectangleLines(x,y,w,h,(Color){90,160,192,255});
}
typedef struct {
    char magic[8];
    uint32_t version,seed;
    Vector3 player;
    float yaw,pitch,vy;
    uint32_t removedCount,placedCount;
    unsigned char visited[GRID_N][GRID_N][GRID_N];
    unsigned char keysFound[6];
    unsigned char keysInserted[6];
    unsigned char carrying;
    Removed removed[MAX_REMOVED];
    Placed placed[MAX_PLACED];
} SaveData;
// Novo save possui os dados originais e as mecanicas adicionais.
// A leitura do save antigo e suportada, com ajuste de altura de 12 -> 6.
typedef struct {
    SaveData base;
    char tag[8];
    Vector3 carriedOriginPos;
    Block carriedOrigin;
    Vector3 hunterPos;
    int hunterLayer;
    float hunterCooldown;
    float hunterSeal[6];
} SearchSave;
static void FillBase(SaveData *s,Vector3 p,float yaw,float pitch,float vy,bool carrying) {
    memcpy(s->magic,"VS08",5);s->version=8;s->seed=worldSeed;
    s->player=p;s->yaw=yaw;s->pitch=pitch;s->vy=vy;
    s->removedCount=(uint32_t)removedCount;s->placedCount=(uint32_t)placedCount;
    memcpy(s->visited,visited,sizeof visited);memcpy(s->keysFound,keysFound,sizeof keysFound);
    memcpy(s->keysInserted,keysInserted,sizeof keysInserted);s->carrying=carrying?1:0;
    memcpy(s->removed,removed,(size_t)removedCount*sizeof(Removed));
    memcpy(s->placed,placed,(size_t)placedCount*sizeof(Placed));
}
static bool SaveGame(Vector3 p,float yaw,float pitch,float vy,bool carrying) {
    SearchSave *s=calloc(1,sizeof *s);if(!s)return false;
    FillBase(&s->base,p,yaw,pitch,vy,carrying);
    memcpy(s->tag,"SEARCH8",8);
    s->carriedOriginPos=carriedOrigin.pos;s->carriedOrigin=carriedOrigin;
    s->hunterPos=hunterPos;s->hunterLayer=hunterLayer;s->hunterCooldown=hunterCooldown;
    memcpy(s->hunterSeal,hunterSeal,sizeof hunterSeal);
    FILE *f=fopen(SAVE_FILE ".tmp","wb");
    bool ok=f && fwrite(s,sizeof *s,1,f)==1;
    if(f && fclose(f)!=0)ok=false;
    if(ok && rename(SAVE_FILE ".tmp",SAVE_FILE)!=0)ok=false;
    if(!ok)remove(SAVE_FILE ".tmp");
    free(s);return ok;
}
static bool RestoreBase(const SaveData *s,Vector3 *p,float *yaw,float *pitch,float *vy,bool *carrying,bool legacy) {
    if(s->removedCount>MAX_REMOVED||s->placedCount>MAX_PLACED||
       s->player.x<WORLD_MIN||s->player.x>WORLD_MAX||
       s->player.z<WORLD_MIN||s->player.z>WORLD_MAX||
       s->player.y<0||s->player.y>96)return false;
    worldSeed=s->seed;*p=s->player;*yaw=s->yaw;*pitch=s->pitch;*vy=s->vy;
    if(legacy) {
        int level=(int)floorf(p->y/12.0f);
        p->y=ClampF(level*LEVEL_H+fmodf(p->y,12.0f)*0.5f,0,GRID_N*LEVEL_H-PLAYER_HEIGHT);
    }
    removedCount=(int)s->removedCount;placedCount=(int)s->placedCount;
    memcpy(removed,s->removed,(size_t)removedCount*sizeof(Removed));
    memcpy(placed,s->placed,(size_t)placedCount*sizeof(Placed));
    memcpy(visited,s->visited,sizeof visited);
    *carrying=s->carrying!=0;
    SetKeys();totalKeys=0;
    memcpy(keysFound,s->keysFound,sizeof keysFound);
    memcpy(keysInserted,s->keysInserted,sizeof keysInserted);
    for(int i=0;i<6;i++)if(keysFound[i])totalKeys++;
    if(legacy) {
        for(int i=0;i<placedCount;i++)if(placed[i].active){
            int level=(int)floorf(placed[i].block.pos.y/12.0f);
            placed[i].block.pos.y=level*LEVEL_H+fmodf(placed[i].block.pos.y,12.0f)*0.5f;
        }
    }
    return true;
}
static bool LoadGame(Vector3 *p,float *yaw,float *pitch,float *vy,bool *carrying) {
    FILE *f=fopen(SAVE_FILE,"rb");
    if(f) {
        SearchSave *s=calloc(1,sizeof *s);
        bool ok=s && fread(s,sizeof *s,1,f)==1;
        fclose(f);
        ok=ok && memcmp(s->tag,"SEARCH8",7)==0 &&
            memcmp(s->base.magic,"VS08",4)==0 && s->base.version==8;
        if(ok)ok=RestoreBase(&s->base,p,yaw,pitch,vy,carrying,false);
        if(ok){
            carriedOrigin=s->carriedOrigin;
            hunterPos=s->hunterPos;hunterLayer=s->hunterLayer;
            hunterCooldown=s->hunterCooldown;memcpy(hunterSeal,s->hunterSeal,sizeof hunterSeal);
            hunterSpawned=true;
        }
        free(s);
        if(ok)return true;
    }
    f=fopen(LEGACY_SAVE_FILE,"rb");if(!f)return false;
    SaveData *s=malloc(sizeof *s);
    bool ok=s && fread(s,sizeof *s,1,f)==1;
    fclose(f);
    ok=ok && memcmp(s->magic,"VECT06",6)==0 && s->version==1;
    if(ok)ok=RestoreBase(s,p,yaw,pitch,vy,carrying,true);
    free(s);
    if(ok){
        memset(hunterSeal,0,sizeof hunterSeal);hunterSpawned=false;
        // Save antigo nao guardava origem do item carregado: evita travamento
        // devolvendo-o ao inventario de colocacao da versao nova.
        carriedOrigin=(Block){0};carriedOrigin.placedIndex=-1;
        *carrying=false;
        Note("Save antigo importado. Salve novamente com Ctrl+S.");
    }
    return ok;
}
static void NewGame(uint32_t seed,Vector3 *p,float *vy,float *yaw,float *pitch,bool *carrying) {
    worldSeed=seed;ResetEdits();memset(visited,0,sizeof visited);SetKeys();
    ResetPlayer(p,vy,yaw,pitch,&(bool){true},carrying);
    currentLayer=mapLayer=0;mapOpen=false;MarkVisited(*p);
    memset(hunterSeal,0,sizeof hunterSeal);hunterSpawned=false;hunterCooldown=2.5f;
    memset(&carriedOrigin,0,sizeof carriedOrigin);
    Note("Novo mundo gerado. Seis triangulos estao escondidos.");
}
int main(int argc,char **argv) {
    uint32_t argSeed=0;
    if(argc>1) {
        char *end=NULL;errno=0;unsigned long long n=strtoull(argv[1],&end,10);
        if(errno||end==argv[1]||*end||n>UINT32_MAX||argv[1][0]=='-') {
            fprintf(stderr,"Seed invalida: 0 a 4294967295\n");return 1;
        }
        argSeed=(uint32_t)n;seedFromArgs=true;
    }
    InitWindow(SCREEN_W,SCREEN_H,"VECTOR SEARCH // 0.8");
    SetExitKey(KEY_NULL);SetTargetFPS(60);
    Model avatar={0};bool avatarLoaded=LoadAvatar(&avatar);
    RenderTexture2D menuPreview=LoadRenderTexture(PREVIEW_W,PREVIEW_H);
    RenderTexture2D keyPreview=LoadRenderTexture(KEYCAM_W,KEYCAM_H);
    bool keyCameraEnabled=true;
    bool menu=true,playing=false,grounded=true,carrying=false;
    Vector3 player={0};float yaw=0,pitch=0,vy=0;
    Camera3D camera={0};camera.up=(Vector3){0,1,0};camera.fovy=75;camera.projection=CAMERA_PERSPECTIVE;
    while(!WindowShouldClose()) {
        float dt=MinF(GetFrameTime(),1.0f/30.0f);
        if(messageTime>0) messageTime-=dt;
        if(menu) {
            EnableCursor();
            if(IsKeyPressed(KEY_P)) {
                if(avatarLoaded) UnloadModel(avatar);
                avatar=(Model){0};avatarLoaded=LoadAvatar(&avatar);
                Note(avatarLoaded?"Modelo OBJ recarregado.":"OBJ ausente: usando boneco reserva.");
            }
            if(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_N)) {
                NewGame(seedFromArgs?argSeed:NewSeed(),&player,&vy,&yaw,&pitch,&carrying);
                menu=false;playing=true;DisableCursor();
            }
            if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_L)) {
                if(LoadGame(&player,&yaw,&pitch,&vy,&carrying)) {grounded=false;menu=false;playing=true;DisableCursor();Note("Ultimo save carregado.");
                } else Note("Save inexistente, danificado ou de outra versao.");
            }
            RenderMenuPreview(menuPreview,&avatar,avatarLoaded);
            BeginDrawing();ClearBackground((Color){7,12,20,255});
            DrawText("VECTOR SEARCH 0.8",64,95,46,(Color){90,190,245,255});
            DrawText("AS SEIS CHAVES // O COLETOR",70,162,25,GOLD);
            DrawText("1 / N - NOVO MUNDO",80,294,28,LIGHTGRAY);
            DrawText("2 / L - CARREGAR ULTIMO SAVE",80,354,25,LIGHTGRAY);
            DrawText("P - RECARREGAR PLAYER.OBJ",80,419,20,SKYBLUE);
            DrawText("96 x 96 x 48 | 8 andares | 6 chaves",80,483,18,GRAY);
            DrawText("O OBJ nao muda a colisao (1,65 m).",80,516,18,GRAY);
            DrawTexturePanel(menuPreview.texture,688,150,PREVIEW_W,PREVIEW_H);
            DrawText("PREVIA DO JOGADOR NA SALA | REGUA = 1,65 m",688,568,16,GOLD);
            if(!avatarLoaded) DrawText("ARQUIVO assets/player.obj NAO ENCONTRADO",688,590,16,BAD);
            if(messageTime>0) DrawText(message,70,600,18,GOLD);
            EndDrawing();continue;
        }
        if(!playing) continue;
        if(IsKeyPressed(KEY_ESCAPE)) {menu=true;playing=false;mapOpen=false;continue;}
        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            mapOpen=!mapOpen;
            if(mapOpen) {mapLayer=LayerOf(player.y);EnableCursor();}
            else DisableCursor();
        }
        // Salvar funciona tanto durante a exploracao quanto com o mapa aberto.
        if((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S))
            Note(SaveGame(player,yaw,pitch,vy,carrying)?"Mundo salvo em vector_search_save.dat":"Erro ao salvar. Confira permissoes de escrita.");
        if(mapOpen) {
            float wheel=GetMouseWheelMove();
            if(wheel>0) mapLayer=(mapLayer+1)%GRID_N;
            if(wheel<0) mapLayer=(mapLayer+GRID_N-1)%GRID_N;
            BeginDrawing();ClearBackground(BLACK);DrawMap();EndDrawing();continue;
        }
        if(IsKeyPressed(KEY_Q)) keyCameraEnabled=!keyCameraEnabled;
        if(IsKeyPressed(KEY_R)) {
            NewGame(IsKeyDown(KEY_LEFT_SHIFT)?worldSeed:NewSeed(),&player,&vy,&yaw,&pitch,&carrying);
            grounded=true;
        }
        Vector2 delta=GetMouseDelta();yaw+=delta.x*0.0025f;
        pitch=ClampF(pitch-delta.y*0.0025f,-1.45f,1.45f);
        currentLayer=LayerOf(player.y);
        RebuildNearby(player);
        float fw=(float)(IsKeyDown(KEY_W)-IsKeyDown(KEY_S));
        float side=(float)(IsKeyDown(KEY_D)-IsKeyDown(KEY_A));
        float dx=sinf(yaw)*fw+cosf(yaw)*side;
        float dz=-cosf(yaw)*fw+sinf(yaw)*side;
        float len=sqrtf(dx*dx+dz*dz);if(len>0) {dx/=len;dz/=len;}
        float speed=IsKeyDown(KEY_LEFT_SHIFT)?7.5f:4.5f;
        if(IsKeyPressed(KEY_SPACE) && grounded) {vy=JUMP_SPEED;grounded=false;}
        MovePlayerSolid(&player,dx*speed*dt,dz*speed*dt,&vy,dt,&grounded);
        currentLayer=LayerOf(player.y);
        // A subida ocorre pulando pelos degraus coloridos ao redor do poco.
        MarkVisited(player);
        UpdateHunter(dt,player);
        if(IsKeyPressed(KEY_F)) InteractKeys(player);
        camera.position=(Vector3){player.x,player.y+EYE_H,player.z};
        camera.target=(Vector3){player.x+cosf(pitch)*sinf(yaw),player.y+EYE_H+sinf(pitch),player.z-cosf(pitch)*cosf(yaw)};
        RebuildNearby(player);
        Ray ray=AimRay(camera);RayCollision hit={0};int selected=RayBlock(ray,&hit);
        Block preview={0};bool canPlace=false;
        if(carrying) canPlace=SuggestPlacement(ray,hit,selected,player,&preview);
        if(carrying && IsKeyPressed(KEY_V)){CancelCarry();carrying=false;Note("Coleta cancelada: bloco restaurado.");RebuildNearby(player);}
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!carrying && selected>=0) {
                carriedOrigin=nearby[selected];
                if(RemoveBlock(carriedOrigin)) carrying=true;
                RebuildNearby(player);
            } else if(carrying && canPlace) {
                if(AddPlaced(preview)) carrying=false;
                RebuildNearby(player);
            }
        }
        int cx=ChunkOf(player.x),cz=ChunkOf(player.z),ly=LayerOf(player.y);
        int watchedKey=NearestUncollectedKey(player);
        if(keyCameraEnabled) RenderKeyCamera(keyPreview,player,yaw,&avatar,avatarLoaded,watchedKey);
        BeginDrawing();ClearBackground(BLACK);BeginMode3D(camera);
        for(int layer=ly;layer<=ly;layer++)
            for(int z=cz-RENDER_RADIUS;z<=cz+RENDER_RADIUS;z++)
                for(int x=cx-RENDER_RADIUS;x<=cx+RENDER_RADIUS;x++) {
                    DrawChunkEnvironment(x,z,layer,cx,cz,ly);
                    if(InGrid(x)&&InGrid(z)&&layer>=0&&layer<GRID_N) {
                        DrawVisitedCircle(x,layer,z);
                        DrawArtifacts(x,z,layer);
                    }
                }
        for(int i=0;i<nearbyCount;i++) {
            Block b=nearby[i];
            if(abs(ChunkOf(b.pos.x)-cx)>RENDER_RADIUS || abs(ChunkOf(b.pos.z)-cz)>RENDER_RADIUS) continue;
            bool sameLevel=(LayerOf(b.pos.y)==ly);
            bool fromBelow=(b.slot>=100 && b.placedIndex<0 &&
                b.pos.y+b.size.y/2>=FloorY(ly)-0.05f &&
                ChunkOf(b.pos.x)==cx && ChunkOf(b.pos.z)==cz &&
                LayerOf(b.pos.y)==ly-1);
            if(!sameLevel && !fromBelow)continue;
            bool inRoom=(ChunkOf(b.pos.x)==cx && ChunkOf(b.pos.z)==cz);
            Color color=(!carrying&&selected==i)?HIGHLIGHT:BlockColor(b);
            if(inRoom) DrawCube(b.pos,b.size.x,b.size.y,b.size.z,color);
            else DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,DIM);
            if(inRoom && !carrying && selected==i)
                DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,WHITE);
        }
        if(carrying) {
            if(canPlace)DrawCube(preview.pos,preview.size.x,preview.size.y,preview.size.z,
                              (Color){35,125,185,125});
            if(canPlace)DrawCubeWires(preview.pos,preview.size.x,preview.size.y,preview.size.z,HIGHLIGHT);
            Vector3 hand=Vector3Add(ray.position,Vector3Scale(ray.direction,1.35f));
            DrawCubeWires(hand,0.25f,0.25f,0.25f,HIGHLIGHT);
        }
        if(hunterLayer==ly && ChunkOf(hunterPos.x)>=cx-RENDER_RADIUS &&
           ChunkOf(hunterPos.x)<=cx+RENDER_RADIUS &&
           ChunkOf(hunterPos.z)>=cz-RENDER_RADIUS &&
           ChunkOf(hunterPos.z)<=cz+RENDER_RADIUS)DrawHunter();
        EndMode3D();
        DrawText("VECTOR SEARCH 0.8 | WASD | ESPACO | SHIFT | CLICK BLOCO | F CHAVE",16,16,17,NEON);
        DrawText("DIR. MAPA | CTRL+S SAVE | V CANCELAR BLOCO | ESC MENU | Q CAMERA",16,42,16,NEON);
        DrawText("SUBIR: ESCADA DE BLOCOS | DESCER: POCO CENTRAL | BLOQUEIE O COLETOR",16,67,16,SKYBLUE);
        DrawText(TextFormat("SEED %u | ANDAR %d | CHAVES %d/6",worldSeed,ly,totalKeys),16,94,18,GOLD);
        if(messageTime>0) DrawText(message,16,126,18,GOLD);
        if(keyCameraEnabled) {
            int px=SCREEN_W-KEYCAM_W-16,py=SCREEN_H-KEYCAM_H-47;
            DrawTexturePanel(keyPreview.texture,px,py,KEYCAM_W,KEYCAM_H);
            DrawRectangle(px,py-25,KEYCAM_W,25,(Color){13,20,30,240});
            DrawText(watchedKey>=0?"CAMERA AUXILIAR | SALA DA CHAVE MAIS PROXIMA":
                                      "CAMERA AUXILIAR | HEXAGONO CENTRAL",px+10,py-20,15,GOLD);
        }
        DrawLine(SCREEN_W/2-6,SCREEN_H/2,SCREEN_W/2+6,SCREEN_H/2,NEON);
        DrawLine(SCREEN_W/2,SCREEN_H/2-6,SCREEN_W/2,SCREEN_H/2+6,NEON);
        EndDrawing();
    }
    UnloadRenderTexture(menuPreview);
    UnloadRenderTexture(keyPreview);
    if(avatarLoaded) UnloadModel(avatar);
    CloseWindow();return 0;
}
