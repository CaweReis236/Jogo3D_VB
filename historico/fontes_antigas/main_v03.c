#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define CHUNK_SIZE 12.0f
#define HALF_CHUNK (CHUNK_SIZE / 2.0f)
#define RENDER_RADIUS 2
#define WALL_H 3.5f
#define WALL_T 0.18f
#define DOOR_HALF 1.6f
#define PLAYER_RADIUS 0.30f
#define EYE_H 1.7f
#define GRAVITY 19.0f
#define JUMP_SPEED 7.0f

// A seed pertence ao mundo, nao a uma constante de compilacao.
static uint32_t worldSeed = 0;

// Raylib fornece sorteio independente do algoritmo deterministico dos chunks.
static uint32_t NovaSeed(void)
{
    uint32_t a = (uint32_t)GetRandomValue(0, 65535);
    uint32_t b = (uint32_t)GetRandomValue(0, 65535);
    return (a << 16) | b;
}

// Volta ao centro, ao nivel do chao, e reinicia a camera e o pulo.
static void ReiniciarJogador(Vector3 *jogador, float *velocidadeY,
                            float *yaw, float *pitch)
{
    *jogador = (Vector3){0, 0, 0};
    *velocidadeY = 0.0f;
    *yaw = 0.0f;
    *pitch = 0.0f;
}

typedef struct {
    float x, z;
    float w, d, h;
} Caixa;

static uint32_t Misturar(uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v;
}

static uint32_t HashChunk(int cx, int cz, int slot)
{
    return Misturar((uint32_t)cx * 73856093u ^
                    (uint32_t)cz * 19349663u ^
                    (uint32_t)(slot + 1) * 83492791u ^ worldSeed);
}

static int CoordenadaChunk(float pos)
{
    return (int)floorf((pos + HALF_CHUNK) / CHUNK_SIZE);
}

// Gera até quatro caixas em posições que não bloqueiam as portas.
static int GerarCaixas(int cx, int cz, Caixa caixas[4])
{
    int qtd = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t h = HashChunk(cx, cz, i);
        if (h % 100u >= 65u) continue;

        float desvioX = ((float)((h >> 8) & 255u) / 255.0f - 0.5f) * 0.8f;
        float desvioZ = ((float)((h >> 16) & 255u) / 255.0f - 0.5f) * 0.8f;

        Caixa b = {
            .x = cx * CHUNK_SIZE + ((i % 2) ? 3.0f : -3.0f) + desvioX,
            .z = cz * CHUNK_SIZE + ((i / 2) ? 3.0f : -3.0f) + desvioZ,
            .w = 1.0f + (float)((h >> 5) & 7u) * 0.12f,
            .d = 1.0f + (float)((h >> 12) & 7u) * 0.12f,
            .h = 0.8f + (float)((h >> 20) & 7u) * 0.18f
        };
        caixas[qtd++] = b;
    }
    return qtd;
}

// Colisão circular do jogador contra um retângulo no plano XZ.
static bool TocouRetangulo(float px, float pz, float raio,
                          float x0, float z0, float x1, float z1)
{
    float x = fmaxf(x0, fminf(px, x1));
    float z = fmaxf(z0, fminf(pz, z1));
    float dx = px - x;
    float dz = pz - z;
    return dx * dx + dz * dz < raio * raio;
}

static bool Colidiu(float px, float pz)
{
    int cx = CoordenadaChunk(px);
    int cz = CoordenadaChunk(pz);

    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            int xchunk = cx + dx;
            int zchunk = cz + dz;
            float mx = xchunk * CHUNK_SIZE;
            float mz = zchunk * CHUNK_SIZE;
            float oeste = mx - HALF_CHUNK;
            float norte = mz - HALF_CHUNK;

            // Parede oeste, com uma abertura no centro.
            if (TocouRetangulo(px, pz, PLAYER_RADIUS,
                oeste - WALL_T/2, norte,
                oeste + WALL_T/2, mz - DOOR_HALF) ||
                TocouRetangulo(px, pz, PLAYER_RADIUS,
                oeste - WALL_T/2, mz + DOOR_HALF,
                oeste + WALL_T/2, mz + HALF_CHUNK)) return true;

            // Parede norte, também com passagem.
            if (TocouRetangulo(px, pz, PLAYER_RADIUS,
                oeste, norte - WALL_T/2,
                mx - DOOR_HALF, norte + WALL_T/2) ||
                TocouRetangulo(px, pz, PLAYER_RADIUS,
                mx + DOOR_HALF, norte - WALL_T/2,
                mx + HALF_CHUNK, norte + WALL_T/2)) return true;

            Caixa caixas[4];
            int qtd = GerarCaixas(xchunk, zchunk, caixas);
            for (int i = 0; i < qtd; i++) {
                Caixa b = caixas[i];
                if (TocouRetangulo(px, pz, PLAYER_RADIUS,
                    b.x - b.w/2, b.z - b.d/2,
                    b.x + b.w/2, b.z + b.d/2)) return true;
            }
        }
    }
    return false;
}

static void DesenharChunk(int cx, int cz)
{
    float mx = cx * CHUNK_SIZE;
    float mz = cz * CHUNK_SIZE;
    float oeste = mx - HALF_CHUNK;
    float norte = mz - HALF_CHUNK;

    Color verde = (Color){0, 240, 70, 255};
    Color grade = (Color){0, 85, 27, 255};

    // Grade no piso: 1 quadrado por unidade.
    for (int i = 0; i <= (int)CHUNK_SIZE; i++) {
        float t = (float)i;
        DrawLine3D((Vector3){oeste + t, 0, norte},
                   (Vector3){oeste + t, 0, norte + CHUNK_SIZE}, grade);
        DrawLine3D((Vector3){oeste, 0, norte + t},
                   (Vector3){oeste + CHUNK_SIZE, 0, norte + t}, grade);
    }

    // Apenas paredes oeste e norte: evitamos desenhá-las duas vezes.
    float tam = HALF_CHUNK - DOOR_HALF;
    DrawCubeWires((Vector3){oeste, WALL_H/2, mz - (HALF_CHUNK + DOOR_HALF)/2},
                  WALL_T, WALL_H, tam, verde);
    DrawCubeWires((Vector3){oeste, WALL_H/2, mz + (HALF_CHUNK + DOOR_HALF)/2},
                  WALL_T, WALL_H, tam, verde);
    DrawCubeWires((Vector3){mx - (HALF_CHUNK + DOOR_HALF)/2, WALL_H/2, norte},
                  tam, WALL_H, WALL_T, verde);
    DrawCubeWires((Vector3){mx + (HALF_CHUNK + DOOR_HALF)/2, WALL_H/2, norte},
                  tam, WALL_H, WALL_T, verde);

    Caixa caixas[4];
    int qtd = GerarCaixas(cx, cz, caixas);
    for (int i = 0; i < qtd; i++) {
        Caixa b = caixas[i];
        DrawCubeWires((Vector3){b.x, b.h/2, b.z}, b.w, b.h, b.d, verde);
    }
}

int main(int argc, char **argv)
{
    // Opcional: ./simulador 123456  => recria sempre o mesmo mundo.
    bool seedInformada = (argc >= 2);
    if (seedInformada) {
        char *fim = NULL;
        errno = 0;
        unsigned long long valor = strtoull(argv[1], &fim, 10);
        if (errno != 0 || fim == argv[1] || *fim != '\0' || valor > UINT32_MAX || argv[1][0] == '-') {
            fprintf(stderr, "Seed invalida. Use um numero de 0 a 4294967295.\n");
            return 1;
        }
        worldSeed = (uint32_t)valor;
    }

    InitWindow(SCREEN_W, SCREEN_H, "SIMULADOR VETORIAL - CHUNKS");
    if (!seedInformada) worldSeed = NovaSeed();
    printf("Seed atual: %u\n", (unsigned)worldSeed);
    SetTargetFPS(60);
    DisableCursor();

    Vector3 jogador = {0, 0, 0}; // y = altura dos pés
    float velocidadeY = 0.0f;
    float yaw = 0.0f, pitch = 0.0f;

    Camera3D camera = {0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 1.0f/30.0f);

        // R = mundo novo; Shift+R = volta ao inicio mantendo a seed.
        if (IsKeyPressed(KEY_R)) {
            if (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) {
                uint32_t antiga = worldSeed;
                do { worldSeed = NovaSeed(); } while (worldSeed == antiga);
            }
            ReiniciarJogador(&jogador, &velocidadeY, &yaw, &pitch);
            printf("Seed atual: %u\n", (unsigned)worldSeed);
        }

        Vector2 mouse = GetMouseDelta();
        yaw += mouse.x * 0.0025f;
        pitch -= mouse.y * 0.0025f;
        pitch = fmaxf(-1.45f, fminf(1.45f, pitch));

        float frente = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        float lado = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        float movX = sinf(yaw)*frente + cosf(yaw)*lado;
        float movZ = -cosf(yaw)*frente + sinf(yaw)*lado;
        float comprimento = sqrtf(movX*movX + movZ*movZ);
        if (comprimento > 0.0f) {
            movX /= comprimento;
            movZ /= comprimento;
        }

        float velocidade = IsKeyDown(KEY_LEFT_SHIFT) ? 7.5f : 4.5f;
        float novoX = jogador.x + movX * velocidade * dt;
        if (!Colidiu(novoX, jogador.z)) jogador.x = novoX;
        float novoZ = jogador.z + movZ * velocidade * dt;
        if (!Colidiu(jogador.x, novoZ)) jogador.z = novoZ;

        if (IsKeyPressed(KEY_SPACE) && jogador.y <= 0.0f)
            velocidadeY = JUMP_SPEED;
        velocidadeY -= GRAVITY * dt;
        jogador.y += velocidadeY * dt;
        if (jogador.y < 0.0f) {
            jogador.y = 0.0f;
            velocidadeY = 0.0f;
        }

        camera.position = (Vector3){jogador.x, jogador.y + EYE_H, jogador.z};
        camera.target = (Vector3){
            camera.position.x + cosf(pitch)*sinf(yaw),
            camera.position.y + sinf(pitch),
            camera.position.z - cosf(pitch)*cosf(yaw)
        };

        int cx = CoordenadaChunk(jogador.x);
        int cz = CoordenadaChunk(jogador.z);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        for (int z = cz - RENDER_RADIUS; z <= cz + RENDER_RADIUS; z++)
            for (int x = cx - RENDER_RADIUS; x <= cx + RENDER_RADIUS; x++)
                DesenharChunk(x, z);
        EndMode3D();

        Color verde = (Color){0, 255, 70, 255};
        DrawText("VECTOR SYSTEM // CHUNKS", 20, 20, 20, verde);
        DrawText("WASD andar | Mouse olhar | ESPACO pular | SHIFT correr | ESC sair", 20, 50, 16, verde);
        DrawText("R novo mundo | SHIFT+R reiniciar com a mesma seed", 20, 75, 16, verde);
        DrawText(TextFormat("SEED: %u  |  CHUNK (%d, %d)  |  X %.1f Z %.1f  |  FPS %d",
                            (unsigned)worldSeed, cx, cz, jogador.x, jogador.z, GetFPS()), 20, 100, 16, verde);
        DrawLine(SCREEN_W/2 - 5, SCREEN_H/2, SCREEN_W/2 + 5, SCREEN_H/2, verde);
        DrawLine(SCREEN_W/2, SCREEN_H/2 - 5, SCREEN_W/2, SCREEN_H/2 + 5, verde);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
