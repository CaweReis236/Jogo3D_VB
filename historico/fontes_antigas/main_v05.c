// Simulador vetorial 0.5 - salas de cores suaves, blocos azuis e mundo procedural
// Raylib 6.x | C11
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define CHUNK_SIZE 12.0f
#define HALF_CHUNK 6.0f
#define RENDER_RADIUS 2
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
#define MAX_NEAR 1500
#define MAX_REMOVED 8192
#define MAX_PLACED 4096
#define REACH 6.5f

static uint32_t worldSeed = 0;
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
static uint32_t Hash(int cx, int cz, int slot)
{
    return Mix(worldSeed ^ (uint32_t)cx * 73856093u ^
               (uint32_t)cz * 19349663u ^ (uint32_t)(slot+1) * 83492791u);
}
static uint32_t NewSeed(void)
{
    return ((uint32_t)GetRandomValue(0,65535) << 16) |
            (uint32_t)GetRandomValue(0,65535);
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
static void GenerateChunk(int cx, int cz)
{
    for (int slot = 0; slot < 4; slot++) {
        uint32_t h = Hash(cx, cz, slot);
        int floors = 2 + (int)(h % 6u); // 2 a 7 blocos de altura
        float sx = (slot & 1) ? 3.25f : -3.25f;
        float sz = (slot & 2) ? 3.25f : -3.25f;
        float dirx = (h & 0x100u) ? 1.0f : -1.0f;
        float dirz = (h & 0x200u) ? 1.0f : -1.0f;
        bool stairsX = (h & 0x400u) != 0;
        float jitterX = (float)((h >> 16) & 7u) * 0.08f - 0.28f;
        float jitterZ = (float)((h >> 20) & 7u) * 0.08f - 0.28f;
        for (int level = 0; level < floors; level++) {
            Block b = {0};
            b.cx = cx; b.cz = cz; b.slot = slot; b.level = level;
            b.placedIndex = -1;
            b.size = (Vector3){BLOCK_W, BLOCK_H, BLOCK_W};
            float step = (float)level * 0.46f;
            b.pos.x = cx * CHUNK_SIZE + sx + jitterX + (stairsX ? dirx * step : dirx * step * 0.28f);
            b.pos.z = cz * CHUNK_SIZE + sz + jitterZ + (stairsX ? dirz * step * 0.28f : dirz * step);
            b.pos.y = (level + 0.5f) * BLOCK_H;
            if (!WasRemoved(b)) PushNearby(b);
        }
    }
}
// Recria geometria apenas perto do jogador; as alteracoes persistem separadas.
static void RebuildNearby(Vector3 player)
{
    nearbyCount = 0;
    int cx = ChunkOf(player.x), cz = ChunkOf(player.z);
    for (int z = cz - RENDER_RADIUS - 1; z <= cz + RENDER_RADIUS + 1; z++)
        for (int x = cx - RENDER_RADIUS - 1; x <= cx + RENDER_RADIUS + 1; x++)
            GenerateChunk(x,z);
    for (int i = 0; i < placedCount; i++) {
        if (placed[i].active &&
            abs(ChunkOf(placed[i].block.pos.x) - cx) <= RENDER_RADIUS + 1 &&
            abs(ChunkOf(placed[i].block.pos.z) - cz) <= RENDER_RADIUS + 1)
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
static bool WallAt(Vector3 feet)
{
    BoundingBox p = PlayerBox(feet);
    int cx = ChunkOf(feet.x), cz = ChunkOf(feet.z);
    for (int z = cz-1; z <= cz+1; z++) for (int x = cx-1; x <= cx+1; x++) {
        float mx=x*CHUNK_SIZE, mz=z*CHUNK_SIZE;
        float west=mx-HALF_CHUNK, north=mz-HALF_CHUNK;
        BoundingBox walls[4] = {
            {{west-WALL_T/2,0,north},{west+WALL_T/2,WALL_H,mz-DOOR_HALF}},
            {{west-WALL_T/2,0,mz+DOOR_HALF},{west+WALL_T/2,WALL_H,mz+HALF_CHUNK}},
            {{west,0,north-WALL_T/2},{mx-DOOR_HALF,WALL_H,north+WALL_T/2}},
            {{mx+DOOR_HALF,0,north-WALL_T/2},{mx+HALF_CHUNK,WALL_H,north+WALL_T/2}}
        };
        for (int i=0;i<4;i++) if (Overlaps(p,walls[i])) return true;
    }
    return false;
}
static bool PlayerCollides(Vector3 feet)
{
    if (WallAt(feet)) return true;
    BoundingBox p=PlayerBox(feet);
    for (int i=0;i<nearbyCount;i++)
        if (Overlaps(p,Box(nearby[i]))) return true;
    return false;
}
static bool HorizontalOverlap(Vector3 feet, BoundingBox b)
{
    return feet.x+PLAYER_RADIUS > b.min.x+0.01f && feet.x-PLAYER_RADIUS < b.max.x-0.01f &&
           feet.z+PLAYER_RADIUS > b.min.z+0.01f && feet.z-PLAYER_RADIUS < b.max.z-0.01f;
}
// Pouso sobre caixas e bloqueio de teto durante o salto.
static void VerticalMove(Vector3 *feet, float *vy, float dt, bool *grounded)
{
    float oldY = feet->y;
    float newY = oldY + *vy*dt;
    *grounded = false;
    if (*vy <= 0.0f) {
        float landing = (newY <= 0.0f && oldY >= -0.01f) ? 0.0f : -10000.0f;
        for (int i=0;i<nearbyCount;i++) {
            BoundingBox b=Box(nearby[i]);
            if (HorizontalOverlap(*feet,b) && oldY >= b.max.y-0.04f && newY <= b.max.y)
                landing=MaxF(landing,b.max.y);
        }
        if (landing > -9999.0f) {
            newY=landing; *vy=0; *grounded=true;
        }
    } else {
        float oldHead=oldY+PLAYER_HEIGHT;
        float newHead=newY+PLAYER_HEIGHT;
        for (int i=0;i<nearbyCount;i++) {
            BoundingBox b=Box(nearby[i]);
            if (HorizontalOverlap(*feet,b) && oldHead <= b.min.y+0.04f && newHead > b.min.y) {
                newY=MinF(newY,b.min.y-PLAYER_HEIGHT-0.01f);
                *vy=0;
            }
        }
    }
    feet->y=MaxF(0.0f,newY);
    if (feet->y == 0.0f && *vy <= 0) { *vy=0; *grounded=true; }
}
// Cada sala recebe uma cor suave e ESTAVEL, calculada a partir da seed.
// As diferencas entre canais RGB sao pequenas: cores pouco saturadas.
static Color RoomColor(int cx, int cz)
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
    uint32_t h = Hash(cx, cz, 57);
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
static void DrawFilledRoom(int cx, int cz)
{
    const float mx = cx * CHUNK_SIZE, mz = cz * CHUNK_SIZE;
    const float west = mx - HALF_CHUNK, north = mz - HALF_CHUNK;
    const float east = mx + HALF_CHUNK, south = mz + HALF_CHUNK;
    const float wallLength = HALF_CHUNK - DOOR_HALF;
    const float wallCenter = (HALF_CHUNK + DOOR_HALF) / 2.0f;
    Color base = RoomColor(cx, cz);
    Color floorColor = Shade(base, 0.53f);
    Color wallColor = Shade(base, 0.88f);

    // Piso ligeiramente abaixo de y=0, evitando efeito z-fighting.
    DrawCube((Vector3){mx, -0.09f, mz}, CHUNK_SIZE, 0.16f, CHUNK_SIZE, floorColor);

    // Quatro paredes completas, cada uma com uma abertura central.
    // Os vizinhos compartilham a mesma geometria de colisao.
    DrawCube((Vector3){west, WALL_H/2, mz-wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){west, WALL_H/2, mz+wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){east, WALL_H/2, mz-wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){east, WALL_H/2, mz+wallCenter}, WALL_T, WALL_H, wallLength, wallColor);
    DrawCube((Vector3){mx-wallCenter, WALL_H/2, north}, wallLength, WALL_H, WALL_T, wallColor);
    DrawCube((Vector3){mx+wallCenter, WALL_H/2, north}, wallLength, WALL_H, WALL_T, wallColor);
    DrawCube((Vector3){mx-wallCenter, WALL_H/2, south}, wallLength, WALL_H, WALL_T, wallColor);
    DrawCube((Vector3){mx+wallCenter, WALL_H/2, south}, wallLength, WALL_H, WALL_T, wallColor);
}

// Fora da sala atual, o mundo conserva o estilo aramado verde.
static void DrawChunkEnvironment(int cx, int cz, int activeCx, int activeCz)
{
    if (cx == activeCx && cz == activeCz) {
        DrawFilledRoom(cx, cz);
        return;
    }
    float mx=cx*CHUNK_SIZE, mz=cz*CHUNK_SIZE;
    float west=mx-HALF_CHUNK, north=mz-HALF_CHUNK;
    for (int i=0;i<=12;i++) {
        DrawLine3D((Vector3){west+i,0,north},(Vector3){west+i,0,north+CHUNK_SIZE},DIM);
        DrawLine3D((Vector3){west,0,north+i},(Vector3){west+CHUNK_SIZE,0,north+i},DIM);
    }
    float wallLength=HALF_CHUNK-DOOR_HALF;
    float wallCenter=(HALF_CHUNK+DOOR_HALF)/2;

    // O limite leste/sul da sala atual ja foi preenchido. Evita duplicacao
    // de suas paredes com linhas verdes desenhadas pelo chunk vizinho.
    if (!(cx == activeCx+1 && cz == activeCz)) {
        DrawCubeWires((Vector3){west,WALL_H/2,mz-wallCenter},WALL_T,WALL_H,wallLength,NEON);
        DrawCubeWires((Vector3){west,WALL_H/2,mz+wallCenter},WALL_T,WALL_H,wallLength,NEON);
        DrawLine3D((Vector3){west,0,mz-DOOR_HALF},(Vector3){west,WALL_H+1.5f,mz-DOOR_HALF},DIM);
    }
    if (!(cx == activeCx && cz == activeCz+1)) {
        DrawCubeWires((Vector3){mx-wallCenter,WALL_H/2,north},wallLength,WALL_H,WALL_T,NEON);
        DrawCubeWires((Vector3){mx+wallCenter,WALL_H/2,north},wallLength,WALL_H,WALL_T,NEON);
        DrawLine3D((Vector3){mx-DOOR_HALF,0,north},(Vector3){mx-DOOR_HALF,WALL_H+1.5f,north},DIM);
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
            b.pos=(Vector3){Snap(p.x),BLOCK_H/2,Snap(p.z)};
        } else {
            Vector3 p=Vector3Add(ray.position,Vector3Scale(ray.direction,3.0f));
            b.pos=(Vector3){Snap(p.x),BLOCK_H/2,Snap(p.z)};
        }
    } else {
        Vector3 p=Vector3Add(ray.position,Vector3Scale(ray.direction,3.0f));
        b.pos=(Vector3){Snap(p.x),BLOCK_H/2,Snap(p.z)};
    }
    b.pos.y=MaxF(BLOCK_H/2,b.pos.y);
    return b;
}
static bool CanPlace(Block b, Vector3 player)
{
    BoundingBox dest=Box(b);
    if (Overlaps(dest,PlayerBox(player))) return false;
    for (int i=0;i<nearbyCount;i++) if (Overlaps(dest,Box(nearby[i]))) return false;
    // Nao colocar blocos dentro das divisorias do labirinto.
    int cx=ChunkOf(b.pos.x), cz=ChunkOf(b.pos.z);
    for (int z=cz-1;z<=cz+1;z++) for (int x=cx-1;x<=cx+1;x++) {
        float mx=x*CHUNK_SIZE,mz=z*CHUNK_SIZE;
        float west=mx-HALF_CHUNK,north=mz-HALF_CHUNK;
        BoundingBox walls[4]={
            {{west-WALL_T/2,0,north},{west+WALL_T/2,WALL_H,mz-DOOR_HALF}},
            {{west-WALL_T/2,0,mz+DOOR_HALF},{west+WALL_T/2,WALL_H,mz+HALF_CHUNK}},
            {{west,0,north-WALL_T/2},{mx-DOOR_HALF,WALL_H,north+WALL_T/2}},
            {{mx+DOOR_HALF,0,north-WALL_T/2},{mx+HALF_CHUNK,WALL_H,north+WALL_T/2}}
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
int main(int argc, char **argv)
{
    if (argc>=2) {
        char *end=NULL; errno=0;
        unsigned long long number=strtoull(argv[1],&end,10);
        if(errno || end==argv[1] || *end || number>UINT32_MAX || argv[1][0]=='-') {
            fprintf(stderr,"Seed invalida: use 0 a 4294967295.\n"); return 1;
        }
        worldSeed=(uint32_t)number;
    }
    InitWindow(SCREEN_W,SCREEN_H,"VECTOR SYSTEM // PROCEDURAL 3D");
    if(argc<2) worldSeed=NewSeed();
    SetTargetFPS(60);
    DisableCursor();
    Vector3 player={0,0,0};
    float vy=0,yaw=0,pitch=0;
    bool grounded=true, carrying=false;
    Camera3D camera={0};
    camera.up=(Vector3){0,1,0}; camera.fovy=75; camera.projection=CAMERA_PERSPECTIVE;
    printf("Seed: %u\n",(unsigned)worldSeed);
    while(!WindowShouldClose()) {
        float dt=MinF(GetFrameTime(),1.0f/30.0f);
        if(IsKeyPressed(KEY_R)) {
            if(!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) {
                uint32_t old=worldSeed;
                do { worldSeed=NewSeed(); } while(old==worldSeed);
            }
            ResetEdits();
            ResetPlayer(&player,&vy,&yaw,&pitch,&grounded,&carrying);
            printf("Seed: %u\n",(unsigned)worldSeed);
        }
        Vector2 delta=GetMouseDelta();
        yaw+=delta.x*0.0025f;
        pitch=ClampF(pitch-delta.y*0.0025f,-1.45f,1.45f);
        RebuildNearby(player);
        float fw=(float)(IsKeyDown(KEY_W)-IsKeyDown(KEY_S));
        float side=(float)(IsKeyDown(KEY_D)-IsKeyDown(KEY_A));
        float dx=sinf(yaw)*fw+cosf(yaw)*side;
        float dz=-cosf(yaw)*fw+sinf(yaw)*side;
        float len=sqrtf(dx*dx+dz*dz);
        if(len>0) {dx/=len;dz/=len;}
        float speed=(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT))?7.5f:4.5f;
        Vector3 next=player;
        next.x+=dx*speed*dt;
        if(!PlayerCollides(next)) player.x=next.x;
        next=player; next.z+=dz*speed*dt;
        if(!PlayerCollides(next)) player.z=next.z;
        if(IsKeyPressed(KEY_SPACE) && grounded) {vy=JUMP_SPEED;grounded=false;}
        vy-=GRAVITY*dt;
        VerticalMove(&player,&vy,dt,&grounded);
        camera.position=(Vector3){player.x,player.y+EYE_H,player.z};
        camera.target=(Vector3){
            player.x+cosf(pitch)*sinf(yaw),
            player.y+EYE_H+sinf(pitch),
            player.z-cosf(pitch)*cosf(yaw)};
        RebuildNearby(player);
        Ray ray=AimRay(camera);
        RayCollision hit={0};
        int selected=RayBlock(ray,&hit);
        Block preview={0};
        bool canPlace=false;
        if(carrying) {
            preview=SuggestPlacement(ray,hit,selected);
            canPlace=CanPlace(preview,player);
        }
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!carrying && selected>=0) {
                if(RemoveBlock(nearby[selected])) carrying=true;
                RebuildNearby(player);
            } else if(carrying && canPlace) {
                if(AddPlaced(preview)) carrying=false;
                RebuildNearby(player);
            }
        }
        int cx=ChunkOf(player.x),cz=ChunkOf(player.z);
        BeginDrawing(); ClearBackground(BLACK); BeginMode3D(camera);
        for(int z=cz-RENDER_RADIUS;z<=cz+RENDER_RADIUS;z++)
            for(int x=cx-RENDER_RADIUS;x<=cx+RENDER_RADIUS;x++) DrawChunkEnvironment(x,z,cx,cz);
        for(int i=0;i<nearbyCount;i++) {
            Block b=nearby[i];
            if(abs(ChunkOf(b.pos.x)-cx)>RENDER_RADIUS || abs(ChunkOf(b.pos.z)-cz)>RENDER_RADIUS) continue;
            Color c=(!carrying && selected==i)?HIGHLIGHT:COR_BLOCO;
            DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,c);
        }
        if(carrying) {
            Color c=canPlace?COR_BLOCO:BAD;
            DrawCubeWires(preview.pos,preview.size.x,preview.size.y,preview.size.z,c);
            // Cubo suspenso na frente da camera = indicador de item na mao.
            Vector3 hand=Vector3Add(ray.position,Vector3Scale(ray.direction,1.35f));
            DrawCubeWires(hand,0.25f,0.25f,0.25f,HIGHLIGHT);
        }
        EndMode3D();
        DrawText("VECTOR SYSTEM // V0.5",20,20,20,NEON);
        DrawText("WASD andar | ESPACO pular | SHIFT correr | MOUSE olhar",20,49,16,NEON);
        DrawText("CLICK pegar / colocar | R nova seed | SHIFT+R mesma seed",20,73,16,NEON);
        DrawText(TextFormat("SEED %u | CHUNK (%d,%d) | ALTURA %.1f | FPS %d",
            (unsigned)worldSeed,cx,cz,player.y,GetFPS()),20,98,16,NEON);
        const char *status=carrying?(canPlace?"BLOCO NA MAO - CLIQUE PARA COLOCAR":"POSICAO BLOQUEADA - MIRE EM OUTRO LUGAR"):
                                   (selected>=0?"BLOCO NO ALVO - CLIQUE PARA PEGAR":"PROCURE UM BLOCO ATE 6.5 METROS");
        DrawText(status,20,124,16,carrying&&!canPlace?BAD:HIGHLIGHT);
        DrawText("SALA",SCREEN_W-126,24,14,NEON);
        DrawRectangle(SCREEN_W-72,20,44,24,RoomColor(cx,cz));
        DrawLine(SCREEN_W/2-6,SCREEN_H/2,SCREEN_W/2+6,SCREEN_H/2,carrying?HIGHLIGHT:NEON);
        DrawLine(SCREEN_W/2,SCREEN_H/2-6,SCREEN_W/2,SCREEN_H/2+6,carrying?HIGHLIGHT:NEON);
        EndDrawing();
    }
    CloseWindow(); return 0;
}
