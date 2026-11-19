#include <raylib.h>
#include <raymath.h>

#define LARGURA 1280
#define ALTURA 720

int main(void)
{
    InitWindow(LARGURA, ALTURA, "SIMULADOR VETORIAL");

    SetTargetFPS(60);
    DisableCursor();

    Camera3D camera = {0};

    camera.position = (Vector3){0, 1.7f, 4};
    camera.target = (Vector3){0, 1.7f, 0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 70;
    camera.projection = CAMERA_PERSPECTIVE;

    Color verde = {0, 255, 65, 255};
    Color verdeEscuro = {0, 100, 25, 255};

    while (!WindowShouldClose())
    {
        // Movimentação em primeira pessoa
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);

        // Estrutura do quarto
        DrawCubeWires(
            (Vector3){0, 2, 0},
            10, 4, 10,
            verde
        );

        // Chão quadriculado
        for (int i = -5; i <= 5; i++)
        {
            DrawLine3D(
                (Vector3){(float)i, 0, -5},
                (Vector3){(float)i, 0, 5},
                verdeEscuro
            );

            DrawLine3D(
                (Vector3){-5, 0, (float)i},
                (Vector3){5, 0, (float)i},
                verdeEscuro
            );
        }

        // Mesa
        DrawCubeWires(
            (Vector3){0, 0.75f, -2},
            2, 1.5f, 1,
            verde
        );

        // Caixa
        DrawCubeWires(
            (Vector3){3, 0.5f, -3},
            1, 1, 1,
            verde
        );

        EndMode3D();

        // Interface
        DrawText("VECTOR SYSTEM v0.1", 20, 20, 20, verde);
        DrawText("WASD - MOVIMENTO", 20, 50, 15, verde);
        DrawText("MOUSE - CAMERA", 20, 70, 15, verde);
        DrawText("ESC - SAIR", 20, 90, 15, verde);

        DrawFPS(20, 120);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
