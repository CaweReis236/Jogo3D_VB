// VECTOR SEARCH 1.4 - menu de modelos por pastas (assets/<nome>/player.obj)
// Raylib 6.x | C11
#define _POSIX_C_SOURCE 200809L
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
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define CHUNK_SIZE 10.0f
#define HALF_CHUNK 5.0f
#define RENDER_RADIUS 1
#define GRID_N 10
#define LEVEL_COUNT 6
#define GRID_MIN (-5)
#define GRID_MAX 4
#define LEVEL_H 6.0f
#define WORLD_MIN (-55.0f)
#define WORLD_MAX (45.0f)
#define SAVE_FILE "vector_search_v11_save.dat"
#define LEGACY_SAVE_FILE "simulador_save.dat"
#define WALL_H 5.84f
#define WALL_T 0.18f
#define DOOR_HALF 2.0f
#define PLAYER_RADIUS 0.30f
#define PLAYER_HEIGHT 1.65f
#define EYE_H 1.55f
#define GRAVITY 19.0f
#define JUMP_SPEED 7.4f
#define BLOCK_W 1.0f
#define BLOCK_H 1.0f
#define BLOCK_GRID 1.0f
#define MAX_NEAR 4096
#define AVATARS_ROOT "assets"
#define MODEL_CHOICE_FILE ".vector_search_modelo"
#define MAX_AVATARS 64
#define AVATAR_NAME_MAX 120
#define AVATAR_PATH_MAX 1024
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
#define STALKER_SPEED 3.15f
#define MAX_CRUMBS 640
#define BUFF_SECONDS 32.0f
#define CRUMB_RADIUS 0.20f
#define RANKING_FILE "vector_search_ranking.tsv"
#define CONFUSION_SECONDS 25.0f
#define VIRUSES_PER_FLOOR 3
#define VIRUS_TOTAL (LEVEL_COUNT*VIRUSES_PER_FLOOR)
#define CAPTURE_SECONDS 5.0f
#define BLOCK_BREAK_SECONDS 5.0f
#define VIDEO_W 640
#define VIDEO_H 360
#define VIDEO_FPS 24
#define VIDEO_PATH_CAP 2048

static uint32_t worldSeed = 0;
static float collectorEmbedded=0,stalkerEmbedded=0,disorienterEmbedded=0;
static unsigned char visited[GRID_N][LEVEL_COUNT][GRID_N];
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
// Cada virus tem seu proprio movimento, sala e contador de contenção.
typedef struct {
    Vector3 pos;
    int homeLayer,targetCx,targetCz;
    float containedFor, embeddedFor,rerouteTime;
    bool captured;
    uint32_t cycle;
} Virus;
static Virus viruses[VIRUS_TOTAL];
static int virusesCaptured=0;
// Virus segurado com o botao direito. Enquanto esta preso a mao, nao anda
// sozinho nem conta como aprisionado. Lanca no segundo clique direito.
static int grabbedVirus=-1;
static Vector3 virusThrowDir[VIRUS_TOTAL];
static float virusThrowTime[VIRUS_TOTAL];

static Vector3 hunterPos;
static int hunterLayer=0;
static float hunterCooldown=2.5f;
static float hunterSeal[6]={0};
static bool hunterEnabled=true;
static bool hunterSpawned=false;
// Novo predador: vermelho, espinhoso, so persegue o jogador.
static Vector3 stalkerPos;
static int stalkerLayer=0;
static bool stalkerSpawned=false;
static float stalkerGrace=4.0f;
static Vector3 breadcrumbs[MAX_CRUMBS];
static int breadcrumbCount=0;
static unsigned char buffClaimed[LEVEL_COUNT];
static float buffSeconds=0.0f;
// Novo estado da campanha. O ranking local fica fora do save da partida.
typedef struct { int mode,base,cx,cz,step; } EnemyRoute; // mode +1 sobe, -1 desce, 2/ -2 sai da escada
static EnemyRoute collectorRoute={0}, stalkerRoute={0};
static unsigned char floorCleared[LEVEL_COUNT]={0};
static float runSeconds=0.0f;
static bool floorIdentityLost=false, blueStored=false;
static int blueLayer=0,blueCx=0,blueCz=0;
static uint32_t blueCycle=0,teleportCycle=0;
static bool confused=false, disorienterActive=false;
static float confusionTime=0.0f,disorienterCooldown=0.0f;
static Vector3 disorienterPos={0};
static EnemyRoute disorienterRoute={0};
typedef struct { char name[24]; unsigned long long ms; uint32_t seed; } Score;
static Score localScores[100],onlineScores[100];
static int localScoreCount=0,onlineScoreCount=0;
static bool onlineAvailable=false;
static bool scoreSubmitted=false;
static char winnerName[24]="";
static int winnerNameLen=0;
// A gravacao usa OUTRO framebuffer e OUTRA camera. Nunca grava a HUD ou
// o ponteiro do mouse. O MP4 e codificado pelo ffmpeg atraves de stdin.
typedef struct {
    FILE *pipe;
    RenderTexture2D target;
    bool targetReady;
    double accumulator;
    uint64_t frames;
    char file[VIDEO_PATH_CAP];
} GameplayRecorder;
static GameplayRecorder recorder={0};
static unsigned int recordingSequence=0;

// Uma pasta por personagem: assets/<nome>/player.obj (ou outro .obj).
// MTL e texturas permanecem na MESMA pasta de seu OBJ.
typedef struct {
    char name[AVATAR_NAME_MAX];
    char path[AVATAR_PATH_MAX];
} AvatarChoice;
static AvatarChoice availableAvatars[MAX_AVATARS];
static int avatarCount=0, selectedAvatar=0;

static bool HasObjExtension(const char *filename) {
    const char *dot=strrchr(filename,'.');
    if(!dot || strlen(dot)!=4) return false;
    return (tolower((unsigned char)dot[1])=='o' &&
            tolower((unsigned char)dot[2])=='b' &&
            tolower((unsigned char)dot[3])=='j' && dot[4]==0);
}
static int AvatarCompare(const void *aa,const void *bb) {
    const AvatarChoice *a=aa,*b=bb;
    return strcmp(a->name,b->name);
}
// Seleciona player.obj se existir; caso contrario, o primeiro OBJ alfabetico.
static bool FindObjInFolder(const char *folder,char *out,size_t cap) {
    DIR *dir=opendir(folder);
    if(!dir) return false;
    char best[NAME_MAX+1]="";
    struct dirent *item;
    while((item=readdir(dir))!=NULL) {
        if(!HasObjExtension(item->d_name)) continue;
        char candidate[AVATAR_PATH_MAX];
        int n=snprintf(candidate,sizeof candidate,"%s/%s",folder,item->d_name);
        struct stat st;
        if(n<=0 || (size_t)n>=sizeof candidate || stat(candidate,&st)!=0 || !S_ISREG(st.st_mode))continue;
        if(strcmp(item->d_name,"player.obj")==0) {
            snprintf(best,sizeof best,"%s",item->d_name);
            break;
        }
        if(best[0]==0 || strcmp(item->d_name,best)<0)
            snprintf(best,sizeof best,"%s",item->d_name);
    }
    closedir(dir);
    if(!best[0])return false;
    int n=snprintf(out,cap,"%s/%s",folder,best);
    return n>0 && (size_t)n<cap;
}
static void FindAvatars(void) {
    avatarCount=0;
    DIR *root=opendir(AVATARS_ROOT);
    if(root) {
        struct dirent *item;
        while((item=readdir(root))!=NULL && avatarCount<MAX_AVATARS) {
            if(item->d_name[0]=='.')continue;
            char folder[AVATAR_PATH_MAX];
            int n=snprintf(folder,sizeof folder,"%s/%s",AVATARS_ROOT,item->d_name);
            if(n<=0 || (size_t)n>=sizeof folder)continue;
            struct stat st;
            if(stat(folder,&st)!=0 || !S_ISDIR(st.st_mode))continue;
            AvatarChoice *choice=&availableAvatars[avatarCount];
            if(!FindObjInFolder(folder,choice->path,sizeof choice->path))continue;
            snprintf(choice->name,sizeof choice->name,"%.*s",AVATAR_NAME_MAX-1,item->d_name);
            avatarCount++;
        }
        closedir(root);
    }
    // Compatibilidade com uma instalacao antiga, anterior a subpastas.
    if(avatarCount==0 && FileExists("assets/player.obj")) {
        snprintf(availableAvatars[0].name,AVATAR_NAME_MAX,"Legado");
        snprintf(availableAvatars[0].path,AVATAR_PATH_MAX,"assets/player.obj");
        avatarCount=1;
    }
    qsort(availableAvatars,(size_t)avatarCount,sizeof availableAvatars[0],AvatarCompare);
    selectedAvatar=0;
}
static const char *SelectedAvatarPath(void) {
    return avatarCount>0?availableAvatars[selectedAvatar].path:NULL;
}
static const char *SelectedAvatarName(void) {
    return avatarCount>0?availableAvatars[selectedAvatar].name:"Boneco reserva";
}
static void StoreAvatarChoice(void) {
    if(avatarCount==0)return;
    FILE *f=fopen(MODEL_CHOICE_FILE,"w");
    if(f) {fprintf(f,"%s\n",SelectedAvatarName());fclose(f);}
}
static void RestoreAvatarChoice(void) {
    FILE *f=fopen(MODEL_CHOICE_FILE,"r");
    if(!f)return;
    char saved[AVATAR_NAME_MAX]={0};
    if(fgets(saved,sizeof saved,f)) {
        saved[strcspn(saved,"\r\n")]=0;
        for(int i=0;i<avatarCount;i++)
            if(strcmp(saved,availableAvatars[i].name)==0) {selectedAvatar=i;break;}
    }
    fclose(f);
}



static float MinF(float a, float b) { return a < b ? a : b; }
static float MaxF(float a, float b) { return a > b ? a : b; }
static float ClampF(float x, float a, float b) { return MaxF(a, MinF(x,b)); }
// Centro da celula de 1 m em coordenadas globais; o limite nunca fica
// a meio caminho entre duas posicoes permitidas.
static float CellCenter(float x) { return floorf(x/BLOCK_GRID)*BLOCK_GRID+0.5f*BLOCK_GRID; }
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
// Os degraus formam a mesma escada usada pelo jogador e pelos adversarios.
static Vector3 StairPoint(int cx,int cz,int base,int step) {
    static const Vector2 path[8]={
        {2.42f,-0.45f},{2.27f,1.17f},{0.95f,2.29f},{-0.64f,2.32f},
        {-1.72f,1.40f},{-1.70f,0.18f},{-1.35f,-1.06f},{0.10f,-1.57f}
    };
    int i=step<0?0:(step>7?7:step);
    return (Vector3){cx*CHUNK_SIZE+path[i].x,FloorY(base)+0.75f*(i+1),cz*CHUNK_SIZE+path[i].y};
}
static void SpawnBlue(void) {
    uint32_t h=Hash3(blueCycle+11,311,29,1240);
    blueLayer=(int)(h%LEVEL_COUNT);
    blueCx=GRID_MIN+(int)((h>>8)%GRID_N);
    blueCz=GRID_MIN+(int)((h>>16)%GRID_N);
}
static Vector3 BluePosition(void) {
    return (Vector3){blueCx*CHUNK_SIZE-3.6f,FloorY(blueLayer)+0.15f,blueCz*CHUNK_SIZE+3.5f};
}
static void ResetNewFeatures(void) {
    memset(floorCleared,0,sizeof floorCleared);
    runSeconds=0;floorIdentityLost=false;blueStored=false;blueCycle=0;teleportCycle=0;
    confused=false;confusionTime=0;disorienterActive=false;disorienterCooldown=12.0f;
    collectorRoute=(EnemyRoute){0};stalkerRoute=(EnemyRoute){0};disorienterRoute=(EnemyRoute){0};
    scoreSubmitted=false;winnerNameLen=0;winnerName[0]=0;
    SpawnBlue();
}
static bool LevelCompleted(int level) {
    return level>=0 && level<LEVEL_COUNT && floorCleared[level]!=0;
}
static void Note(const char *s) { snprintf(message,sizeof message,"%s",s); messageTime=3.5f; }
// Seis chaves: seis andares distintos, escolhidos pela seed.
// Exatamente uma chave por andar, nos 6 andares (0..5).
static void SetKeys(void) {
    memset(keysFound,0,sizeof keysFound);
    memset(keysInserted,0,sizeof keysInserted);
    totalKeys=0;
    for(int k=0;k<6;k++) {
        int x=0,z=0;
        for(int a=0;a<300;a++) {
            uint32_t h=Hash3(k+29,k,a,63);
            x=GRID_MIN+(int)(h%GRID_N);
            z=GRID_MIN+(int)((h>>11)%GRID_N);
            if(!(x==0&&z==0&&k==0) && abs(x)+abs(z)>=2)break;
        }
        keyRooms[k][0]=x;keyRooms[k][1]=k;keyRooms[k][2]=z;
    }
}
// Cada andar possui exatamente um buff, em um chunk definido pela seed.
static void BuffRoom(int layer,int *cx,int *cz) {
    uint32_t h=Hash3(113,layer,17,901);
    *cx=GRID_MIN+(int)(h%GRID_N);
    *cz=GRID_MIN+(int)((h>>13)%GRID_N);
}
static Vector3 BuffPosition(int layer) {
    int cx,cz;BuffRoom(layer,&cx,&cz);
    return (Vector3){cx*CHUNK_SIZE-3.5f,FloorY(layer)+0.15f,cz*CHUNK_SIZE-3.5f};
}
static int CarriedKeyCount(void) {
    int c=0;for(int k=0;k<6;k++)if(keysFound[k]&&!keysInserted[k])c++;
    return c;
}
static int InsertedKeyCount(void) {
    int c=0;for(int k=0;k<6;k++)if(keysInserted[k])c++;
    return c;
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
    if(layer>=LEVEL_COUNT-1) return;
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
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=LEVEL_COUNT) return;
    // Arquitetura fixa do acesso vertical.
    GenerateStairs(cx,cz,layer);
    // Blocos soltos para construir barreiras contra o Coletor.
    // Mantemos o poco e os corredores principais desobstruidos.
    if(cx==0 && cz==0 && layer==0) return;
    for(int slot=0;slot<9;slot++) {
        Block b={0};b.cx=cx;b.cz=cz;b.slot=slot;b.level=layer*16+slot;
        b.placedIndex=-1;
        // Caixas distribuidas nas celulas inteiras do chunk: 1m x 1m x 1m.
        // Ficam nos quadrantes interiores, sem impedir portas ou a escada.
        // Canteiros distintos: ate nove caixas sem empilhamentos acidentais.
        static const Vector2 cells[16] = {
            {-3.5f,-3.5f},{-2.5f,-3.5f},{ 2.5f,-3.5f},{ 3.5f,-3.5f},
            {-3.5f,-2.5f},{-2.5f,-2.5f},{ 2.5f,-2.5f},{ 3.5f,-2.5f},
            {-3.5f, 2.5f},{-2.5f, 2.5f},{ 2.5f, 2.5f},{ 3.5f, 2.5f},
            {-3.5f, 3.5f},{-2.5f, 3.5f},{ 2.5f, 3.5f},{ 3.5f, 3.5f}
        };
        int cell=(slot*5+(int)(Hash3(cx,layer,cz,1337)%16u))%16;
        float sx=cells[cell].x,sz=cells[cell].y;
        b.size=(Vector3){BLOCK_W,BLOCK_H,BLOCK_W};
        b.pos=(Vector3){cx*CHUNK_SIZE+sx,FloorY(layer)+0.5f,
                          cz*CHUNK_SIZE+sz};
        // Nunca enterre um bonus laranja debaixo de uma caixa gerada.
        int bx,bz;BuffRoom(layer,&bx,&bz);
        if(cx==bx && cz==bz && fabsf(sx+3.5f)<1.35f && fabsf(sz+3.5f)<1.35f)continue;
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
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=LEVEL_COUNT) return 0;
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
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=LEVEL_COUNT)return 0;
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
    // Tampa do oitavo andar: mesmo volume desenhado por DrawCeiling.
    BoundingBox topRoof={{WORLD_MIN,LEVEL_COUNT*LEVEL_H-0.255f,WORLD_MIN},
                         {WORLD_MAX,LEVEL_COUNT*LEVEL_H-0.005f,WORLD_MAX}};
    if(Overlaps(body,topRoof))return true;
    int cx=ChunkOf((body.min.x+body.max.x)/2);
    int cz=ChunkOf((body.min.z+body.max.z)/2);
    for(int l=0;l<LEVEL_COUNT;l++) {
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
       body.min.y < -0.1f || body.max.y > LEVEL_COUNT*LEVEL_H-0.01f) return true;
    int cx=ChunkOf((body.min.x+body.max.x)/2),cz=ChunkOf((body.min.z+body.max.z)/2);
    for(int l=0;l<LEVEL_COUNT;l++) {
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
    if(layer<0 || layer>=LEVEL_COUNT) return false;
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
    for(int l=0;l<LEVEL_COUNT;l++) {
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
            for(int l=0;l<LEVEL_COUNT;l++) {
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
            for(int l=1;l<LEVEL_COUNT;l++) {
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

// -----------------------------------------------------------------------------
// CONTENCAO: apenas bloqueios realmente posicionados pelo jogador contam.
// Portas sao testadas na largura util para o raio do virus. Bordas do mundo
// ja sao seladas. Se TODAS as saidas estiverem obstruidas por 5 s, o virus
// e capturado definitivamente; bloqueios parciais nao contam.
// -----------------------------------------------------------------------------
static bool DoorSealed(int cx,int cz,int layer,int dir){
    int nx=cx+(dir==0?1:dir==1?-1:0);
    int nz=cz+(dir==2?1:dir==3?-1:0);
    if(!InGrid(nx)||!InGrid(nz))return true;
    float plane=(dir<2)?(cx*CHUNK_SIZE+(dir==0?HALF_CHUNK:-HALF_CHUNK)):
                         (cz*CHUNK_SIZE+(dir==2?HALF_CHUNK:-HALF_CHUNK));
    float across=(dir<2)?cz*CHUNK_SIZE:cx*CHUNK_SIZE;
    for(float u=-DOOR_HALF+HUNTER_RADIUS;u<=DOOR_HALF-HUNTER_RADIUS+0.001f;u+=0.12f){
        bool blocked=false;
        for(int i=0;i<placedCount;i++)if(placed[i].active){
            BoundingBox bb=Box(placed[i].block);
            if(bb.min.y>FloorY(layer)+HUNTER_H*0.65f || bb.max.y<FloorY(layer)+0.22f)continue;
            if(dir<2){
                if(bb.min.x<=plane+HUNTER_RADIUS && bb.max.x>=plane-HUNTER_RADIUS &&
                   bb.min.z<=across+u+HUNTER_RADIUS && bb.max.z>=across+u-HUNTER_RADIUS)blocked=true;
            }else{
                if(bb.min.z<=plane+HUNTER_RADIUS && bb.max.z>=plane-HUNTER_RADIUS &&
                   bb.min.x<=across+u+HUNTER_RADIUS && bb.max.x>=across+u-HUNTER_RADIUS)blocked=true;
            }
            if(blocked)break;
        }
        if(!blocked)return false;
    }
    return true;
}
static bool RoomSealed(int cx,int cz,int level){
    for(int d=0;d<4;d++)if(!DoorSealed(cx,cz,level,d))return false;
    return true;
}
// Se uma caixa surgiu DENTRO de um inimigo, ela se quebra depois de 5 s.
// Isso vale para todos os tipos. Nao quebra paredes nem a escada fixa.
static void EnemyEmbedded(Vector3 pos,float *timer,float dt){
    RebuildNearby(pos);
    BoundingBox body={{pos.x-HUNTER_RADIUS,pos.y+0.10f,pos.z-HUNTER_RADIUS},
                      {pos.x+HUNTER_RADIUS,pos.y+HUNTER_H,pos.z+HUNTER_RADIUS}};
    int index=-1;
    for(int i=0;i<nearbyCount;i++){
        if(nearby[i].slot>=100&&nearby[i].placedIndex<0)continue;
        if(Overlaps(body,Box(nearby[i]))){index=i;break;}
    }
    if(index<0){*timer=0;return;}
    *timer+=dt;
    if(*timer>=BLOCK_BREAK_SECONDS){
        if(RemoveBlock(nearby[index]))Note("Uma caixa que aprisionava um inimigo se quebrou!");
        *timer=0;
    }
}
static void SetViruses(void){
    virusesCaptured=0;
    for(int layer=0;layer<LEVEL_COUNT;layer++)for(int n=0;n<VIRUSES_PER_FLOOR;n++){
        int id=layer*VIRUSES_PER_FLOOR+n;
        Virus *v=&viruses[id];*v=(Virus){0};v->homeLayer=layer;
        int cx=0,cz=0;
        for(int t=0;t<256;t++){
            uint32_t h=Hash3(id+91,t,layer,1660);
            cx=GRID_MIN+(int)(h%GRID_N);cz=GRID_MIN+(int)((h>>11)%GRID_N);
            bool conflict=cx==keyRooms[layer][0]&&cz==keyRooms[layer][2];
            for(int j=layer*VIRUSES_PER_FLOOR;j<id;j++)
                if(ChunkOf(viruses[j].pos.x)==cx && ChunkOf(viruses[j].pos.z)==cz)conflict=true;
            if(!conflict && !(cx==0&&cz==0))break;
        }
        v->pos=(Vector3){cx*CHUNK_SIZE,FloorY(layer),cz*CHUNK_SIZE};
        v->targetCx=cx;v->targetCz=cz;
        v->rerouteTime=1.5f+(float)(Hash3(id,71,3,144)%12)*0.15f;
    }
}
// Movimento nao-perseguidor: os virus procuram portas livres.
static Vector3 HostileWaypoint(Vector3 from,Vector3 target);
static bool StepHostile(Vector3 *pos,Vector3 goal,float movement);
static void UpdateViruses(float dt,Vector3 player){
    for(int i=0;i<VIRUS_TOTAL;i++){
        Virus *v=&viruses[i];
        if(v->captured || i==grabbedVirus)continue;
        if(virusThrowTime[i]>0){
            virusThrowTime[i]=MaxF(0,virusThrowTime[i]-dt);
            Vector3 dest=Vector3Add(v->pos,Vector3Scale(virusThrowDir[i],6.8f*dt));
            RebuildNearby(v->pos);
            if(!StepHostile(&v->pos,dest,6.8f*dt))virusThrowTime[i]=0;
            // Para o virus nao atravessar a parede nem ganhar Y absurdo.
            v->pos.y=FloorY(v->homeLayer);
            if(virusThrowTime[i]>0)continue;
        }
        int cx=ChunkOf(v->pos.x),cz=ChunkOf(v->pos.z),ly=v->homeLayer;
        if(!InGrid(cx)||!InGrid(cz))continue;
        if(RoomSealed(cx,cz,ly)){
            v->containedFor+=dt;
            if(v->containedFor>=CAPTURE_SECONDS){
                v->captured=true;virusesCaptured++;
                Note("VIRUS CONTIDO! Bloqueio mantido por 5 segundos.");
            }
        } else v->containedFor=0;
        if(v->captured)continue;
        EnemyEmbedded(v->pos,&v->embeddedFor,dt);
        v->rerouteTime-=dt;
        if(v->rerouteTime<=0 || (cx==v->targetCx&&cz==v->targetCz)){
            v->cycle++;
            int order=(int)(Hash3(i,(int)v->cycle,ly,1821)%4u);
            v->targetCx=cx;v->targetCz=cz;
            for(int d=0;d<4;d++){
                int dir=(order+d)%4;
                int nx=cx+(dir==0?1:dir==1?-1:0);
                int nz=cz+(dir==2?1:dir==3?-1:0);
                if(!DoorSealed(cx,cz,ly,dir)&&InGrid(nx)&&InGrid(nz)){
                    v->targetCx=nx;v->targetCz=nz;break;
                }
            }
            v->rerouteTime=16.0f;
        }
        Vector3 target={v->targetCx*CHUNK_SIZE,FloorY(ly),v->targetCz*CHUNK_SIZE};
        Vector3 waypoint=HostileWaypoint(v->pos,target);
        waypoint.y=FloorY(ly);
        RebuildNearby(v->pos);
        StepHostile(&v->pos,waypoint,0.78f*dt);
    }
    RebuildNearby(player);
}
static void DrawViruses(int cx,int cz,int layer){
    for(int i=0;i<VIRUS_TOTAL;i++){
        Virus v=viruses[i];if(v.captured||v.homeLayer!=layer||
            ChunkOf(v.pos.x)!=cx||ChunkOf(v.pos.z)!=cz)continue;
        Vector3 c={v.pos.x,v.pos.y+0.8f,v.pos.z};
        Color green={45,242,102,255};
        DrawSphere(c,0.42f,green);
        for(int j=0;j<7;j++){
            float a=(float)j*2*PI/7;
            Vector3 tip={c.x+0.83f*cosf(a),c.y+0.3f*((j%3)-1),c.z+0.83f*sinf(a)};
            DrawCylinderEx(c,tip,0.20f,0,3,(Color){18,169,65,255});
        }
        if(v.containedFor>0){
            DrawCircle3D((Vector3){c.x,v.pos.y+0.04f,c.z},0.65f,
                (Vector3){1,0,0},90,(Color){248,180,58,255});
        }
    }
}

// Cada sala recebe uma cor suave e ESTAVEL, calculada a partir da seed.
// As diferencas entre canais RGB sao pequenas: cores pouco saturadas.
static Color RoomColor(int cx, int cz, int layer)
{
    if(LevelCompleted(layer)) return (Color){108,108,112,255};
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
// O teto e o mesmo volume que forma o piso superior. O vao vertical
// permanece aberto, e o ultimo andar recebe uma tampa solida.
static void DrawCeiling(int cx,int cz,int layer,Color color,bool wire) {
    if(layer<LEVEL_COUNT-1) {
        DrawFloor(cx,cz,layer+1,color,wire);
    } else {
        float x=cx*CHUNK_SIZE,z=cz*CHUNK_SIZE,y=LEVEL_COUNT*LEVEL_H-0.13f;
        if(wire) DrawCubeWires((Vector3){x,y,z},CHUNK_SIZE,0.25f,CHUNK_SIZE,color);
        else DrawCube((Vector3){x,y,z},CHUNK_SIZE,0.25f,CHUNK_SIZE,color);
    }
}
static void DrawWallBox(BoundingBox b, Color c,bool wire) {
    Vector3 ctr=Vector3Scale(Vector3Add(b.min,b.max),0.5f);
    Vector3 size=Vector3Subtract(b.max,b.min);
    if(wire) DrawCubeWires(ctr,size.x,size.y,size.z,c);
    else DrawCube(ctr,size.x,size.y,size.z,c);
}
static void DrawFilledRoom(int cx,int cz,int layer,bool roof) {
    Color base=RoomColor(cx,cz,layer);
    DrawFloor(cx,cz,layer,Shade(base,0.53f),false);
    if(roof) DrawCeiling(cx,cz,layer,Shade(base,0.66f),false);
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
    if(!InGrid(cx)||!InGrid(cz)||layer<0||layer>=LEVEL_COUNT)return;
    if(cx==acx && cz==acz && layer==aly){DrawFilledRoom(cx,cz,layer,true);return;}
    DrawFloor(cx,cz,layer,DIM,true);
    DrawCeiling(cx,cz,layer,DIM,true);
    float ox=cx*CHUNK_SIZE, oz=cz*CHUNK_SIZE,fy=FloorY(layer)+0.01f;
    // Grade retro realmente desenhada sobre as salas laterais.
    // As linhas sao interrompidas sobre o buraco central dos andares.
    for(int i=-5;i<=5;i++) {
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
    Color wallLine=LevelCompleted(layer)?(Color){143,143,148,255}:NEON;
    for(int i=0;i<n;i++) DrawWallBox(walls[i],wallLine,true);
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
       dest.min.y<-0.001f ||dest.max.y>LEVEL_COUNT*LEVEL_H-0.1f) return false;
    if(Overlaps(dest,PlayerBox(player)) || WallsHit(dest) || FloorHit(dest)) return false;
    for(int i=0;i<nearbyCount;i++)if(Overlaps(dest,Box(nearby[i])))return false;
    // impedir criar um bloco que ocupa o piso macico de qualquer andar
    for(int l=1;l<LEVEL_COUNT;l++) {
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
// Alvo exato: face do bloco, piso ou parede realmente apontados.
// Sem o antigo anel de busca que colocava caixas em lugares inesperados.
static bool SuggestPlacement(Ray ray,RayCollision hit,int hitIndex,Vector3 player,Block *result) {
    // Raycast de todas as superficies: bloco, paredes, pisos e teto.
    // A face determina uma unica celula ADJACENTE. Nao tentamos offsets
    // invisiveis; o cursor sempre significa a posicao desenhada na previa.
    RayCollision chosen={0};
    float distance=REACH;
    bool found=false;
    if(hitIndex>=0 && hit.hit && hit.distance>0.05f && hit.distance<distance){
        chosen=hit;distance=hit.distance;found=true;
    }
    int px=ChunkOf(player.x),pz=ChunkOf(player.z),ly=LayerOf(player.y);
    for(int layer=ly-1;layer<=ly+1;layer++)
    for(int x=px-1;x<=px+1;x++)for(int z=pz-1;z<=pz+1;z++) {
        BoundingBox floors[4],walls[8];
        int nf=FloorParts(x,z,layer,floors);
        int nw=WallsFor(x,z,layer,walls);
        for(int k=0;k<nf+nw;k++) {
            RayCollision h=GetRayCollisionBox(ray,k<nf?floors[k]:walls[k-nf]);
            if(h.hit && h.distance>0.05f && h.distance<distance){
                chosen=h;distance=h.distance;found=true;
            }
        }
    }
    if(!found){*result=(Block){0};return false;}
    // Raycast ultrapassa a face apontada apenas 1/2 celula: sempre
    // fica na celula ao lado, como o bloco de construcao do Minecraft.
    const float d=0.502f;
    Vector3 p={
        CellCenter(chosen.point.x+chosen.normal.x*d),
        CellCenter(chosen.point.y+chosen.normal.y*d),
        CellCenter(chosen.point.z+chosen.normal.z*d)
    };
    Block b=NewLooseBlock(p);
    // Precisa obedecer ao tamanho padrao, independentemente do bloco tirado.
    b.size=(Vector3){1,1,1};
    b.level=LayerOf(p.y)*16;
    *result=b; // mesmo se invalido, mostrar a celula em vermelho.
    return CanPlace(b,player);
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
    // O deposito tem prioridade: inserir antes de buscar nova chave.
    if(Vector3Distance(p,(Vector3){0,0,0})<3.0f) {
        int before=InsertedKeyCount();
        for(int k=0;k<6;k++) if(keysFound[k]&&!keysInserted[k]) keysInserted[k]=1;
        int after=InsertedKeyCount();
        if(after==6) Note("HEXAGONO COMPLETO! VITORIA! SEIS TRIANGULOS ENCAIXADOS.");
        else if(after>before && after==6) Note("Seis chaves encaixadas! Falta conter os 18 virus.");
        else if(after>before) Note("Chaves depositadas! Inventario liberado (maximo 2).");
        else Note("Hexagono vazio. Procure os triangulos espalhados pelos andares.");
        return true;
    }
    for(int i=0;i<6;i++) {
        if(keysFound[i]) continue;
        Vector3 kp={keyRooms[i][0]*CHUNK_SIZE+2.0f,FloorY(keyRooms[i][1])+1.1f,keyRooms[i][2]*CHUNK_SIZE+2.0f};
        if(Vector3Distance(p,kp)<2.8f) {
            if(hunterSeal[i]>0.0f) {Note("Coletor lacrou a chave. Espere o lacre terminar.");return true;}
            if(CarriedKeyCount()>=2) {Note("Mochila cheia (2 chaves). Volte ao hexagono (seta vermelha).");return true;}
            keysFound[i]=1;totalKeys++;floorCleared[keyRooms[i][1]]=1;
            Note("TRIANGULO COLETADO! Capacidade: 2. Deposito no hexagono central.");
            return true;
        }
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
static Vector3 HostileWaypoint(Vector3 from,Vector3 target);
// Anda por segmentos continuos, validando barricadas e paredes. Sem saltar de Y.
static bool StepHostile(Vector3 *pos,Vector3 goal,float movement) {
    Vector3 d=Vector3Subtract(goal,*pos);
    float len=Vector3Length(d);
    if(len<0.08f)return true;
    int n=(int)ceilf(movement/PHYSICS_STEP);
    if(n<1)n=1;
    if(n>32)n=32;
    float portion=MinF(movement,len)/(float)n;
    for(int i=0;i<n;i++) {
        Vector3 diff=Vector3Subtract(goal,*pos);
        float mag=Vector3Length(diff);
        if(mag<0.08f)break;
        Vector3 next=Vector3Add(*pos,Vector3Scale(diff,MinF(portion,mag)/mag));
        // Movimento para degraus permite atravessar a superficie dos blocos
        // FIXOS da escada; HunterCollides impede as caixas moviveis.
        if(!HunterCollides(next)) *pos=next;
        else {
            Vector3 trial=*pos;trial.x=next.x;
            if(!HunterCollides(trial))pos->x=trial.x;
            trial=*pos;trial.z=next.z;
            if(!HunterCollides(trial))pos->z=trial.z;
            trial=*pos;trial.y=next.y;
            if(!HunterCollides(trial))pos->y=trial.y;
            break;
        }
    }
    return Vector3Distance(*pos,goal)<0.09f;
}
// Clique direito: agarra o virus mirado. Um segundo clique lanca o virus
// para uma celula proxima, verificando paredes e caixas entre origem/destino.
static int VirusInSight(Ray ray,Vector3 player){
    int best=-1;float bestDistance=REACH;
    for(int i=0;i<VIRUS_TOTAL;i++){
        if(viruses[i].captured)continue;
        Vector3 center=Vector3Add(viruses[i].pos,(Vector3){0,0.8f,0});
        Vector3 v=Vector3Subtract(center,ray.position);
        float along=Vector3DotProduct(v,ray.direction);
        if(along<0.4f || along>REACH || along>=bestDistance)continue;
        Vector3 closest=Vector3Add(ray.position,Vector3Scale(ray.direction,along));
        if(Vector3Distance(center,closest)>0.83f)continue;
        if(abs(LayerOf(player.y)-viruses[i].homeLayer)>1)continue;
        // Nao enxergar atraves das paredes nem por tras dos blocos.
        bool blocked=false;
        int steps=(int)ceilf(along/0.22f);
        for(int k=1;k<steps;k++){
            Vector3 pt=Vector3Add(ray.position,Vector3Scale(ray.direction,along*(float)k/(float)steps));
            BoundingBox tiny={{pt.x-0.025f,pt.y-0.025f,pt.z-0.025f},
                              {pt.x+0.025f,pt.y+0.025f,pt.z+0.025f}};
            if(WallsHit(tiny)||FloorHit(tiny)){blocked=true;break;}
            for(int j=0;j<nearbyCount;j++){
                if(Overlaps(tiny,Box(nearby[j]))){blocked=true;break;}
            }
            if(blocked)break;
        }
        if(!blocked){best=i;bestDistance=along;}
    }
    return best;
}
static void CarryVirus(Vector3 player,float yaw){
    if(grabbedVirus<0)return;
    int ly=LayerOf(player.y);
    if(ly<0)ly=0;
    if(ly>=LEVEL_COUNT)ly=LEVEL_COUNT-1;
    Virus *v=&viruses[grabbedVirus];
    v->homeLayer=ly;
    // Renderiza perto do jogador para ficar visivel quando a camera filma.
    v->pos=(Vector3){player.x+sinf(yaw)*1.40f,FloorY(ly),player.z-cosf(yaw)*1.40f};
    v->containedFor=0;v->rerouteTime=0;
}
static bool ThrowVirus(Vector3 player,float yaw){
    if(grabbedVirus<0)return false;
    int ly=LayerOf(player.y);
    if(ly<0)ly=0;
    if(ly>=LEVEL_COUNT)ly=LEVEL_COUNT-1;
    int id=grabbedVirus;
    Vector3 direction={sinf(yaw),0,-cosf(yaw)};
    // Busca um ponto alcancavel no segmento da mira; nunca arremessa
    // a criatura para dentro de uma parede ou para fora do mapa.
    RebuildNearby(player);
    float travel=0;
    for(float d=0.55f;d<=3.60f;d+=0.16f){
        Vector3 candidate={player.x+direction.x*d,FloorY(ly),player.z+direction.z*d};
        if(HunterCollides(candidate))break;
        travel=d;
    }
    if(travel<0.80f){Note("Virus sem espaco para soltar. Afaste-se da parede.");return false;}
    Virus *v=&viruses[id];
    v->pos=(Vector3){player.x+direction.x*travel,FloorY(ly),player.z+direction.z*travel};
    v->homeLayer=ly;v->containedFor=0;v->rerouteTime=0;
    virusThrowDir[id]=direction;
    virusThrowTime[id]=0.38f;
    grabbedVirus=-1;
    Note("Virus lancado! Feche as passagens da sala para captura-lo.");
    return true;
}
// Aproximacao, oito degraus continuos e saida. Subir/descer leva tempo real;
// trocar de andar nao gera um inimigo novo, nem altera o Y instantaneamente.
static void WalkHostile(Vector3 *pos,EnemyRoute *route,Vector3 target,float speed,float dt) {
    int from=LayerOf(pos->y),to=LayerOf(target.y);
    if(from<0)from=0;
    if(from>=LEVEL_COUNT)from=LEVEL_COUNT-1;
    if(to<0)to=0;
    if(to>=LEVEL_COUNT)to=LEVEL_COUNT-1;
    if(route->mode==0 && from!=to) {
        route->mode=(from<to)?1:-1;
        route->base=(from<to)?from:from-1;
        route->cx=ChunkOf(pos->x);route->cz=ChunkOf(pos->z);
        route->step=(route->mode==1)?0:7;
    }
    if(route->mode!=0) {
        Vector3 goal;
        if(route->mode==2 || route->mode==-2) {
            goal=(Vector3){route->cx*CHUNK_SIZE+2.90f,
                  FloorY(route->base)+(route->mode==2?LEVEL_H:0.0f),
                  route->cz*CHUNK_SIZE-0.45f};
            RebuildNearby(*pos);
            if(StepHostile(pos,goal,speed*dt))route->mode=0;
            return;
        }
        goal=StairPoint(route->cx,route->cz,route->base,route->step);
        RebuildNearby(*pos);
        if(StepHostile(pos,goal,speed*dt)) {
            route->step+=(route->mode>0?1:-1);
            if(route->step>=8)route->mode=2;
            else if(route->step<0)route->mode=-2;
        }
        return;
    }
    Vector3 w=HostileWaypoint(*pos,target);
    // No mesmo andar, o deslocamento e horizontal e passa pelas portas.
    w.y=pos->y;
    RebuildNearby(*pos);
    StepHostile(pos,w,speed*dt);
}
static void UpdateHunter(float dt,Vector3 player) {
    if(!hunterEnabled)return;
    for(int i=0;i<6;i++)hunterSeal[i]=MaxF(0,hunterSeal[i]-dt);
    if(!hunterSpawned)SpawnHunter(player);
    hunterCooldown=MaxF(0,hunterCooldown-dt);
    Vector3 target=player;
    bool hasCargo=CarriedKeyCount()>0;
    if(!hasCargo) {
        float best=1e30f;
        for(int k=0;k<6;k++) {
            if(keysFound[k]||hunterSeal[k]>0)continue;
            Vector3 kp={keyRooms[k][0]*CHUNK_SIZE+2,FloorY(keyRooms[k][1]),keyRooms[k][2]*CHUNK_SIZE+2};
            float distance=Vector3Distance(hunterPos,kp);
            if(distance<best){best=distance;target=kp;}
        }
    }
    WalkHostile(&hunterPos,&collectorRoute,target,HUNTER_SPEED,dt);
    EnemyEmbedded(hunterPos,&collectorEmbedded,dt);
    hunterLayer=LayerOf(hunterPos.y);
    if(hunterCooldown<=0 && Vector3Distance(hunterPos,player)<1.5f) {
        for(int k=5;k>=0;k--)if(keysFound[k]&&!keysInserted[k]) {
            keysFound[k]=0;totalKeys--;hunterCooldown=12;
            Note("O COLETOR ROUBOU UMA CHAVE! Defenda a passagem com blocos.");break;
        }
    }
    if(hunterCooldown<=0)for(int k=0;k<6;k++) {
        if(keysFound[k]||keyRooms[k][1]!=hunterLayer||hunterSeal[k]>0)continue;
        Vector3 kp={keyRooms[k][0]*CHUNK_SIZE+2,FloorY(hunterLayer),keyRooms[k][2]*CHUNK_SIZE+2};
        if(Vector3Distance(hunterPos,kp)<1.7f){
            hunterSeal[k]=18;hunterCooldown=4;
            Note("O COLETOR LACROU UMA CHAVE POR 18 SEGUNDOS.");break;
        }
    }
    RebuildNearby(player);
}
static void DrawHunter(void) {
    if(!hunterEnabled||!hunterSpawned)return;
    DrawSphere((Vector3){hunterPos.x,hunterPos.y+0.78f,hunterPos.z},0.48f,HUNTER_COLOR);
    DrawSphereWires((Vector3){hunterPos.x,hunterPos.y+0.78f,hunterPos.z},0.61f,8,10,(Color){255,125,255,255});
}
// CACADOR vermelho, espinhoso e implacavel: alcanca o jogador e reinicia
// TODO o progresso da partida atual (mesma seed); save manual nao e apagado.
static void DrawStalker(void) {
    if(!stalkerSpawned)return;
    Vector3 c={stalkerPos.x,stalkerPos.y+0.82f,stalkerPos.z};
    DrawSphere(c,0.45f,(Color){175,20,29,255});
    for(int i=0;i<10;i++) {
        float theta=(float)i*2*PI/10.0f;
        Vector3 d={cosf(theta),i%2?0.6f:-0.30f,sinf(theta)};
        d=Vector3Normalize(d);
        Vector3 bottom=Vector3Add(c,Vector3Scale(d,0.27f));
        Vector3 tip=Vector3Add(c,Vector3Scale(d,0.98f));
        DrawCylinderEx(bottom,tip,0.23f,0.0f,3,(Color){235,37,45,255});
    }
    DrawSphere((Vector3){c.x,c.y+0.20f,c.z},0.12f,(Color){255,120,95,255});
}
static void SpawnStalker(Vector3 player) {
    stalkerLayer=LayerOf(player.y);
    int cx=ChunkOf(player.x),cz=ChunkOf(player.z);
    int sx=cx+3,sz=cz+2;
    if(!InGrid(sx))sx=cx-3;
    if(!InGrid(sz))sz=cz-2;
    if(!InGrid(sx))sx=cx;
    if(!InGrid(sz))sz=cz;
    stalkerPos=(Vector3){sx*CHUNK_SIZE,FloorY(stalkerLayer),sz*CHUNK_SIZE};
    stalkerSpawned=true;
    stalkerGrace=3.0f;
}
// Coordenada intermediaria = centro da porta que separa os chunks.
static Vector3 HostileWaypoint(Vector3 from,Vector3 target) {
    Vector3 w=target;
    int x=ChunkOf(from.x),z=ChunkOf(from.z);
    int tx=ChunkOf(target.x),tz=ChunkOf(target.z);
    if(x!=tx) {
        w.x=x*CHUNK_SIZE+(tx>x?HALF_CHUNK+0.7f:-HALF_CHUNK-0.7f);
        w.z=z*CHUNK_SIZE;
    } else if(z!=tz) {
        w.x=x*CHUNK_SIZE;
        w.z=z*CHUNK_SIZE+(tz>z?HALF_CHUNK+0.7f:-HALF_CHUNK-0.7f);
    }
    return w;
}
static void UpdateStalker(float dt,Vector3 player,bool *caught) {
    *caught=false;
    if(!stalkerSpawned)SpawnStalker(player);
    stalkerGrace=MaxF(0,stalkerGrace-dt);
    WalkHostile(&stalkerPos,&stalkerRoute,player,STALKER_SPEED,dt);
    EnemyEmbedded(stalkerPos,&stalkerEmbedded,dt);
    stalkerLayer=LayerOf(stalkerPos.y);
    if(stalkerGrace>0)return;
    if(Vector3Distance(stalkerPos,player)<1.15f) {
        if(buffSeconds>0) {
            buffSeconds=0;stalkerGrace=3.0f;
            // Recuo fisico: retorna seguindo o caminho; nao teleporta.
            Note("ESCUDO CONSUMIDO! Caçador temporariamente repelido.");
        }else *caught=true;
    }
}
static void MarkVisited(Vector3 p) {
    int x=ChunkOf(p.x),z=ChunkOf(p.z),y=LayerOf(p.y);
    if(InGrid(x)&&InGrid(z)&&y>=0&&y<LEVEL_COUNT) visited[GridIndex(x)][y][GridIndex(z)]=1;
}
// Trilhas de pao manuais. B deposita um marcador BEGE no local atual.
static bool DropBreadcrumb(Vector3 player,bool grounded) {
    if(!grounded) {Note("Pise em uma superficie antes de deixar uma migalha.");return false;}
    if(breadcrumbCount>=MAX_CRUMBS) {Note("Limite de 640 migalhas alcancado.");return false;}
    for(int i=breadcrumbCount-1;i>=0;i--) {
        if(Vector3Distance(breadcrumbs[i],player)<0.85f) {
            Note("Ja existe uma migalha perto daqui.");return false;
        }
    }
    breadcrumbs[breadcrumbCount++]=(Vector3){player.x,player.y+0.035f,player.z};
    Note("Migalha bege deixada no chao.");
    return true;
}
static void DrawBreadcrumbs(int cx,int cz,int layer) {
    Color beige={207,185,145,255};
    for(int i=0;i<breadcrumbCount;i++) {
        Vector3 p=breadcrumbs[i];
        if(ChunkOf(p.x)!=cx||ChunkOf(p.z)!=cz||LayerOf(p.y)!=layer)continue;
        DrawCircle3D(p,CRUMB_RADIUS,(Vector3){1,0,0},90,beige);
        DrawCircle3D((Vector3){p.x,p.y+0.006f,p.z},0.065f,(Vector3){1,0,0},90,(Color){245,229,192,255});
    }
}
static void DrawBuff(int cx,int cz,int layer) {
    if(layer<0||layer>=LEVEL_COUNT||buffClaimed[layer])return;
    int bx,bz;BuffRoom(layer,&bx,&bz);
    if(cx!=bx||cz!=bz)return;
    Vector3 p=BuffPosition(layer);
    float spin=(float)GetTime();
    DrawCube((Vector3){p.x,p.y+0.45f+0.14f*sinf(spin*2),p.z},0.86f,0.86f,0.86f,(Color){255,138,20,255});
    DrawCubeWires((Vector3){p.x,p.y+0.45f+0.14f*sinf(spin*2),p.z},0.88f,0.88f,0.88f,(Color){255,231,145,255});
    DrawCircle3D((Vector3){p.x,FloorY(layer)+0.045f,p.z},1.04f,(Vector3){1,0,0},90,(Color){205,91,22,255});
}
static void TryCollectBuff(Vector3 player) {
    int layer=LayerOf(player.y);
    if(layer<0||layer>=LEVEL_COUNT||buffClaimed[layer])return;
    if(Vector3Distance(player,BuffPosition(layer))<1.6f) {
        buffClaimed[layer]=1;
        buffSeconds=BUFF_SECONDS;
        Note("ENERGIA LARANJA! 32s: velocidade + pulo + escudo contra Cacador.");
    }
}
// Outro bonus nao reaparece ate que o anterior tenha sido usado.
static void DrawBlue(int cx,int cz,int layer) {
    if(blueStored||cx!=blueCx||cz!=blueCz||layer!=blueLayer)return;
    Vector3 p=BluePosition();float t=(float)GetTime();
    DrawCube((Vector3){p.x,p.y+0.40f+0.12f*sinf(2*t),p.z},0.75f,0.75f,0.75f,(Color){25,125,245,255});
    DrawCubeWires((Vector3){p.x,p.y+0.40f+0.12f*sinf(2*t),p.z},0.81f,0.81f,0.81f,WHITE);
}
static void TryCollectBlue(Vector3 player) {
    if(!blueStored && Vector3Distance(player,BluePosition())<1.6f) {
        blueStored=true;
        Note("SALTO AZUL GUARDADO! E: ir ate um andar com chave. Preco: perder numero do andar.");
    }
}
// Testa posicoes sobre piso seguro, sem ocupar blocos, escada ou borda.
static bool SafeLanding(int cx,int cz,int layer,Vector3 *result) {
    static const Vector2 positions[]={
        {3.6f,3.6f},{-3.6f,3.6f},{3.6f,-3.6f},{-3.6f,-3.6f},
        {4.2f,0.0f},{0.0f,4.2f},{-4.2f,0.0f},{0.0f,-4.2f},
        {3.8f,1.9f},{-3.8f,-1.9f}
    };
    for(int j=0;j<(int)(sizeof positions/sizeof positions[0]);j++) {
        Vector3 p={cx*CHUNK_SIZE+positions[j].x,FloorY(layer),cz*CHUNK_SIZE+positions[j].y};
        if(!FloorSolid(layer,p.x,p.z))continue;
        RebuildNearby(p);
        if(!PlayerCollides(p)) {*result=p;return true;}
    }
    return false;
}
static void TeleportSafeRandom(Vector3 *p,float *vy,bool *grounded) {
    teleportCycle++;
    for(int t=0;t<512;t++) {
        uint32_t h=Hash3((int)teleportCycle,t,431,1501);
        int x=GRID_MIN+(int)(h%GRID_N), z=GRID_MIN+(int)((h>>8)%GRID_N);
        int y=(int)((h>>17)%LEVEL_COUNT);
        if(SafeLanding(x,z,y,p)) {*vy=0;*grounded=true;RebuildNearby(*p);return;}
    }
    *p=(Vector3){3.6f,0,3.6f};*vy=0;*grounded=true;
}
static int NearestUncollectedKey(Vector3 p);
static void UseBlue(Vector3 *p,float *vy,bool *grounded) {
    if(!blueStored){Note("Nao ha salto azul no inventario.");return;}
    int k=NearestUncollectedKey(*p);
    if(k<0){Note("Nao ha chaves restantes no mapa.");return;}
    int cx=keyRooms[k][0],cz=keyRooms[k][2],level=keyRooms[k][1];
    Vector3 dest;
    if(!SafeLanding(cx,cz,level,&dest)){
        Note("Sala de destino bloqueada. O salto azul nao foi gasto.");return;
    }
    *p=dest;*vy=0;*grounded=true;
    int oldBlueLevel=blueLayer;
    blueStored=false;floorIdentityLost=true;
    do{blueCycle++;SpawnBlue();}while(blueLayer==oldBlueLevel);
    disorienterActive=false;
    Note("SALTO REALIZADO. A leitura do andar foi perdida pelo resto da partida.");
}
// O Desorientador so e criado a ate 3 salas (distancia Manhattan 3D) de
// alguma chave solta; se tocar no jogador, confunde por 25 segundos.
static bool NearLooseKey(Vector3 p) {
    int cx=ChunkOf(p.x),cz=ChunkOf(p.z),level=LayerOf(p.y);
    for(int k=0;k<6;k++) if(!keysFound[k]) {
        int d=abs(cx-keyRooms[k][0])+abs(cz-keyRooms[k][2])+abs(level-keyRooms[k][1]);
        if(d<=3)return true;
    }
    return false;
}
static void DrawDisorienter(void) {
    if(!disorienterActive)return;
    Vector3 p={disorienterPos.x,disorienterPos.y+0.95f,disorienterPos.z};
    Color spectral=(Color){115,238,211,255};
    DrawSphere(p,0.42f,spectral);
    for(int j=0;j<6;j++) {
        float a=j*PI/3.0f;
        DrawCylinderEx(Vector3Add(p,(Vector3){0,0,0}),
            Vector3Add(p,(Vector3){cosf(a)*0.75f,0.35f,sinf(a)*0.75f}),
            0.14f,0,3,(Color){17,120,107,255});
    }
}
static void UpdateDisorienter(float dt,float realDt,Vector3 *player,float *vy,bool *grounded) {
    disorienterCooldown=MaxF(0,disorienterCooldown-realDt);
    if(confused) {
        confusionTime=MaxF(0,confusionTime-realDt);
        if(confusionTime<=0){confused=false;Note("A interferencia terminou. HUD restaurada.");}
    }
    if(disorienterCooldown>0||confused||!NearLooseKey(*player)){
        disorienterActive=false;disorienterRoute=(EnemyRoute){0};return;
    }
    if(!disorienterActive) {
        int x=ChunkOf(player->x),z=ChunkOf(player->z),level=LayerOf(player->y);
        int tx=InGrid(x+1)?x+1:x-1;
        disorienterPos=(Vector3){tx*CHUNK_SIZE,FloorY(level),z*CHUNK_SIZE};
        disorienterRoute=(EnemyRoute){0};disorienterActive=true;
    }
    WalkHostile(&disorienterPos,&disorienterRoute,*player,2.75f,dt);
    EnemyEmbedded(disorienterPos,&disorienterEmbedded,dt);
    if(Vector3Distance(disorienterPos,*player)<1.2f) {
        TeleportSafeRandom(player,vy,grounded);
        confused=true;confusionTime=CONFUSION_SECONDS;
        disorienterCooldown=95;disorienterActive=false;
        stalkerGrace=MaxF(stalkerGrace,4.0f);
    }
    RebuildNearby(*player);
}
// Seta VERMELHA tridimensional no campo de visao. Aponta para (0,0,0),
// inclusive para baixo quando o jogador esta nos andares superiores.
static void DrawHubArrow(Camera3D camera,Vector3 player,float yaw,float pitch) {
    Vector3 toHub=Vector3Subtract((Vector3){0,0.30f,0},(Vector3){player.x,player.y+1.2f,player.z});
    if(Vector3LengthSqr(toHub)<0.01f)toHub=(Vector3){0,-1,0};
    toHub=Vector3Normalize(toHub);
    Vector3 front=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    Vector3 right=(Vector3){cosf(yaw),0,sinf(yaw)};
    Vector3 start=Vector3Add(camera.position,Vector3Add(Vector3Scale(front,2.25f),Vector3Add(Vector3Scale(right,0.82f),(Vector3){0,-0.55f,0})));
    Vector3 middle=Vector3Add(start,Vector3Scale(toHub,0.52f));
    Vector3 tip=Vector3Add(start,Vector3Scale(toHub,0.94f));
    DrawCylinderEx(start,middle,0.055f,0.055f,8,(Color){255,58,45,255});
    DrawCylinderEx(middle,tip,0.20f,0.0f,8,(Color){255,37,37,255});
    (void)pitch;
}

static void DrawMap(void) {
    const int cell=43,x0=SCREEN_W/2-5*cell,y0=125;
    DrawRectangle(0,0,SCREEN_W,SCREEN_H,(Color){8,13,21,250});
    DrawText("VECTOR SEARCH // MAPA DE EXPLORACAO",x0,60,23,SKYBLUE);
    DrawText(floorIdentityLost?"PLANO DESCONHECIDO | RODA: TROCAR MAPA":
      TextFormat("ANDAR %d/5 | RODA: MUDAR ANDAR",mapLayer),x0,98,17,LIGHTGRAY);
    int bx,bz;BuffRoom(mapLayer,&bx,&bz);
    for(int z=0;z<GRID_N;z++)for(int x=0;x<GRID_N;x++) {
        int xx=x0+x*cell,yy=y0+z*cell;
        bool seen=visited[x][mapLayer][z]!=0;
        DrawRectangle(xx,yy,cell-3,cell-3,seen?(Color){66,82,87,255}:(Color){22,27,37,255});
        DrawRectangleLines(xx,yy,cell-3,cell-3,(Color){83,91,105,255});
        // Bônus SEMPRE marcado, mesmo em sala inexplorada.
        if(x==GridIndex(blueCx) && z==GridIndex(blueCz) && mapLayer==blueLayer && !blueStored)
            DrawRectangle(xx+cell/2-4,yy+cell/2-4,9,9,BLUE);
        if(x==GridIndex(bx)&&z==GridIndex(bz)){
            Color icon=buffClaimed[mapLayer]?(Color){87,87,84,255}:(Color){255,142,28,255};
            DrawRectangle(xx+cell/2-8,yy+cell/2-8,16,16,icon);
        }
    }
    // Virus ativos aparecem no mapa mesmo que a sala nao tenha sido vista.
    // A posicao do JOGADOR nao e desenhada em nenhum momento.
    for(int i=0;i<VIRUS_TOTAL;i++){
        Virus v=viruses[i];
        if(v.captured || v.homeLayer!=mapLayer)continue;
        int cx=ChunkOf(v.pos.x),cz=ChunkOf(v.pos.z);
        if(!InGrid(cx)||!InGrid(cz))continue;
        float u=ClampF((v.pos.x-(cx*CHUNK_SIZE-HALF_CHUNK))/CHUNK_SIZE,0,0.999f);
        float w=ClampF((v.pos.z-(cz*CHUNK_SIZE-HALF_CHUNK))/CHUNK_SIZE,0,0.999f);
        int xx=x0+GridIndex(cx)*cell+(int)(u*(cell-3));
        int yy=y0+GridIndex(cz)*cell+(int)(w*(cell-3));
        DrawCircle(xx,yy,5,(Color){50,245,99,255});
        DrawCircle(xx,yy,2,(Color){17,90,41,255});
    }
    // As migalhas sao marcas intencionais, nunca um rastro automatico.
    for(int i=0;i<breadcrumbCount;i++) {
        Vector3 p=breadcrumbs[i];
        if(LayerOf(p.y)!=mapLayer)continue;
        int cx=ChunkOf(p.x),cz=ChunkOf(p.z);
        if(!InGrid(cx)||!InGrid(cz))continue;
        float u=(p.x-(cx*CHUNK_SIZE-HALF_CHUNK))/CHUNK_SIZE;
        float v=(p.z-(cz*CHUNK_SIZE-HALF_CHUNK))/CHUNK_SIZE;
        int px=x0+GridIndex(cx)*cell+(int)(u*(cell-3));
        int py=y0+GridIndex(cz)*cell+(int)(v*(cell-3));
        DrawCircle(px,py,3,(Color){207,185,145,255});
    }
    DrawText("VERDE = virus | BEGE = trilha | LARANJA/AZUL = bonus",x0,y0+GRID_N*cell+18,16,LIGHTGRAY);
    DrawText("Salas conhecidas = fundo claro. Mapa NAO mostra sua posicao.",x0,y0+GRID_N*cell+42,15,LIGHTGRAY);
    DrawText("TAB: FECHAR | Movimento e inimigos pausados aqui",x0,y0+GRID_N*cell+64,15,LIGHTGRAY);
    DrawText(TextFormat("CHAVES %d/6 | CARREGANDO %d/2 | NO HEXAGONO %d/6 | VIRUS %d/18",totalKeys,CarriedKeyCount(),InsertedKeyCount(),virusesCaptured),x0,y0+GRID_N*cell+88,17,GOLD);
}

// Modelos carregados a partir de assets/<pasta>/<arquivo.obj>.
// A geometria deve ter pes no Y=0 e altura aproximada de 1.65 m.
// O formato OBJ usa MTL externo; as imagens citadas pelo MTL devem estar
// na mesma pasta, com nomes identicos aos usados no arquivo .mtl.
static bool LoadAvatar(Model *avatar,const char *path)
{
    if(!path || !FileExists(path))return false;
    *avatar=LoadModel(path);
    if(avatar->meshCount<=0 || avatar->meshes==NULL)return false;

    // Compatibilidade opcional para modelos com uma unica textura e sem MTL.
    // Se houver varios materiais (ex.: maos, cabeca, roupa), use o MTL.
    char folder[AVATAR_PATH_MAX];
    snprintf(folder,sizeof folder,"%s",path);
    char *slash=strrchr(folder,'/');
    if(slash)*slash=0;
    char imagePath[AVATAR_PATH_MAX]="";
    const char *imageNames[]={"player.png","texture.png","textura.png"};
    for(size_t i=0;i<sizeof(imageNames)/sizeof(imageNames[0]);i++) {
        char candidate[AVATAR_PATH_MAX];
        int n=snprintf(candidate,sizeof candidate,"%s/%s",folder,imageNames[i]);
        if(n>0 && (size_t)n<sizeof candidate && FileExists(candidate)) {
            snprintf(imagePath,sizeof imagePath,"%s",candidate);
            break;
        }
    }
    for(int m=0;m<avatar->materialCount;m++) {
        if(!avatar->materials[m].maps)continue;
        Texture2D tex=avatar->materials[m].maps[MATERIAL_MAP_ALBEDO].texture;
        if((tex.id==0 || (tex.width<=1 && tex.height<=1)) && imagePath[0]) {
            Texture2D image=LoadTexture(imagePath);
            if(image.id>0)SetMaterialTexture(&avatar->materials[m],MATERIAL_MAP_ALBEDO,image);
        }
        tex=avatar->materials[m].maps[MATERIAL_MAP_ALBEDO].texture;
        fprintf(stderr,"[avatar] %s | material %d: textura %dx%d (id %u)\n",
                path,m,tex.width,tex.height,tex.id);
    }
    return true;
}
static bool ReloadAvatar(Model *avatar,bool *loaded) {
    if(*loaded)UnloadModel(*avatar);
    *avatar=(Model){0};
    *loaded=LoadAvatar(avatar,SelectedAvatarPath());
    return *loaded;
}
static void ChangeAvatar(int direction,Model *avatar,bool *loaded) {
    if(avatarCount<1)return;
    selectedAvatar=(selectedAvatar+direction+avatarCount)%avatarCount;
    ReloadAvatar(avatar,loaded);
    StoreAvatarChoice();
    Note(*loaded?"Modelo selecionado.":"Nao foi possivel abrir o OBJ; modelo reserva ativo.");
}
static void RefreshAvatars(Model *avatar,bool *loaded) {
    char remembered[AVATAR_NAME_MAX]={0};
    snprintf(remembered,sizeof remembered,"%s",SelectedAvatarName());
    FindAvatars();
    for(int i=0;i<avatarCount;i++) if(strcmp(remembered,availableAvatars[i].name)==0) {
        selectedAvatar=i;break;
    }
    ReloadAvatar(avatar,loaded);
    StoreAvatarChoice();
}
static void DrawAvatar(const Model *avatar, bool loaded, Vector3 feet, float yaw)
{
    // Tinta branca: nao desbota nem colore os pixels da textura original.
    Color tint=WHITE;
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
    DrawFilledRoom(0,0,0,false);
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
static void DrawRoomForCinema(int cx,int cz,int layer);
static void DrawStalker(void);
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
    DrawRoomForCinema(cx,cz,layer);
    DrawBreadcrumbs(cx,cz,layer);
    DrawBuff(cx,cz,layer);
    DrawArtifacts(cx,cz,layer);
    DrawViruses(cx,cz,layer);
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
    if(stalkerSpawned && stalkerLayer==layer && ChunkOf(stalkerPos.x)==cx && ChunkOf(stalkerPos.z)==cz)DrawStalker();
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
// ------------------------------------------ CAMERA DE REPLAY -----------------------------------------
// O video depende de ffmpeg instalado no computador do jogador. Nao grava
// arquivos temporarios gigantes: os pixels RGBA sao enviados diretamente ao
// codificador, e o MP4 e finalizado com pclose ao sair/vencer.
static bool RecorderVideoFolder(char *out,size_t capacity) {
    const char *home=getenv("HOME");
    if(!home || !*home)return false;
    out[0]=0;
    // Em Linux Mint, XDG_VIDEO_DIR normalmente e ~/Videos ou ~/Vídeos.
    FILE *p=popen("xdg-user-dir VIDEOS 2>/dev/null","r");
    if(p) {
        if(fgets(out,(int)capacity,p)) {
            out[strcspn(out,"\r\n")]=0;
            if(out[0]!='/')out[0]=0;
        }
        pclose(p);
    }
    if(!out[0]) {
        char local[VIDEO_PATH_CAP];
        snprintf(local,sizeof local,"%s/Vídeos",home);
        if(DirectoryExists(local))snprintf(out,capacity,"%s",local);
        else snprintf(out,capacity,"%s/Videos",home);
    }
    if(!DirectoryExists(out) && mkdir(out,0755)!=0 && errno!=EEXIST)return false;
    return DirectoryExists(out);
}
// Protege caminhos contendo espacos e apostrofos, sem permitir comando
// injetado pelo nome da pasta pessoal.
static bool ShellQuote(const char *src,char *out,size_t n) {
    size_t k=0;
    if(n<3)return false;
    out[k++]='\'';
    for(size_t i=0;src[i];i++) {
        const char *q=(src[i]=='\'')?"'\\''":NULL;
        if(q) {for(int j=0;q[j];j++){if(k+2>=n)return false;out[k++]=q[j];}}
        else {if(k+2>=n)return false;out[k++]=src[i];}
    }
    if(k+2>n)return false;
    out[k++]='\'';out[k]=0;
    return true;
}
static void RecorderStop(void) {
    if(!recorder.pipe)return;
    // O codificador precisa receber EOF para escrever o indice MP4 (moov).
    int status=pclose(recorder.pipe);
    recorder.pipe=NULL;
    if(status==0)fprintf(stderr,"[video] salvo: %s (%llu quadros)\n",
              recorder.file,(unsigned long long)recorder.frames);
    else fprintf(stderr,"[video] ffmpeg falhou (status %d). Confira %s\n",status,recorder.file);
    recorder.accumulator=0;recorder.frames=0;
}
static bool RecorderStart(void) {
    RecorderStop();
    if(!recorder.targetReady){fprintf(stderr,"[video] framebuffer indisponivel\n");return false;}
    if(system("ffmpeg -version >/dev/null 2>&1")!=0) {
        fprintf(stderr,"[video] ffmpeg ausente. Instale: sudo apt install ffmpeg\n");
        return false;
    }
    char folder[VIDEO_PATH_CAP];
    if(!RecorderVideoFolder(folder,sizeof folder)){
        fprintf(stderr,"[video] nao consegui localizar/criar a pasta Videos/Videos do usuario\n");return false;
    }
    time_t now=time(NULL);struct tm tmnow={0};
    struct tm *t=localtime(&now);if(t)tmnow=*t;
    recordingSequence++;
    int n=snprintf(recorder.file,sizeof recorder.file,
            "%s/VectorSearch_%04d%02d%02d_%02d%02d%02d_seed%u_%03u.mp4",
            folder,tmnow.tm_year+1900,tmnow.tm_mon+1,tmnow.tm_mday,
            tmnow.tm_hour,tmnow.tm_min,tmnow.tm_sec,worldSeed,recordingSequence);
    if(n<=0 || (size_t)n>=sizeof recorder.file)return false;
    char quoted[VIDEO_PATH_CAP*4];
    if(!ShellQuote(recorder.file,quoted,sizeof quoted))return false;
    char command[VIDEO_PATH_CAP*4+512];
    snprintf(command,sizeof command,
        "ffmpeg -hide_banner -loglevel error -nostdin -y "
        "-f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d -i - "
        "-an -c:v libx264 -preset veryfast -crf 22 -pix_fmt yuv420p "
        "-movflags +faststart %s",VIDEO_W,VIDEO_H,VIDEO_FPS,quoted);
    recorder.pipe=popen(command,"w");
    recorder.accumulator=0;recorder.frames=0;
    if(!recorder.pipe){perror("[video] popen");return false;}
    fprintf(stderr,"[video] gravando: %s\n",recorder.file);
    return true;
}
// Cenografia de "casa de bonecas": mantem o piso e as paredes de FUNDO.
// Remove as duas paredes de frente e o teto, que cobriam o personagem.
static void DrawRoomForCinema(int cx,int cz,int layer){
    Color base=RoomColor(cx,cz,layer);
    DrawFloor(cx,cz,layer,Shade(base,0.57f),false);
    BoundingBox walls[8];int n=WallsFor(cx,cz,layer,walls);
    float west=cx*CHUNK_SIZE-HALF_CHUNK,north=cz*CHUNK_SIZE-HALF_CHUNK;
    for(int i=0;i<n;i++){
        BoundingBox b=walls[i];
        if(fabsf((b.min.x+b.max.x)*0.5f-west)<0.14f ||
           fabsf((b.min.z+b.max.z)*0.5f-north)<0.14f)
            DrawWallBox(b,Shade(base,0.90f),false);
    }
}
static void RecorderRenderScene(RenderTexture2D target,Vector3 player,float yaw,
                       const Model *avatar,bool loaded) {
    int cx=ChunkOf(player.x),cz=ChunkOf(player.z),ly=LayerOf(player.y);
    if(ly<0)ly=0;
    if(ly>=LEVEL_COUNT)ly=LEVEL_COUNT-1;
    float x=cx*CHUNK_SIZE,z=cz*CHUNK_SIZE,y=FloorY(ly);
    Camera3D cam={0};
    // Camera fixa dentro do canto +X/+Z e perto do teto da sala atual.
    // Troca sem atraso assim que muda cx/cz/ly. Orientacao permanece fixa.
    cam.position=(Vector3){x+4.25f,y+5.45f,z+4.25f};
    cam.target=(Vector3){x,y+1.45f,z};
    cam.up=(Vector3){0,1,0};cam.fovy=18.0f;
    cam.projection=CAMERA_ORTHOGRAPHIC;
    BeginTextureMode(target);
    ClearBackground((Color){8,11,17,255});
    BeginMode3D(cam);
    DrawRoomForCinema(cx,cz,ly);
    DrawBreadcrumbs(cx,cz,ly);
    DrawBuff(cx,cz,ly);
    DrawBlue(cx,cz,ly);
    DrawArtifacts(cx,cz,ly);
    DrawViruses(cx,cz,ly);
    // O mundo procedural nao exige guardar toda geometria de cada sala.
    nearbyCount=0;
    GenerateChunk(cx,cz,ly);
    for(int j=0;j<placedCount;j++)if(placed[j].active &&
       ChunkOf(placed[j].block.pos.x)==cx && ChunkOf(placed[j].block.pos.z)==cz &&
       LayerOf(placed[j].block.pos.y)==ly)PushNearby(placed[j].block);
    for(int i=0;i<nearbyCount;i++){
        Block b=nearby[i];
        if(ChunkOf(b.pos.x)!=cx || ChunkOf(b.pos.z)!=cz || LayerOf(b.pos.y)!=ly)continue;
        DrawCube(b.pos,b.size.x,b.size.y,b.size.z,BlockColor(b));
    }
    if(hunterSpawned && ChunkOf(hunterPos.x)==cx && ChunkOf(hunterPos.z)==cz &&
       LayerOf(hunterPos.y)==ly) DrawHunter();
    if(stalkerSpawned && ChunkOf(stalkerPos.x)==cx && ChunkOf(stalkerPos.z)==cz &&
       LayerOf(stalkerPos.y)==ly) DrawStalker();
    if(disorienterActive && ChunkOf(disorienterPos.x)==cx && ChunkOf(disorienterPos.z)==cz &&
       LayerOf(disorienterPos.y)==ly)DrawDisorienter();
    DrawAvatar(avatar,loaded,player,yaw);
    EndMode3D();
    DrawText("VECTOR SEARCH  //  CAM 01",12,12,19,(Color){200,220,217,255});
    DrawText("[REC]  CAMERA ORTOGRAFICA",12,VIDEO_H-27,16,(Color){240,90,88,255});
    EndTextureMode();
    RebuildNearby(player);
}
static void RecorderFrame(Vector3 player,float yaw,const Model *avatar,
                          bool loaded,float dt) {
    if(!recorder.pipe || !recorder.targetReady)return;
    recorder.accumulator+=ClampF(dt,0.0f,0.15f);
    int count=(int)(recorder.accumulator*VIDEO_FPS);
    if(count>3)count=3;
    if(count<=0)return;
    recorder.accumulator-=(double)count/VIDEO_FPS;
    RecorderRenderScene(recorder.target,player,yaw,avatar,loaded);
    Image frame=LoadImageFromTexture(recorder.target.texture);
    if(frame.data==NULL || frame.width!=VIDEO_W || frame.height!=VIDEO_H){
        if(frame.data)UnloadImage(frame);
        fprintf(stderr,"[video] falha na captura do framebuffer\n");RecorderStop();return;
    }
    ImageFlipVertical(&frame);
    ImageFormat(&frame,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    size_t bytes=(size_t)VIDEO_W*VIDEO_H*4;
    for(int i=0;i<count;i++){
        if(fwrite(frame.data,1,bytes,recorder.pipe)!=bytes) {
            fprintf(stderr,"[video] ffmpeg interrompeu a gravacao\n");
            RecorderStop();break;
        }
        recorder.frames++;
    }
    UnloadImage(frame);
}
// -----------------------------------------------------------------------------------------------------
// Arquivo VS11 separado. A topologia 10x10x6 nao e compativel com os
// saves VS10 de 8 andares; os arquivos antigos nao sao apagados.
typedef struct {
    char magic[8];
    uint32_t version,bytes,seed;
    Vector3 player;
    float yaw,pitch,vy;
    uint8_t carrying;
    uint32_t removedCount,placedCount,breadcrumbCount;
    Removed removed[MAX_REMOVED];
    Placed placed[MAX_PLACED];
    Block carriedOrigin;
    uint8_t visited[GRID_N][LEVEL_COUNT][GRID_N];
    uint8_t keysFound[6],keysInserted[6],floorCleared[LEVEL_COUNT];
    Vector3 breadcrumbs[MAX_CRUMBS];
    uint8_t buffClaimed[LEVEL_COUNT];
    float buffSeconds,runSeconds;
    uint8_t floorIdentityLost,blueStored,confused,disorienterActive;
    int blueLayer,blueCx,blueCz;
    uint32_t blueCycle,teleportCycle;
    float confusionTime,disorienterCooldown;
    Vector3 disorienterPos,hunterPos,stalkerPos;
    int hunterLayer,stalkerLayer;
    float hunterCooldown,hunterSeal[6],stalkerGrace;
    uint8_t hunterSpawned,stalkerSpawned;
    EnemyRoute collectorRoute,stalkerRoute,disorienterRoute;
    float collectorEmbedded,stalkerEmbedded,disorienterEmbedded;
    Virus viruses[VIRUS_TOTAL];
    uint32_t virusesCaptured;
} Save11;
static bool SaveGame(Vector3 p,float yaw,float pitch,float vy,bool carrying){
    Save11 *s=calloc(1,sizeof *s);if(!s)return false;
    memcpy(s->magic,"VS11SAVE",8);s->version=11;s->bytes=(uint32_t)sizeof *s;
    s->seed=worldSeed;s->player=p;s->yaw=yaw;s->pitch=pitch;s->vy=vy;
    s->carrying=(uint8_t)carrying;
    s->removedCount=(uint32_t)removedCount;s->placedCount=(uint32_t)placedCount;
    s->breadcrumbCount=(uint32_t)breadcrumbCount;
    memcpy(s->removed,removed,(size_t)removedCount*sizeof(Removed));
    memcpy(s->placed,placed,(size_t)placedCount*sizeof(Placed));
    s->carriedOrigin=carriedOrigin;
    memcpy(s->visited,visited,sizeof visited);
    memcpy(s->keysFound,keysFound,sizeof keysFound);
    memcpy(s->keysInserted,keysInserted,sizeof keysInserted);
    memcpy(s->floorCleared,floorCleared,sizeof floorCleared);
    memcpy(s->breadcrumbs,breadcrumbs,(size_t)breadcrumbCount*sizeof(Vector3));
    memcpy(s->buffClaimed,buffClaimed,sizeof buffClaimed);
    s->buffSeconds=buffSeconds;s->runSeconds=runSeconds;
    s->floorIdentityLost=(uint8_t)floorIdentityLost;s->blueStored=(uint8_t)blueStored;
    s->confused=(uint8_t)confused;s->disorienterActive=(uint8_t)disorienterActive;
    s->blueLayer=blueLayer;s->blueCx=blueCx;s->blueCz=blueCz;
    s->blueCycle=blueCycle;s->teleportCycle=teleportCycle;
    s->confusionTime=confusionTime;s->disorienterCooldown=disorienterCooldown;
    s->disorienterPos=disorienterPos;
    s->hunterPos=hunterPos;s->hunterLayer=hunterLayer;
    s->hunterCooldown=hunterCooldown;memcpy(s->hunterSeal,hunterSeal,sizeof hunterSeal);
    s->stalkerPos=stalkerPos;s->stalkerLayer=stalkerLayer;s->stalkerGrace=stalkerGrace;
    s->hunterSpawned=(uint8_t)hunterSpawned;s->stalkerSpawned=(uint8_t)stalkerSpawned;
    s->collectorRoute=collectorRoute;s->stalkerRoute=stalkerRoute;s->disorienterRoute=disorienterRoute;
    s->collectorEmbedded=collectorEmbedded;s->stalkerEmbedded=stalkerEmbedded;
    s->disorienterEmbedded=disorienterEmbedded;
    memcpy(s->viruses,viruses,sizeof viruses);s->virusesCaptured=(uint32_t)virusesCaptured;
    FILE *f=fopen(SAVE_FILE ".tmp","wb");
    bool ok=f && fwrite(s,sizeof *s,1,f)==1;
    if(f && fclose(f)!=0)ok=false;
    if(ok && rename(SAVE_FILE ".tmp",SAVE_FILE)!=0)ok=false;
    if(!ok)remove(SAVE_FILE ".tmp");
    free(s);return ok;
}
static bool LoadGame(Vector3 *p,float *yaw,float *pitch,float *vy,bool *carrying){
    FILE *f=fopen(SAVE_FILE,"rb");if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
    long fileSize=ftell(f);rewind(f);
    if(fileSize!=(long)sizeof(Save11)){fclose(f);return false;}
    Save11 *s=malloc(sizeof *s);
    bool ok=s && fread(s,sizeof *s,1,f)==1;fclose(f);
    if(ok){
        ok=!memcmp(s->magic,"VS11SAVE",8)&&s->version==11&&s->bytes==sizeof *s&&
           s->removedCount<=MAX_REMOVED&&s->placedCount<=MAX_PLACED&&
           s->breadcrumbCount<=MAX_CRUMBS&&s->virusesCaptured<=VIRUS_TOTAL&&
           s->blueLayer>=0&&s->blueLayer<LEVEL_COUNT&&
           InGrid(s->blueCx)&&InGrid(s->blueCz)&&
           isfinite(s->player.x)&&isfinite(s->player.y)&&isfinite(s->player.z)&&
           s->player.x>WORLD_MIN+PLAYER_RADIUS&&s->player.x<WORLD_MAX-PLAYER_RADIUS&&
           s->player.z>WORLD_MIN+PLAYER_RADIUS&&s->player.z<WORLD_MAX-PLAYER_RADIUS&&
           s->player.y>=0&&s->player.y<=LEVEL_COUNT*LEVEL_H-PLAYER_HEIGHT;
        int found=0;
        for(int i=0;ok&&i<VIRUS_TOTAL;i++){
            if(s->viruses[i].homeLayer!=i/VIRUSES_PER_FLOOR ||
               !isfinite(s->viruses[i].pos.x)||!isfinite(s->viruses[i].pos.y)||
               !isfinite(s->viruses[i].pos.z))ok=false;
            if(s->viruses[i].captured)found++;
        }
        if(found!=(int)s->virusesCaptured)ok=false;
    }
    if(!ok){free(s);return false;}
    worldSeed=s->seed;*p=s->player;*yaw=s->yaw;*pitch=s->pitch;*vy=s->vy;
    *carrying=s->carrying!=0;
    removedCount=(int)s->removedCount;placedCount=(int)s->placedCount;
    breadcrumbCount=(int)s->breadcrumbCount;
    memcpy(removed,s->removed,(size_t)removedCount*sizeof(Removed));
    memcpy(placed,s->placed,(size_t)placedCount*sizeof(Placed));
    carriedOrigin=s->carriedOrigin;
    memcpy(visited,s->visited,sizeof visited);
    SetKeys();totalKeys=0;
    memcpy(keysFound,s->keysFound,sizeof keysFound);
    memcpy(keysInserted,s->keysInserted,sizeof keysInserted);
    memcpy(floorCleared,s->floorCleared,sizeof floorCleared);
    for(int i=0;i<6;i++)if(keysFound[i])totalKeys++;
    memcpy(breadcrumbs,s->breadcrumbs,(size_t)breadcrumbCount*sizeof(Vector3));
    memcpy(buffClaimed,s->buffClaimed,sizeof buffClaimed);
    buffSeconds=ClampF(s->buffSeconds,0,BUFF_SECONDS);
    runSeconds=ClampF(s->runSeconds,0,9999999);
    floorIdentityLost=s->floorIdentityLost!=0;blueStored=s->blueStored!=0;
    confused=s->confused!=0;disorienterActive=s->disorienterActive!=0;
    blueLayer=s->blueLayer;blueCx=s->blueCx;blueCz=s->blueCz;
    blueCycle=s->blueCycle;teleportCycle=s->teleportCycle;
    confusionTime=ClampF(s->confusionTime,0,CONFUSION_SECONDS);
    disorienterCooldown=ClampF(s->disorienterCooldown,0,120);
    disorienterPos=s->disorienterPos;
    hunterPos=s->hunterPos;hunterLayer=s->hunterLayer;
    hunterCooldown=s->hunterCooldown;memcpy(hunterSeal,s->hunterSeal,sizeof hunterSeal);
    stalkerPos=s->stalkerPos;stalkerLayer=s->stalkerLayer;stalkerGrace=s->stalkerGrace;
    hunterSpawned=s->hunterSpawned!=0;stalkerSpawned=s->stalkerSpawned!=0;
    collectorRoute=s->collectorRoute;stalkerRoute=s->stalkerRoute;disorienterRoute=s->disorienterRoute;
    collectorEmbedded=s->collectorEmbedded;stalkerEmbedded=s->stalkerEmbedded;
    disorienterEmbedded=s->disorienterEmbedded;
    memcpy(viruses,s->viruses,sizeof viruses);virusesCaptured=(int)s->virusesCaptured;
    mapOpen=false;currentLayer=mapLayer=LayerOf(p->y);
    grabbedVirus=-1;
    memset(virusThrowTime,0,sizeof virusThrowTime);
    free(s);return true;
}
static void NewGame(uint32_t seed,Vector3 *p,float *vy,float *yaw,float *pitch,bool *carrying) {
    worldSeed=seed;ResetEdits();memset(visited,0,sizeof visited);SetKeys();
    ResetPlayer(p,vy,yaw,pitch,&(bool){true},carrying);
    currentLayer=mapLayer=0;mapOpen=false;MarkVisited(*p);
    memset(hunterSeal,0,sizeof hunterSeal);hunterSpawned=false;hunterCooldown=2.5f;
    memset(&carriedOrigin,0,sizeof carriedOrigin);
    collectorEmbedded=stalkerEmbedded=disorienterEmbedded=0;
    stalkerSpawned=false;stalkerGrace=4.0f;
    breadcrumbCount=0;memset(buffClaimed,0,sizeof buffClaimed);buffSeconds=0;
    ResetNewFeatures();
    SetViruses();
    grabbedVirus=-1;
    memset(virusThrowTime,0,sizeof virusThrowTime);
    Note("Mundo 10x10x6: seis chaves e 18 virus para conter!");
}
static int ScoreCompare(const void *a,const void *b) {
    const Score *x=(const Score*)a,*y=(const Score*)b;
    return x->ms<y->ms?-1:x->ms>y->ms?1:0;
}
static void LoadLocalScores(void) {
    localScoreCount=0;
    FILE *f=fopen(RANKING_FILE,"r");if(!f)return;
    char line[256];
    while(fgets(line,sizeof line,f)) {
        Score t={0};
        if(sscanf(line,"%23s %llu %u",t.name,&t.ms,&t.seed)==3 && t.ms>0){
            if(localScoreCount<100)localScores[localScoreCount++]=t;
            else {
                int worst=0;
                for(int i=1;i<localScoreCount;i++)if(localScores[i].ms>localScores[worst].ms)worst=i;
                if(t.ms<localScores[worst].ms)localScores[worst]=t;
            }
        }
    }
    fclose(f);
    qsort(localScores,(size_t)localScoreCount,sizeof(Score),ScoreCompare);
}
static bool ValidServerURL(const char *s) {
    if(!s || !(strncmp(s,"https://",8)==0||strncmp(s,"http://",7)==0))return false;
    for(const unsigned char *p=(const unsigned char*)s;*p;p++)
        if(!(isalnum(*p)||strchr(":/._-",*p)))return false;
    return strlen(s)<250;
}
// Uma URL de servidor deve ser configurada em VECTOR_SEARCH_RANKING_URL.
// Nunca vendemos um ranking local como um placar mundial.
static bool PostOnlineScore(Score score) {
    const char *url=getenv("VECTOR_SEARCH_RANKING_URL");
    if(!ValidServerURL(url))return false;
    char command[700];
    int n=snprintf(command,sizeof command,
        "curl -f -s -S --max-time 4 -X POST --data-urlencode 'name=%s' "
        "--data-urlencode 'time_ms=%llu' --data-urlencode 'seed=%u' '%s/submit' >/dev/null 2>&1",
        score.name,score.ms,score.seed,url);
    if(n<0||(size_t)n>=sizeof command)return false;
    return system(command)==0;
}
static void FetchOnlineScores(void) {
    onlineScoreCount=0;onlineAvailable=false;
    const char *url=getenv("VECTOR_SEARCH_RANKING_URL");
    if(!ValidServerURL(url))return;
    char command[600];
    int n=snprintf(command,sizeof command,"curl -fsS --max-time 4 '%s/ranking' 2>/dev/null",url);
    if(n<0||(size_t)n>=sizeof command)return;
    FILE *f=popen(command,"r");if(!f)return;
    char line[256];
    // Primeira linha do servidor: cabecalho explicito VS-RANKING-1.
    if(fgets(line,sizeof line,f) && !strncmp(line,"VS-RANKING-1",12)) {
        onlineAvailable=true;
        while(fgets(line,sizeof line,f) && onlineScoreCount<100){
            Score t={0};
            if(sscanf(line,"%23s %llu %u",t.name,&t.ms,&t.seed)==3&&t.ms>0)
                onlineScores[onlineScoreCount++]=t;
        }
    }
    if(pclose(f)!=0)onlineAvailable=false;
    qsort(onlineScores,(size_t)onlineScoreCount,sizeof(Score),ScoreCompare);
}
static void SubmitVictoryScore(void) {
    if(scoreSubmitted)return;
    if(winnerNameLen==0){strcpy(winnerName,"ANONIMO");winnerNameLen=7;}
    Score t={0};snprintf(t.name,sizeof t.name,"%s",winnerName);
    t.ms=(unsigned long long)(runSeconds*1000.0f);if(t.ms==0)t.ms=1;
    t.seed=worldSeed;
    FILE *f=fopen(RANKING_FILE,"a");
    if(f){fprintf(f,"%s\t%llu\t%u\n",t.name,t.ms,t.seed);fclose(f);}
    LoadLocalScores();
    scoreSubmitted=true;
    PostOnlineScore(t); // sem servidor, o registro local continua disponivel
}
static void TimeString(float seconds,char out[32]) {
    unsigned long long total=(unsigned long long)seconds;
    snprintf(out,32,"%02llu:%02llu:%02llu",total/3600ull,(total/60ull)%60ull,total%60ull);
}
static void DrawInstructions(void) {
    ClearBackground((Color){9,13,23,255});
    DrawText("VECTOR SEARCH // INSTRUCOES",75,45,35,SKYBLUE);
    DrawText("VITORIA: 6 triangulos no hexagono + 18 virus contidos (3 por andar).",75,104,19,GOLD);
    DrawText("WASD mover   MOUSE olhar   ESPACO pular   SHIFT correr",75,157,20,LIGHTGRAY);
    DrawText("ESQUERDO: pegar/colocar bloco em grade 1m   V: devolver   F: chave",75,198,19,LIGHTGRAY);
    DrawText("B trilha bege   TAB mapa (pausa)   DIR. agarrar/lancar virus",75,239,19,LIGHTGRAY);
    DrawText("E: salto AZUL (perda PERMANENTE da leitura do andar).",75,280,19,(Color){90,170,255,255});
    DrawText("BONUS LARANJA: velocidade, salto e protecao temporaria.",75,321,19,ORANGE);
    DrawText("COLETOR ROXO: rouba e lacra triangulos. CACADOR VERMELHO: reinicia partida.",75,362,19,(Color){226,145,205,255});
    DrawText("DESORIENTADOR: confusao de 25s; AZUL: punicao permanente.",75,403,19,(Color){105,224,204,255});
    DrawText("18 VIRUS LENTOS: leve-os a salas e bloqueie portas por 5s.",75,444,19,LIGHTGRAY);
    DrawText("CTRL+S salva    F2 reinicia nova seed    SHIFT+F2 mesma seed",75,485,19,LIGHTGRAY);
    DrawText("ESC menu    Mapa nao revela sua posicao. RANKING LOCAL SALVO NO DISCO.",75,526,19,LIGHTGRAY);
    DrawText("VIDEO: a jogada sera gravada automaticamente em MP4 (ffmpeg).",75,561,18,SKYBLUE);
    DrawRectangle(75,594,1115,80,(Color){24,40,55,255});
    DrawText("ENTER: JOGAR                                      ESC: MENU",101,577,21,GOLD);
}
static void DrawWorldVictory(void){
    Camera3D cam={0};
    cam.position=(Vector3){125,115,135};cam.target=(Vector3){-5,16,-5};
    cam.up=(Vector3){0,1,0};cam.fovy=65;cam.projection=CAMERA_PERSPECTIVE;
    BeginMode3D(cam);
    Color outline={68,155,92,255};
    // As 600 salas, distribuidas pelos seis andares, simultaneamente.
    for(int l=0;l<LEVEL_COUNT;l++)for(int z=GRID_MIN;z<=GRID_MAX;z++)
    for(int x=GRID_MIN;x<=GRID_MAX;x++){
        DrawCubeWires((Vector3){x*CHUNK_SIZE,FloorY(l)+LEVEL_H/2,z*CHUNK_SIZE},
                      CHUNK_SIZE,LEVEL_H,CHUNK_SIZE,outline);
        BoundingBox walls[8];int count=WallsFor(x,z,l,walls);
        for(int i=0;i<count;i++)DrawWallBox(walls[i],outline,true);
    }
    EndMode3D();
}
static void DrawWin(bool submitted) {
    char elapsed[32];TimeString(runSeconds,elapsed);
    ClearBackground((Color){8,15,25,255});
    DrawWorldVictory();
    DrawRectangle(62,46,1110,600,(Color){8,13,23,210});
    DrawText("VECTOR SEARCH // VITORIA!",100,75,48,GOLD);
    DrawText("As 6 chaves estao no hexagono e os 18 virus foram contidos.",102,162,22,LIGHTGRAY);
    DrawText(TextFormat("TEMPO: %s",elapsed),104,229,30,SKYBLUE);
    DrawText(TextFormat("SEED: %u",worldSeed),104,280,20,LIGHTGRAY);
    if(!submitted) {
        DrawText("DIGITE SEU NOME PARA REGISTRAR O RESULTADO (A-Z, 0-9, _ ou -):",104,345,19,LIGHTGRAY);
        DrawRectangle(105,387,545,55,(Color){27,45,63,255});
        DrawText(winnerNameLen?winnerName:"ANONIMO",118,401,27,WHITE);
        DrawText("ENTER: REGISTRAR",105,470,22,GOLD);
    }else {
        DrawText(TextFormat("RESULTADO REGISTRADO: %s",winnerName),104,357,25,GREEN);
        DrawText("O registro local fica salvo mesmo apos fechar o jogo.",104,407,19,LIGHTGRAY);
    }
    DrawText("G: ABRIR RANKING      M: MENU PRINCIPAL",104,552,24,SKYBLUE);
}
static void DrawRanking(void) {
    ClearBackground((Color){9,13,22,255});
    DrawText("VECTOR SEARCH // RANKING",80,50,37,SKYBLUE);
    DrawText("LOCAL (persistente neste computador)",72,116,22,GOLD);
    DrawText("GLOBAL (servidor compartilhado)",685,116,22,GOLD);
    if(!onlineAvailable) {
        DrawText("SEM CONEXAO COM SERVIDOR GLOBAL",686,172,18,BAD);
        DrawText("Defina VECTOR_SEARCH_RANKING_URL",686,206,17,LIGHTGRAY);
        DrawText("e hospede leaderboard_server.py.",686,236,17,LIGHTGRAY);
        DrawText("Um ranking local NAO equivale a ranking mundial.",686,267,16,GRAY);
    }
    for(int i=0;i<MinF(localScoreCount,12);i++) {
        char time[32];TimeString(localScores[i].ms/1000.0f,time);
        DrawText(TextFormat("%2d. %-20s %s",i+1,localScores[i].name,time),75,164+i*33,17,LIGHTGRAY);
    }
    if(onlineAvailable)for(int i=0;i<MinF(onlineScoreCount,12);i++) {
        char time[32];TimeString(onlineScores[i].ms/1000.0f,time);
        DrawText(TextFormat("%2d. %-20s %s",i+1,onlineScores[i].name,time),685,164+i*33,17,LIGHTGRAY);
    }
    DrawText("R: ATUALIZAR ONLINE      ESC: VOLTAR",75,646,20,SKYBLUE);
}
// Botoes reais para uso com o mouse; atalhos de teclado continuam funcionando.
static bool UIButton(const char *label,int x,int y,int w,int h,Color color) {
    Vector2 m=GetMousePosition();
    bool hovered=m.x>=x && m.x<x+w && m.y>=y && m.y<y+h;
    DrawRectangle(x,y,w,h,hovered?Shade(color,1.35f):color);
    DrawRectangleLines(x,y,w,h,WHITE);
    DrawText(label,x+15,y+(h-22)/2,22,WHITE);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
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
    signal(SIGPIPE,SIG_IGN);
    InitWindow(SCREEN_W,SCREEN_H,"VECTOR SEARCH // 1.4");
    SetExitKey(KEY_NULL);SetTargetFPS(60);
    FindAvatars();RestoreAvatarChoice();
    Model avatar={0};bool avatarLoaded=LoadAvatar(&avatar,SelectedAvatarPath());
    RenderTexture2D menuPreview=LoadRenderTexture(PREVIEW_W,PREVIEW_H);
    RenderTexture2D keyPreview=LoadRenderTexture(KEYCAM_W,KEYCAM_H);
    recorder.target=LoadRenderTexture(VIDEO_W,VIDEO_H);
    recorder.targetReady=recorder.target.id!=0;
    bool keyCameraEnabled=true;
    bool menu=true,playing=false,helpScreen=false,winScreen=false,rankingScreen=false;
    bool grounded=true,carrying=false;
    Vector3 player={0};float yaw=0,pitch=0,vy=0;
    LoadLocalScores();
    Camera3D camera={0};camera.up=(Vector3){0,1,0};camera.fovy=75;camera.projection=CAMERA_PERSPECTIVE;
    while(!WindowShouldClose()) {
        float frameDt=GetFrameTime();
        float dt=MinF(frameDt,1.0f/30.0f);
        if(messageTime>0) messageTime-=dt;
        if(rankingScreen) {
            EnableCursor();
            if(IsKeyPressed(KEY_ESCAPE)) {rankingScreen=false;menu=true;}
            if(IsKeyPressed(KEY_R)) FetchOnlineScores();
            BeginDrawing();DrawRanking();
            if(UIButton("VOLTAR",1025,628,170,45,(Color){41,68,92,255})){
                rankingScreen=false;menu=true;
            }
            EndDrawing();continue;
        }
        if(winScreen) {
            EnableCursor();
            if(!scoreSubmitted) {
                int ch;
                while((ch=GetCharPressed())>0) {
                    if(winnerNameLen<20 && ((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||
                        (ch>='0'&&ch<='9')||ch=='_'||ch=='-')) {
                        winnerName[winnerNameLen++]=(char)ch;winnerName[winnerNameLen]=0;
                    }
                }
                if(IsKeyPressed(KEY_BACKSPACE) && winnerNameLen>0)winnerName[--winnerNameLen]=0;
                if(IsKeyPressed(KEY_ENTER))SubmitVictoryScore();
            }
            if(scoreSubmitted && IsKeyPressed(KEY_G)){
                LoadLocalScores();FetchOnlineScores();winScreen=false;rankingScreen=true;
            }
            if((scoreSubmitted && IsKeyPressed(KEY_M))||IsKeyPressed(KEY_ESCAPE)){
                if(!scoreSubmitted)SubmitVictoryScore();
                winScreen=false;menu=true;
            }
            BeginDrawing();DrawWin(scoreSubmitted);
            if(!scoreSubmitted && UIButton("REGISTRAR",720,385,200,55,(Color){40,93,65,255}))
                SubmitVictoryScore();
            if(scoreSubmitted && UIButton("RANKING",700,525,200,60,(Color){45,82,121,255})){
                LoadLocalScores();FetchOnlineScores();winScreen=false;rankingScreen=true;
            }
            if(UIButton("MENU",950,525,165,60,(Color){73,69,88,255})){
                if(!scoreSubmitted)SubmitVictoryScore();
                winScreen=false;menu=true;
            }
            EndDrawing();continue;
        }
        if(helpScreen) {
            EnableCursor();
            if(IsKeyPressed(KEY_ENTER)) {helpScreen=false;playing=true;DisableCursor();
                if(!RecorderStart())Note("SEM VIDEO: ffmpeg indisponivel ou pasta inacessivel.");}
            if(IsKeyPressed(KEY_ESCAPE)){helpScreen=false;menu=true;}
            BeginDrawing();DrawInstructions();
            if(UIButton("JOGAR",275,602,180,56,(Color){32,92,71,255})){
                helpScreen=false;playing=true;DisableCursor();
                if(!RecorderStart())Note("SEM VIDEO: confira ffmpeg e pasta Videos/Videos.");
            }
            if(UIButton("VOLTAR",940,602,190,56,(Color){76,56,70,255})){
                helpScreen=false;menu=true;
            }
            EndDrawing();continue;
        }
        if(menu) {
            EnableCursor();
            if(IsKeyPressed(KEY_P)) {
                RefreshAvatars(&avatar,&avatarLoaded);
                Note(avatarLoaded?"Pasta de modelos atualizada.":"Nenhum OBJ valido: boneco reserva.");
            }
            if(IsKeyPressed(KEY_LEFT)) ChangeAvatar(-1,&avatar,&avatarLoaded);
            if(IsKeyPressed(KEY_RIGHT)) ChangeAvatar(1,&avatar,&avatarLoaded);
            if(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_N)) {
                NewGame(seedFromArgs?argSeed:NewSeed(),&player,&vy,&yaw,&pitch,&carrying);
                menu=false;helpScreen=true;playing=false;EnableCursor();
            }
            if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_L)) {
                if(LoadGame(&player,&yaw,&pitch,&vy,&carrying)) {grounded=false;menu=false;helpScreen=true;playing=false;EnableCursor();Note("Ultimo save carregado.");
                } else Note("Save inexistente, danificado ou de outra versao.");
            }
            if(IsKeyPressed(KEY_G)){
                LoadLocalScores();FetchOnlineScores();menu=false;rankingScreen=true;
            }
            RenderMenuPreview(menuPreview,&avatar,avatarLoaded);
            BeginDrawing();ClearBackground((Color){7,12,20,255});
            DrawText("VECTOR SEARCH 1.4",64,95,46,(Color){90,190,245,255});
            DrawText("SEIS CHAVES // 18 VIRUS // QUARENTENA",70,162,24,GOLD);
            DrawText(TextFormat("MODELO: %s  [%d/%d]",SelectedAvatarName(),avatarCount?selectedAvatar+1:0,avatarCount),80,218,21,SKYBLUE);
            DrawText("SETAS ESQUERDA/DIREITA: TROCAR AVATAR",80,250,18,SKYBLUE);
            DrawText("1 / N - NOVO MUNDO",80,294,28,LIGHTGRAY);
            DrawText("2 / L - CARREGAR ULTIMO SAVE",80,354,25,LIGHTGRAY);
            DrawText("P - REESCANEAR MODELOS",80,419,20,SKYBLUE);
            DrawText("G - RANKING LOCAL / GLOBAL",80,455,19,GOLD);
            DrawText("100 x 100 x 36 | 6 andares | 10x10 salas em cada andar",80,483,18,GRAY);
            DrawText("O OBJ nao muda a colisao (1,65 m).",80,516,18,GRAY);
            DrawText("MP4 automatico: Videos/Videos (camera ortografica).",80,542,16,SKYBLUE);
            DrawTexturePanel(menuPreview.texture,688,150,PREVIEW_W,PREVIEW_H);
            DrawText("PREVIA DO JOGADOR NA SALA | REGUA = 1,65 m",688,568,16,GOLD);
            if(avatarCount>0) DrawText(TextFormat("OBJ: %s",availableAvatars[selectedAvatar].path),688,589,14,LIGHTGRAY);
            if(UIButton("<",690,614,54,45,(Color){29,68,100,255})) ChangeAvatar(-1,&avatar,&avatarLoaded);
            if(UIButton(">",1151,614,54,45,(Color){29,68,100,255})) ChangeAvatar(1,&avatar,&avatarLoaded);
            DrawText("ESCOLHER MODELO",787,624,21,SKYBLUE);
            if(!avatarLoaded) DrawText("OBJ NAO CARREGADO; BONECO RESERVA",688,672,16,BAD);
            if(messageTime>0) DrawText(message,70,640,18,GOLD);
            if(UIButton("NOVO",80,563,160,50,(Color){31,98,70,255})){
                NewGame(seedFromArgs?argSeed:NewSeed(),&player,&vy,&yaw,&pitch,&carrying);
                menu=false;helpScreen=true;playing=false;
            }
            if(UIButton("CARREGAR",255,563,175,50,(Color){36,73,106,255})){
                if(LoadGame(&player,&yaw,&pitch,&vy,&carrying)){
                    grounded=false;menu=false;helpScreen=true;playing=false;
                }else Note("Save inexistente ou invalido.");
            }
            if(UIButton("RANKING",445,563,175,50,(Color){93,76,44,255})){
                LoadLocalScores();FetchOnlineScores();menu=false;rankingScreen=true;
            }
            EndDrawing();continue;
        }
        if(!playing) continue;
        if(IsKeyPressed(KEY_ESCAPE)) {RecorderStop();menu=true;playing=false;mapOpen=false;continue;}
        if(!confused && IsKeyPressed(KEY_TAB)) {
            mapOpen=!mapOpen;
            if(mapOpen) {mapLayer=LayerOf(player.y);EnableCursor();}
            else DisableCursor();
        }
        // Salvar funciona tanto durante a exploracao quanto com o mapa aberto.
        if((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S))
            Note(SaveGame(player,yaw,pitch,vy,carrying)?"Mundo salvo em vector_search_v11_save.dat":"Erro ao salvar. Confira permissoes de escrita.");
        if(mapOpen) {
            float wheel=GetMouseWheelMove();
            if(wheel>0) mapLayer=(mapLayer+1)%LEVEL_COUNT;
            if(wheel<0) mapLayer=(mapLayer+LEVEL_COUNT-1)%LEVEL_COUNT;
            BeginDrawing();ClearBackground(BLACK);DrawMap();EndDrawing();continue;
        }
        if(IsKeyPressed(KEY_Q)) keyCameraEnabled=!keyCameraEnabled;
        if(IsKeyPressed(KEY_F2)) {
            RecorderStop();
            NewGame((IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT))?worldSeed:NewSeed(),&player,&vy,&yaw,&pitch,&carrying);
            grounded=true;
            RecorderStart();
        }
        runSeconds+=MaxF(0.0f,frameDt);
        if(IsKeyPressed(KEY_E))UseBlue(&player,&vy,&grounded);
        Vector2 delta=GetMouseDelta();yaw+=delta.x*0.0025f;
        pitch=ClampF(pitch-delta.y*0.0025f,-1.45f,1.45f);
        currentLayer=LayerOf(player.y);
        RebuildNearby(player);
        float fw=(float)(IsKeyDown(KEY_W)-IsKeyDown(KEY_S));
        float side=(float)(IsKeyDown(KEY_D)-IsKeyDown(KEY_A));
        float dx=sinf(yaw)*fw+cosf(yaw)*side;
        float dz=-cosf(yaw)*fw+sinf(yaw)*side;
        float len=sqrtf(dx*dx+dz*dz);if(len>0) {dx/=len;dz/=len;}
        float speed=(IsKeyDown(KEY_LEFT_SHIFT)?7.5f:4.5f)*(buffSeconds>0?1.35f:1.0f);
        if(IsKeyPressed(KEY_SPACE) && grounded) {vy=JUMP_SPEED*(buffSeconds>0?1.28f:1.0f);grounded=false;}
        MovePlayerSolid(&player,dx*speed*dt,dz*speed*dt,&vy,dt,&grounded);
        currentLayer=LayerOf(player.y);
        // A subida ocorre pulando pelos degraus coloridos ao redor do poco.
        MarkVisited(player);
        TryCollectBuff(player);
        TryCollectBlue(player);
        if(buffSeconds>0)buffSeconds=MaxF(0,buffSeconds-dt);
        UpdateHunter(dt,player);
        bool caught=false;
        UpdateStalker(dt,player,&caught);
        UpdateDisorienter(dt,frameDt,&player,&vy,&grounded);
        UpdateViruses(dt,player);
        // Mesmo ao pular ou atravessar uma porta, o virus acompanha o jogador.
        CarryVirus(player,yaw);
        if(caught) {
            // Cacador apaga o progresso da partida EM MEMORIA, nao o save manual.
            // Reinicia com a mesma seed para que a partida seja reproduzivel.
            uint32_t deathSeed=worldSeed;
            RecorderStop();
            NewGame(deathSeed,&player,&vy,&yaw,&pitch,&carrying);
            RecorderStart();
            grounded=true;
            stalkerGrace=5.0f;
            Note("O CACADOR APAGOU VOCE! Partida reiniciada; save manual preservado.");
            RebuildNearby(player);
        }
        if(IsKeyPressed(KEY_F)) {
            InteractKeys(player);
            if(InsertedKeyCount()==6 && virusesCaptured==VIRUS_TOTAL) {
                RecorderFrame(player,yaw,&avatar,avatarLoaded,frameDt);
                RecorderStop();
                winScreen=true;playing=false;mapOpen=false;
                winnerNameLen=0;winnerName[0]=0;scoreSubmitted=false;
                EnableCursor();
                continue;
            }
        }
        if(InsertedKeyCount()==6 && virusesCaptured==VIRUS_TOTAL && !caught){
            RecorderFrame(player,yaw,&avatar,avatarLoaded,frameDt);
            RecorderStop();
            winScreen=true;playing=false;mapOpen=false;
            winnerNameLen=0;winnerName[0]=0;scoreSubmitted=false;
            EnableCursor();continue;
        }
        if(IsKeyPressed(KEY_B)) DropBreadcrumb(player,grounded);
        camera.position=(Vector3){player.x,player.y+EYE_H,player.z};
        camera.target=(Vector3){player.x+cosf(pitch)*sinf(yaw),player.y+EYE_H+sinf(pitch),player.z-cosf(pitch)*cosf(yaw)};
        RebuildNearby(player);
        Ray ray=AimRay(camera);RayCollision hit={0};int selected=RayBlock(ray,&hit);
        Block preview={0};bool canPlace=false;
        if(carrying) canPlace=SuggestPlacement(ray,hit,selected,player,&preview);
        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){
            int virusSelected=(grabbedVirus<0)?VirusInSight(ray,player):-1;
            if(grabbedVirus>=0){
                if(ThrowVirus(player,yaw))RebuildNearby(player);
            }else if(virusSelected>=0){
                grabbedVirus=virusSelected;
                viruses[grabbedVirus].containedFor=0;
                viruses[grabbedVirus].rerouteTime=0;
                CarryVirus(player,yaw);
                Note("Virus agarrado! Leve-o a outra sala. Clique direito para lancar.");
            }
        }
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
        RecorderFrame(player,yaw,&avatar,avatarLoaded,frameDt);
        if(keyCameraEnabled && !confused) RenderKeyCamera(keyPreview,player,yaw,&avatar,avatarLoaded,watchedKey);
        BeginDrawing();ClearBackground(BLACK);BeginMode3D(camera);
        // Renderizacao: somente a sala atual preenchida; ao cruzar uma
        // laje, duas salas verticais do MESMO (cx,cz). Laterais aramadas
        // apenas quando nao se esta na transicao.
        // Recalcula a cada frame a partir do volume REAL do personagem.
        // Nao depende de pulo, grounded ou salas anteriores (sem ficar preso
        // no modo transicao). Somente os andares tocados pelo corpo aparecem.
        int lowLayer=LayerOf(player.y+0.04f);
        int highLayer=LayerOf(player.y+PLAYER_HEIGHT-0.04f);
        if(lowLayer<0)lowLayer=0;
        if(highLayer>=LEVEL_COUNT)highLayer=LEVEL_COUNT-1;
        if(highLayer<lowLayer)highLayer=lowLayer;
        bool betweenFloors=highLayer>lowLayer;
        for(int layer=lowLayer;layer<=highLayer;layer++){
            if(layer==ly || betweenFloors){
                DrawFilledRoom(cx,cz,layer,true);
                DrawBreadcrumbs(cx,cz,layer);
                DrawBuff(cx,cz,layer);
                DrawBlue(cx,cz,layer);
                DrawArtifacts(cx,cz,layer);
                DrawViruses(cx,cz,layer);
            }
        }
        if(!betweenFloors){
            for(int z=cz-RENDER_RADIUS;z<=cz+RENDER_RADIUS;z++)
            for(int x=cx-RENDER_RADIUS;x<=cx+RENDER_RADIUS;x++){
                if(x==cx&&z==cz)continue;
                DrawChunkEnvironment(x,z,ly,cx,cz,ly);
                if(InGrid(x)&&InGrid(z)){
                    DrawBreadcrumbs(x,z,ly);
                    DrawArtifacts(x,z,ly);
                    DrawViruses(x,z,ly);
                }
            }
        }
        for(int i=0;i<nearbyCount;i++) {
            Block b=nearby[i];
            if(abs(ChunkOf(b.pos.x)-cx)>RENDER_RADIUS || abs(ChunkOf(b.pos.z)-cz)>RENDER_RADIUS) continue;
            if(betweenFloors && (ChunkOf(b.pos.x)!=cx || ChunkOf(b.pos.z)!=cz)) continue;
            int blockLayer=LayerOf(b.pos.y);
            bool visible=blockLayer>=lowLayer && blockLayer<=highLayer;
            bool crosses=(b.slot>=100 && b.placedIndex<0 &&
                blockLayer==lowLayer-1 && b.pos.y+b.size.y/2>=FloorY(lowLayer)-0.05f);
            if(!visible && !crosses)continue;
            bool inRoom=(ChunkOf(b.pos.x)==cx && ChunkOf(b.pos.z)==cz);
            Color color=(!carrying&&selected==i)?HIGHLIGHT:BlockColor(b);
            if(inRoom) DrawCube(b.pos,b.size.x,b.size.y,b.size.z,color);
            else DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,DIM);
            if(inRoom && !carrying && selected==i)
                DrawCubeWires(b.pos,b.size.x,b.size.y,b.size.z,WHITE);
        }
        if(carrying) {
            if(preview.size.x>0.5f){
                Color ghost=canPlace?(Color){35,145,205,155}:(Color){220,47,43,150};
                DrawCube(preview.pos,preview.size.x,preview.size.y,preview.size.z,ghost);
                DrawCubeWires(preview.pos,preview.size.x,preview.size.y,preview.size.z,
                              canPlace?HIGHLIGHT:BAD);
            }
            Vector3 hand=Vector3Add(ray.position,Vector3Scale(ray.direction,1.35f));
            DrawCubeWires(hand,0.25f,0.25f,0.25f,HIGHLIGHT);
        }
        if(hunterSpawned && hunterLayer>=lowLayer && hunterLayer<=highLayer && (!betweenFloors || (ChunkOf(hunterPos.x)==cx && ChunkOf(hunterPos.z)==cz)) && ChunkOf(hunterPos.x)>=cx-RENDER_RADIUS &&
           ChunkOf(hunterPos.x)<=cx+RENDER_RADIUS &&
           ChunkOf(hunterPos.z)>=cz-RENDER_RADIUS &&
           ChunkOf(hunterPos.z)<=cz+RENDER_RADIUS)DrawHunter();
        if(stalkerSpawned && stalkerLayer>=lowLayer && stalkerLayer<=highLayer &&
           (!betweenFloors || (ChunkOf(stalkerPos.x)==cx && ChunkOf(stalkerPos.z)==cz)) &&
           abs(ChunkOf(stalkerPos.x)-cx)<=RENDER_RADIUS &&
           abs(ChunkOf(stalkerPos.z)-cz)<=RENDER_RADIUS)DrawStalker();
        if(disorienterActive && (!betweenFloors || (ChunkOf(disorienterPos.x)==cx && ChunkOf(disorienterPos.z)==cz)) && LayerOf(disorienterPos.y)>=lowLayer &&
           LayerOf(disorienterPos.y)<=highLayer &&
           abs(ChunkOf(disorienterPos.x)-cx)<=RENDER_RADIUS &&
           abs(ChunkOf(disorienterPos.z)-cz)<=RENDER_RADIUS) DrawDisorienter();
        if(!confused)DrawHubArrow(camera,player,yaw,pitch);
        EndMode3D();
        if(!confused) {
        DrawText("VECTOR SEARCH 1.4 | WASD | ESPACO | SHIFT | ESQ BLOCO",16,16,17,NEON);
        DrawText("TAB MAPA | DIREITO AGARRAR/LANCAR VIRUS | B MIGALHA | F2 RESET",16,42,16,NEON);
        DrawText("CTRL+S SALVAR | E AZUL | F CHAVE | V CANCELAR | Q CAMERA",16,67,16,SKYBLUE);
        DrawText(floorIdentityLost?
                 TextFormat("SEED %u | ANDAR ??? | CHAVES %d/6 | MOCHILA %d/2 | HEX %d/6 | VIRUS %d/18",
                   worldSeed,totalKeys,CarriedKeyCount(),InsertedKeyCount(),virusesCaptured):
                 TextFormat("SEED %u | ANDAR %d/5 | CHAVES %d/6 | MOCHILA %d/2 | HEX %d/6 | VIRUS %d/18",
                   worldSeed,ly,totalKeys,CarriedKeyCount(),InsertedKeyCount(),virusesCaptured),16,94,18,GOLD);
        int buffCx,buffCz;BuffRoom(ly,&buffCx,&buffCz);
        Vector3 buffP=BuffPosition(ly);
        DrawText(TextFormat("BONUS LARANJA %s: CHUNK (%d,%d) | DIST %.0fm | ESCUDO: %.0fs",
                 buffClaimed[ly]?"COLETADO":"DISPONIVEL",buffCx,buffCz,
                 Vector3Distance((Vector3){player.x,FloorY(ly),player.z},buffP),buffSeconds),
                 16,126,16,(Color){255,174,86,255});
        if(messageTime>0) DrawText(message,16,153,18,GOLD);
        DrawText(blueStored?"AZUL NO INVENTARIO: E PARA TELEPORTAR (PERDA PERMANENTE DE ANDAR)":
                 "BONUS AZUL: ENCONTRE O CUBO MARCADO NO MAPA",16,180,17,SKYBLUE);
        if(keyCameraEnabled) {
            int px=SCREEN_W-KEYCAM_W-16,py=SCREEN_H-KEYCAM_H-47;
            DrawTexturePanel(keyPreview.texture,px,py,KEYCAM_W,KEYCAM_H);
            DrawRectangle(px,py-25,KEYCAM_W,25,(Color){13,20,30,240});
            DrawText(watchedKey>=0?"CAMERA AUXILIAR | SALA DA CHAVE MAIS PROXIMA":
                                      "CAMERA AUXILIAR | HEXAGONO CENTRAL",px+10,py-20,15,GOLD);
        }
        } // fim da HUD: durante 25 s de confusao tudo desaparece
        if(!confused) {
            DrawLine(SCREEN_W/2-6,SCREEN_H/2,SCREEN_W/2+6,SCREEN_H/2,NEON);
            DrawLine(SCREEN_W/2,SCREEN_H/2-6,SCREEN_W/2,SCREEN_H/2+6,NEON);
        }
        EndDrawing();
    }
    RecorderStop();
    if(recorder.targetReady)UnloadRenderTexture(recorder.target);
    UnloadRenderTexture(menuPreview);
    UnloadRenderTexture(keyPreview);
    if(avatarLoaded) UnloadModel(avatar);
    CloseWindow();return 0;
}
