# Vector Search 1.2 — jogo procedural e replay cinematográfico

Requer Linux Mint, Raylib 6.2 instalada, `gcc`, `make`, `pkg-config`, `ffmpeg` e opcionalmente `xdg-user-dir`.

## Instalar

```bash
sudo apt install ffmpeg xdg-user-dirs
cd ~/simulador3d
cp main.c main_v10_backup.c
unzip -o ~/Downloads/vector_search_v12.zip
cp vector_search_v12/main.c main.c
cp vector_search_v12/Makefile Makefile
mkdir -p assets
# Instala os 3 arquivos de exemplo APENAS se nao houver seu OBJ:
if [ ! -f assets/player.obj ]; then
  cp vector_search_v12/assets/player.obj assets/player.obj
  cp vector_search_v12/assets/player.mtl assets/player.mtl
  cp vector_search_v12/assets/player.png assets/player.png
fi
# Se voce ja possui player.obj, copie para assets/ o .mtl e as
# imagens ORIGINAIS exportadas pelo Blender.
make clean && make
./simulador
```

## Video MP4

- Ao clicar em JOGAR (ou Enter na tela de instruções) o replay começa automaticamente.
- A perspectiva de gravação usa uma câmera **ortográfica e fixa no canto alto da sala onde está o personagem**.
- Quando a sala ou o andar do jogador muda, a câmera do replay muda instantaneamente.
- Mostra o avatar real, os inimigos, as chaves, os blocos coloridos e a geometria da sala, sem HUD do jogador.
- Ao voltar ao menu (`ESC`), vencer, fechar a janela ou reiniciar por `F2`, o FFmpeg finaliza o arquivo MP4. A saída vai para a pasta XDG `VIDEOS` (`xdg-user-dir VIDEOS`), geralmente `~/Vídeos` ou `~/Videos`.
- Especificações: **640×360, 24 fps, H.264, sem áudio**; grava direto para o FFmpeg via pipe e não salva frames temporários.
- Pressionar F2 ou perder para o Caçador fecha o vídeo atual e inicia outro.
- Se `ffmpeg` não estiver instalado, a partida funciona normalmente, mas **não grava vídeo**.
- O desempenho pode diminuir dependendo da GPU/CPU; para reduzir o custo, altere `VIDEO_W`, `VIDEO_H`, `VIDEO_FPS` no código.

## Textura do OBJ

Use esta estrutura:

```text
assets/
  player.obj
  player.mtl
  player.png
```

No Blender, faça UV Unwrap antes de exportar; exporte Wavefront OBJ **com materiais** e copie as imagens. O `player.obj` deve conter `mtllib player.mtl` e coordenadas `vt`; o `player.mtl` deve conter `map_Kd player.png` no material aplicado (`usemtl ...`). A textura tem de ser um PNG ou JPG **no disco**, não apenas um nó de material do Blender.

O jogo tenta carregar materiais por `player.mtl` via Raylib; materiais que não tiverem textura recebem `assets/player.png` como alternativa. O modelo é desenhado com tinta `WHITE`, preservando as cores. Se aparecer cinza mesmo com PNG, verifique se sua malha possui UVs e se as faces usam o material correto. **P** no menu recarrega o OBJ; para atualizar o arquivo MTL e as imagens, recarregue também.

Modelo de exemplo (`assets/player.obj`) possui UV, `.mtl` e PNG de demonstração com 1,65 unidades de altura. Se o seu modelo já existe na pasta, o instalador preserva seu OBJ e não copia as texturas de exemplo por cima. Copie seus arquivos .mtl e PNG originais para a mesma pasta.

## Mudanças de jogabilidade 1.1 incluídas

- 10×10 salas em cada um dos 6 andares, uma chave por andar.
- 18 vírus (3 por andar) que devem ser contidos em salas com portas bloqueadas por 5 segundos.
- Inimigos podem ficar presos dentro de bloco; após 5 segundos o bloco se quebra.
- Colocação de blocos baseada no ponto de contato do cursor e ajuste de 0,1 m.
- Tela de vitória mostra o mundo em aramado completo.
- Salvar com Ctrl+S em `vector_search_v11_save.dat`. Saves de versões 1.0 ou anteriores não são substituídos e não são compatíveis com o novo mapa de 6 andares.

## Compilação manual em Linux

```bash
gcc -std=c11 -O2 -Wall -Wextra main.c -o simulador \
$(pkg-config --cflags --libs raylib) \
-lX11 -lXrandr -lXi -lXcursor -lXinerama -lGL -lm -lpthread -ldl
```

## Controles

WASD, espaço para pular, shift para correr, clique esquerdo para blocos, `V` cancela, `F` chaves, `E` powerup azul, `B` migalhas, botão direito abre mapa, `Q` liga/desliga câmera auxiliar, `Ctrl+S` salva, `F2` reinicia, `Shift+F2` reinicia com a mesma seed e `ESC` volta ao menu.

## Observações

A renderização gráfica completa e a codificação por FFmpeg ainda precisam ser experimentadas com a instalação Raylib 6.2 do usuário. O arquivo `Makefile` já inclui as dependências X11 que causavam erros de link anteriormente. O ranking global segue dependendo do servidor Python estar hospedado.
