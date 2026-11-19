// VECTOR SYSTEM 0.7 - avatar OBJ, colisoes robustas, cameras de observacao
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
#define LEVEL_H 12.0f
#define WORLD_MIN (-48.0f)
#define WORLD_MAX (48.0f)
#define SAVE_FILE "simulador_save.dat"
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

// Cada chunk possui quatro construcoes, cada uma com varios pisos.
// O deslocamento lateral de cada nivel cria escadarias e torres escalaveis.
static void GenerateChunk(int cx, int cz, int layer)
{
    if (!InGrid(cx)||!InGrid(cz)||layer<0||layer>=GRID_N) return;
    if(cx==0 && cz==0 && layer==0) return;
    for (int slot = 0; slot < 4; slot++) {
        uint32_t h = Hash3(cx,layer,cz,slot);
        int floors = 2 + (int)(h % 5u); // 2 a 7 blocos de altura
        float sx = (slot & 1) ? 3.25f : -3.25f;
        float sz = (slot & 2) ? 3.25f : -3.25f;
        float dirx = (h & 0x100u) ? 1.0f : -1.0f;
        float dirz = (h & 0x200u) ? 1.0f : -1.0f;
        bool stairsX = (h & 0x400u) != 0;
        float jitterX = (float)((h >> 16) & 7u) * 0.08f - 0.28f;
        float jitterZ = (float)((h >> 20) & 7u) * 0.08f - 0.28f;
        for (int level = 0; level < floors; level++) {
            Block b = {0};
            b.cx = cx; b.cz = cz; b.slot = slot; b.level = layer*16+level;
            b.placedIndex = -1;
            b.size = (Vector3){BLOCK_W, BLOCK_H, BLOCK_W};
            float step = (float)level * 0.46f;
            b.pos.x = cx * CHUNK_SIZE + sx + jitterX + (stairsX ? dirx * step : dirx * step * 0.28f);
            b.pos.z = cz * CHUNK_SIZE + sz + jitterZ + (stairsX ? dirz * step * 0.28f : dirz * step);
            b.pos.y = FloorY(layer) + (level + 0.5f) * BLOCK_H;
            if (!WasRemoved(b)) PushNearby(b);
        }
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
// Testa TODAS as paredes cuja altura cruza a caixa do jogador.
// Antes usavamos apenas LayerOf(feet.y), permitindo atravesar paredes
// ao pular proximo da divisao entre andares.
static bool WallAt(Vector3 feet)
{
    if (feet.x-PLAYER_RADIUS < WORLD_MIN || feet.x+PLAYER_RADIUS > WORLD_MAX ||
        feet.z-PLAYER_RADIUS < WORLD_MIN || feet.z+PLAYER_RADIUS > WORLD_MAX ||
        feet.y < -0.01f || feet.y+PLAYER_HEIGHT > WORLD_MAX) return true;

    BoundingBox p=PlayerBox(feet);
    int cx=ChunkOf(feet.x), cz=ChunkOf(feet.z);
    for (int layer=0; layer<GRID_N; layer++) {
        float fy=FloorY(layer);
        if (p.max.y <= fy || p.min.y >= fy+WALL_H) continue;
        for(int z=cz-1;z<=cz+1;z++) for(int x=cx-1;x<=cx+1;x++) {
            if (!InGrid(x) || !InGrid(z)) continue;
            float mx=x*CHUNK_SIZE, mz=z*CHUNK_SIZE;
            float west=mx-HALF_CHUNK, north=mz-HALF_CHUNK;
            BoundingBox walls[4]={
                {{west-WALL_T/2,fy,north},{west+WALL_T/2,fy+WALL_H,mz-DOOR_HALF}},
                {{west-WALL_T/2,fy,mz+DOOR_HALF},{west+WALL_T/2,fy+WALL_H,mz+HALF_CHUNK}},
                {{west,fy,north-WALL_T/2},{mx-DOOR_HALF,fy+WALL_H,north+WALL_T/2}},
                {{mx+DOOR_HALF,fy,north-WALL_T/2},{mx+HALF_CHUNK,fy+WALL_H,north+WALL_T/2}}
            };
            for(int i=0;i<4;i++) if(Overlaps(p,walls[i])) return true;
        }
    }
    return false;
}
static bool PlayerCollides(Vector3 feet)
{
    if (WallAt(feet)) return true;
    BoundingBox p=PlayerBox(feet);
    for(int i=0;i<nearbyCount;i++) if(Overlaps(p,Box(nearby[i]))) return true;
    return false;
}
static bool HasSupport(Vector3 p)
{
    // O piso de cada andar e solido, mesmo quando nao aparece a grade.
    float base=FloorY(LayerOf(p.y));
    if(fabsf(p.y-base)<0.012f) return true;
    Vector3 probe=p; probe.y-=0.055f;
    return PlayerCollides(probe);
}
// Movimento sub-step por eixo (X, Z, Y) evita atravessar paredes finas
// em FPS baixo, corrige quinas e detecta teto/chao de caixas no salto.
static void MovePlayerSolid(Vector3 *p, float dx, float dz, float *vy,
                            float dt, bool *grounded)
{
    float longest=MaxF(MaxF(fabsf(dx),fabsf(dz)),fabsf(*vy*dt)+GRAVITY*dt*dt);
    int count=(int)ceilf(longest/PHYSICS_STEP);
    if(count<1) count=1;
    if(count>128) count=128;
    float h=dt/(float)count;
    for(int i=0;i<count;i++) {
        Vector3 trial=*p; trial.x+=dx/count;
        if(!PlayerCollides(trial)) p->x=trial.x;
        trial=*p; trial.z+=dz/count;
        if(!PlayerCollides(trial)) p->z=trial.z;

        if(*grounded && !HasSupport(*p)) *grounded=false;
        if(!*grounded) {
            *vy-=GRAVITY*h;
            trial=*p; trial.y+=(*vy)*h;
            float base=FloorY(LayerOf(p->y));
            if(trial.y<=base && *vy<=0) {
                p->y=base; *vy=0; *grounded=true;
            } else if(PlayerCollides(trial)) {
                // O sub-step e pequeno; mantemos a ultima posicao valida.
                // Uma colisao na descida corresponde a pouso na caixa.
                if(*vy<0) *grounded=true;
                *vy=0;
            } else p->y=trial.y;
        }
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

// Sala do jogador: piso e paredes preenchidos, sem grade interna.
// Sem teto: as construcoes verticais e os pulos continuam acessiveis.
static void DrawFilledRoom(int cx, int cz, int layer)
{
    const float mx = cx * CHUNK_SIZE, mz = cz * CHUNK_SIZE;
    const float west = mx - HALF_CHUNK, north = mz - HALF_CHUNK;
    const float east = mx + HALF_CHUNK, south = mz + HALF_CHUNK;
    const float wallLength = HALF_CHUNK - DOOR_HALF;
    const float wallCenter = (HALF_CHUNK + DOOR_HALF) / 2.0f;
    Color base = RoomColor(cx, cz,layer);
    Color floorColor = Shade(base, 0.53f);
    Color wallColor = Shade(base, 0.88f);

    // Piso ligeiramente abaixo de y=0, evitando efeito z-fighting.
    DrawCube((Vector3){mx, FloorY(layer)-0.09f, mz}, CHUNK_SIZE, 0.16f, CHUNK_SIZE, floorColor);

    // Quatro paredes completas, cada uma com uma abertura central.
    // Os vizinhos compartilham a mesma geometria de colisao.
    DrawCube((Vector3){west, FloorY(layer)+WALL_H/2, mz-wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){west, FloorY(layer)+WALL_H/2, mz+wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){east, FloorY(layer)+WALL_H/2, mz-wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){east, FloorY(layer)+WALL_H/2, mz+wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){mx-wallCenter, FloorY(layer)+WALL_H/2, north}, wallLength, WALL_H, WALL_T, wallColor);
    DrawCube((Vector3){mx+wallCenter, FloorY(layer)+WALL_H/2, north}, wallLength, WALL_H, WALL_T, wallColor);
    DrawCube((Vector3){mx-wallCenter, FloorY(layer)+WALL_H/2, south}, wallLength, WALL_H, WALL_T, wallColor);
    DrawCube((Vector3){mx+wallCenter, FloorY(layer)+WALL_H/2, south}, wallLength, WALL_H, WALL_T, wallColor);
}

// Fora da sala atual, o mundo conserva o estilo aramado verde.
static void DrawChunkEnvironment(int cx, int cz, int layer, int activeCx, int activeCz, int activeLayer)
{
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=GRID_N) return;
    if (cx == activeCx && cz == activeCz && layer==activeLayer) {
        DrawFilledRoom(cx, cz,layer);
        return;
    }
    float mx=cx*CHUNK_SIZE, mz=cz*CHUNK_SIZE;
    float west=mx-HALF_CHUNK, north=mz-HALF_CHUNK;
    float fy=FloorY(layer);
    for (int i=0;i<=12;i++) {
        DrawLine3D((Vector3){west+i,fy,north},(Vector3){west+i,fy,north+CHUNK_SIZE},DIM);
        DrawLine3D((Vector3){west,fy,north+i},(Vector3){west+CHUNK_SIZE,fy,north+i},DIM);
    }
    float wallLength=HALF_CHUNK-DOOR_HALF;
    float wallCenter=(HALF_CHUNK+DOOR_HALF)/2;

    // O limite leste/sul da sala atual ja foi preenchido. Evita duplicacao
    // de suas paredes com linhas verdes desenhadas pelo chunk vizinho.
    if (!(cx == activeCx+1 && cz == activeCz && layer==activeLayer)) {
        DrawCubeWires((Vector3){west,FloorY(layer)+WALL_H/2,mz-wallCenter},WALL_T,WALL_H,wallLength,NEON);
        DrawCubeWires((Vector3){west,FloorY(layer)+WALL_H/2,mz+wallCenter},WALL_T,WALL_H,wallLength,NEON);
        DrawLine3D((Vector3){west,fy,mz-DOOR_HALF},(Vector3){west,fy+WALL_H+1.5f,mz-DOOR_HALF},DIM);
    }
    if (!(cx == activeCx && cz == activeCz+1 && layer==activeLayer)) {
        DrawCubeWires((Vector3){mx-wallCenter,FloorY(layer)+WALL_H/2,north},wallLength,WALL_H,WALL_T,NEON);
        DrawCubeWires((Vector3){mx+wallCenter,FloorY(layer)+WALL_H/2,north},wallLength,WALL_H,WALL_T,NEON);
        DrawLine3D((Vector3){mx-DOOR_HALF,fy,north},(Vector3){mx-DOOR_HALF,fy+WALL_H+1.5f,north},DIM);
    }
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
        RayCollision h=GetRayCollisionBox(ray,Box(nearby[i]));
        if (h.hit && h.distance>0.05f && h.distance<dist) {
            found=i; dist=h.distance; *bestHit=h;
        }
    }
    return found;
}
// Tenta encaixar o bloco numa face, no solo ou, em ultimo caso, adiante.
static Block SuggestPlacement(Ray ray, RayCollision hit, int hitIndex)
{
    Block b={0};
    b.size=(Vector3){BLOCK_W,BLOCK_H,BLOCK_W};
    b.placedIndex=-1;
    if (hitIndex>=0) {
        Block target=nearby[hitIndex];
        Vector3 n=hit.normal;
        b.pos=target.pos;
        b.pos.x += n.x*(target.size.x+b.size.x)/2.0f;
        b.pos.y += n.y*(target.size.y+b.size.y)/2.0f;
        b.pos.z += n.z*(target.size.z+b.size.z)/2.0f;
    } else if (ray.direction.y < -0.0001f) {
        float t = -ray.position.y/ray.direction.y;
        if (t > 0.0f && t <= REACH) {
            Vector3 p=Vector3Add(ray.position,Vector3Scale(ray.direction,t));
            b.pos=(Vector3){Snap(p.x),FloorY(currentLayer)+BLOCK_H/2,Snap(p.z)};
        } else {
            Vector3 p=Vector3Add(ray.position,Vector3Scale(ray.direction,3.0f));
            b.pos=(Vector3){Snap(p.x),FloorY(currentLayer)+BLOCK_H/2,Snap(p.z)};
        }
    } else {
        Vector3 p=Vector3Add(ray.position,Vector3Scale(ray.direction,3.0f));
        b.pos=(Vector3){Snap(p.x),FloorY(currentLayer)+BLOCK_H/2,Snap(p.z)};
    }
    b.pos.y=MaxF(FloorY(currentLayer)+BLOCK_H/2,b.pos.y);
    return b;
}
static bool CanPlace(Block b, Vector3 player)
{
    if(b.pos.x<WORLD_MIN+1 || b.pos.x>WORLD_MAX-1 || b.pos.z<WORLD_MIN+1 || b.pos.z>WORLD_MAX-1 || b.pos.y<0 || b.pos.y>WORLD_MAX-1) return false;
    BoundingBox dest=Box(b);
    if (Overlaps(dest,PlayerBox(player))) return false;
    for (int i=0;i<nearbyCount;i++) if (Overlaps(dest,Box(nearby[i]))) return false;
    // Nao colocar blocos dentro das divisorias do labirinto.
    int cx=ChunkOf(b.pos.x), cz=ChunkOf(b.pos.z);
    for (int z=cz-1;z<=cz+1;z++) for (int x=cx-1;x<=cx+1;x++) {
        float mx=x*CHUNK_SIZE,mz=z*CHUNK_SIZE;
        float west=mx-HALF_CHUNK,north=mz-HALF_CHUNK;
        float base=FloorY(currentLayer);
        BoundingBox walls[4]={
            {{west-WALL_T/2,base,north},{west+WALL_T/2,base+WALL_H,mz-DOOR_HALF}},
            {{west-WALL_T/2,base,mz+DOOR_HALF},{west+WALL_T/2,base+WALL_H,mz+HALF_CHUNK}},
            {{west,base,north-WALL_T/2},{mx-DOOR_HALF,base+WALL_H,north+WALL_T/2}},
            {{mx+DOOR_HALF,base,north-WALL_T/2},{mx+HALF_CHUNK,base+WALL_H,north+WALL_T/2}}
        };
        for(int i=0;i<4;i++) if(Overlaps(dest,walls[i])) return false;
    }
    return true;
}
static void ResetPlayer(Vector3 *player,float *vy,float *yaw,float *pitch,bool *grounded,bool *carrying)
{
    *player=(Vector3){0,0,0}; *vy=0;
    *yaw=0; *pitch=0; *grounded=true; *carrying=false;
}
// O mundo ocupa 96 x 96 x 96 unidades (8 salas por eixo).
// Os elevadores nos centros das salas ligam andares, sem exigir saltos impossiveis.
static bool AtLift(Vector3 p) {
    float x=p.x-ChunkOf(p.x)*CHUNK_SIZE;
    float z=p.z-ChunkOf(p.z)*CHUNK_SIZE;
    return fabsf(x)<1.7f && fabsf(z)<1.7f;
}
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
                    (Color){255,215,105,255});
        }
    }
}
static bool InteractKeys(Vector3 p) {
    for(int i=0;i<6;i++) {
        if(keysFound[i]) continue;
        Vector3 kp={keyRooms[i][0]*CHUNK_SIZE+2.0f,FloorY(keyRooms[i][1])+1.1f,keyRooms[i][2]*CHUNK_SIZE+2.0f};
        if(Vector3Distance(p,kp)<2.8f) {
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
    DrawCubeWires((Vector3){-2.0f,0.5f,-1.8f},1.0f,1.0f,1.0f,COR_BLOCO);
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
            DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,COR_BLOCO);
    }
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
static bool SaveGame(Vector3 p,float yaw,float pitch,float vy,bool carrying) {
    SaveData *s=calloc(1,sizeof *s);
    if(!s) return false;
    memcpy(s->magic,"VECT06",7); s->version=1; s->seed=worldSeed;
    s->player=p;s->yaw=yaw;s->pitch=pitch;s->vy=vy;
    s->removedCount=(uint32_t)removedCount;s->placedCount=(uint32_t)placedCount;
    memcpy(s->visited,visited,sizeof visited); memcpy(s->keysFound,keysFound,sizeof keysFound);
    memcpy(s->keysInserted,keysInserted,sizeof keysInserted);s->carrying=carrying?1:0;
    memcpy(s->removed,removed,(size_t)removedCount*sizeof(Removed));
    memcpy(s->placed,placed,(size_t)placedCount*sizeof(Placed));
    FILE *f=fopen(SAVE_FILE ".tmp","wb");
    bool ok=f && fwrite(s,sizeof *s,1,f)==1;
    if(f && fclose(f)!=0) ok=false;
    if(ok && rename(SAVE_FILE ".tmp",SAVE_FILE)!=0) ok=false;
    if(!ok) remove(SAVE_FILE ".tmp");
    free(s); return ok;
}
static bool LoadGame(Vector3 *p,float *yaw,float *pitch,float *vy,bool *carrying) {
    FILE *f=fopen(SAVE_FILE,"rb"); if(!f) return false;
    SaveData *s=malloc(sizeof *s); if(!s) {fclose(f);return false;}
    bool ok=fread(s,sizeof *s,1,f)==1; fclose(f);
    ok=ok && memcmp(s->magic,"VECT06",7)==0 && s->version==1 &&
       s->removedCount<=MAX_REMOVED && s->placedCount<=MAX_PLACED &&
       s->player.x>=WORLD_MIN && s->player.x<=WORLD_MAX &&
       s->player.z>=WORLD_MIN && s->player.z<=WORLD_MAX &&
       s->player.y>=0 && s->player.y<WORLD_MAX;
    if(ok) {
        worldSeed=s->seed; *p=s->player;*yaw=s->yaw;*pitch=s->pitch;*vy=s->vy;
        removedCount=(int)s->removedCount;placedCount=(int)s->placedCount;
        memcpy(removed,s->removed,(size_t)removedCount*sizeof(Removed));
        memcpy(placed,s->placed,(size_t)placedCount*sizeof(Placed));
        memcpy(visited,s->visited,sizeof visited);memcpy(keysFound,s->keysFound,sizeof keysFound);
        *carrying=s->carrying!=0;
        SetKeys(); // refaz os enderecos das chaves a partir da seed
        totalKeys=0;
        memcpy(keysFound,s->keysFound,sizeof keysFound);
        memcpy(keysInserted,s->keysInserted,sizeof keysInserted);
        for(int i=0;i<6;i++) if(keysFound[i]) totalKeys++;
    }
    free(s);return ok;
}
static void NewGame(uint32_t seed,Vector3 *p,float *vy,float *yaw,float *pitch,bool *carrying) {
    worldSeed=seed;ResetEdits();memset(visited,0,sizeof visited);SetKeys();
    ResetPlayer(p,vy,yaw,pitch,&(bool){true},carrying);
    currentLayer=mapLayer=0;mapOpen=false;MarkVisited(*p);
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
    InitWindow(SCREEN_W,SCREEN_H,"VECTOR SYSTEM // 0.7");
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
            DrawText("VECTOR SYSTEM 0.7",64,95,46,(Color){90,190,245,255});
            DrawText("AS SEIS CHAVES",70,162,25,GOLD);
            DrawText("1 / N - NOVO MUNDO",80,294,28,LIGHTGRAY);
            DrawText("2 / L - CARREGAR ULTIMO SAVE",80,354,25,LIGHTGRAY);
            DrawText("P - RECARREGAR PLAYER.OBJ",80,419,20,SKYBLUE);
            DrawText("96 x 96 x 96 | 8 camadas | 6 chaves",80,483,18,GRAY);
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
            Note(SaveGame(player,yaw,pitch,vy,carrying)?"Mundo salvo em simulador_save.dat":"Erro ao salvar. Confira permissoes de escrita.");
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
        if(AtLift(player)) {
            int dest=currentLayer;
            if(IsKeyPressed(KEY_PAGE_UP)) dest++;
            if(IsKeyPressed(KEY_PAGE_DOWN)) dest--;
            if(dest>=0 && dest<GRID_N && dest!=currentLayer) {
                player.y=FloorY(dest);vy=0;grounded=true;currentLayer=dest;
                Note("Elevador: camada alterada.");
            }
        }
        MarkVisited(player);
        if(IsKeyPressed(KEY_F)) InteractKeys(player);
        camera.position=(Vector3){player.x,player.y+EYE_H,player.z};
        camera.target=(Vector3){player.x+cosf(pitch)*sinf(yaw),player.y+EYE_H+sinf(pitch),player.z-cosf(pitch)*cosf(yaw)};
        RebuildNearby(player);
        Ray ray=AimRay(camera);RayCollision hit={0};int selected=RayBlock(ray,&hit);
        Block preview={0};bool canPlace=false;
        if(carrying) {preview=SuggestPlacement(ray,hit,selected);canPlace=CanPlace(preview,player);}
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!carrying && selected>=0) {
                if(RemoveBlock(nearby[selected])) carrying=true;
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
        for(int layer=ly-1;layer<=ly+1;layer++)
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
            DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,(!carrying&&selected==i)?HIGHLIGHT:COR_BLOCO);
        }
        if(carrying) {
            DrawCubeWires(preview.pos,preview.size.x,preview.size.y,preview.size.z,canPlace?COR_BLOCO:BAD);
            Vector3 hand=Vector3Add(ray.position,Vector3Scale(ray.direction,1.35f));
            DrawCubeWires(hand,0.25f,0.25f,0.25f,HIGHLIGHT);
        }
        if(AtLift(player)) {
            DrawHex((Vector3){cx*CHUNK_SIZE,FloorY(ly)+0.06f,cz*CHUNK_SIZE},1.4f,SKYBLUE);
            DrawLine3D((Vector3){cx*CHUNK_SIZE,FloorY(ly),cz*CHUNK_SIZE},
                (Vector3){cx*CHUNK_SIZE,FloorY(ly)+2.8f,cz*CHUNK_SIZE},SKYBLUE);
        }
        EndMode3D();
        DrawText("VECTOR 0.7 | WASD | ESPACO | SHIFT | CLICK BLOCO | F CHAVE",16,16,17,NEON);
        DrawText("DIR. MAPA | CTRL+S SAVE | ESC MENU | R NOVO | Q CAMERA",16,42,16,NEON);
        DrawText("ELEVADOR: centro da sala + PAGE UP / PAGE DOWN",16,67,16,SKYBLUE);
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
