SIMULADOR VETORIAL 0.5 - salas coloridas e blocos azuis
=======================================================

Requisitos:
- Linux Mint / Linux X11 com Raylib 6.x instalada
- GCC, Make, pkg-config

Compilar:
    make

Executar:
    ./simulador

Executar com uma seed especifica:
    ./simulador 123456

Controles:
    WASD                mover
    MOUSE               olhar
    ESPACO              pular
    SHIFT               correr
    CLIQUE ESQUERDO     pegar / colocar bloco
    R                   nova seed e reinicio
    SHIFT + R           reinicio com a mesma seed
    ESC                 sair

Recursos:
- Mundo procedural deterministico e chunks
- Blocos de contornos azuis, interativos
- Sala atual com piso e paredes solidos de cores suaves
- Outras salas exibidas como grade verde
- Colisoes e gravidade
- As alteracoes persistem durante a sessao, nao sao salvas em disco

OBS.: Como sua Raylib foi compilada estaticamente, o Makefile adiciona
explicitamente as dependencias graficas do X11 (inclusive -lX11).
