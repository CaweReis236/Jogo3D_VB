# Vector Search 1.3 — construção em grade, vírus transportáveis e replay melhorado

A atualização preserva as seis chaves, os 18 vírus, o mapa 10×10×6, o ranking, o avatar OBJ e os vídeos MP4. Requer Raylib 6.x, GCC, `pkg-config`, `make` e `ffmpeg` para gravar.

## Instalação no Linux Mint

1. Baixe `vector_search_v13.zip` e extraia dentro de `~/simulador3d`.
2. **Faça backup** do `main.c` anterior antes de substituí-lo; o novo pacote não deve substituir seu modelo OBJ personalizado nem os saves.
3. Copie `vector_search_v13/main.c` e `vector_search_v13/Makefile` para `~/simulador3d`.
4. Se ainda não tiver `assets/player.obj`, copie os arquivos de exemplo de `vector_search_v13/assets/` para `~/simulador3d/assets/`.
5. Compile com `make clean && make` e execute `./simulador`.

Exemplo de comandos:

```bash
cd ~/simulador3d
cp main.c main_backup_v12.c
unzip -o ~/Downloads/vector_search_v13.zip
cp vector_search_v13/main.c main.c
cp vector_search_v13/Makefile Makefile
mkdir -p assets
if [ ! -f assets/player.obj ]; then cp vector_search_v13/assets/player.* assets/; fi
make clean && make
./simulador
```

## Controles

| Controle | Ação |
|---|---|
| WASD / mouse | Caminhar / olhar |
| Espaço / Shift | Pular / correr |
| Clique esquerdo | Pegar bloco ou colocar em **célula de 1×1×1 m** |
| V | Cancelar bloco carregado e devolver ao lugar |
| **Clique direito** | Agarrar vírus mirado (até 6,5 m); **clique novamente** para lançar |
| **Tab** | Abrir/fechar mapa (pausa movimento e inimigos) |
| Roda do mouse no mapa | Mostrar outro andar; **pontos verdes = vírus não capturados** |
| B | Deixar migalha bege |
| F | Coletar/encaixar chave |
| E | Consumir power-up azul |
| Q | Ligar/desligar câmera auxiliar |
| Ctrl+S | Salvar a partida |
| F2 / Shift+F2 | Nova seed / reiniciar com seed atual |
| Esc | Menu principal (finaliza o vídeo) |

## Alterações da versão 1.3

- **Gravação ortográfica**: remove visualmente o teto e as paredes mais próximas à câmera do replay, conservando piso, paredes de fundo e entidades. O cenário parece uma maquete aberta, sem esconder o personagem atrás das paredes. A câmera continua mudando por sala e andar.
- **Posicionamento tipo Minecraft**: blocos gerados e colocados medem 1×1×1 m, com centros alinhados em posições `n + 0,5` dos eixos X/Y/Z. Os corredores têm abertura de 4 m para que quatro blocos contíguos fechem uma porta. Não há mais deslocamento imprevisível para alternativas; o bloco fantasma fica azul quando válido ou vermelho quando a célula está ocupada. Blocos fixos da escada permanecem com a geometria antiga.
- **Vírus**: velocidade reduzida de 1,80 para **0,78 unidade/s**, três por andar; podem ser agarrados com clique direito enquanto você mira, transportados e lançados com um segundo clique. Enquanto agarrados não fogem nem contam como capturados. Após soltá-los, continuam obedecendo a paredes e blocos.
- **Mapa**: agora fica em **Tab**; mostra cada vírus ainda ativo com um círculo verde, sem mostrar o jogador. Os bônus e as migalhas continuam aparecendo.
- **Transição entre andares**: os andares visíveis são decididos novamente a cada frame, pela altura dos pés e da cabeça. Só há duas salas preenchidas quando o personagem realmente atravessa um limite vertical. A renderização aramada lateral volta automaticamente quando ele está totalmente num andar.
- **Desorientador**: a perda temporária da HUD caiu de **60 para 25 segundos**. O efeito permanente do **power-up azul**, que oculta o número do andar, continua.
- **Saves**: mesmo arquivo `vector_search_v11_save.dat`, compatível com a estrutura da versão 1.2. Caixas já colocadas em saves antigos preservam a geometria antiga; quando retiradas e colocadas de novo, serão alinhadas à grade.

## Texturas do avatar

Coloque `assets/player.obj`, `assets/player.mtl` e as imagens referenciadas no arquivo MTL dentro da pasta `assets`. O carregador também tenta aplicar `assets/player.png` quando o material não possui textura. No menu, pressione **P** para recarregar.

## MP4

O FFmpeg grava automaticamente em `~/Vídeos`/`~/Videos` (pasta XDG de vídeos), 640×360, 24 FPS, codec H.264, sem áudio. A câmera tem visão ortográfica e o vídeo não tem HUD. Caso não consiga abrir com o reprodutor padrão, instale o VLC (`sudo apt install vlc`).

## Verificação

Sintaxe C11 e testes isolados de geração, posicionamento por raycast, bloqueio de uma porta, agarrar/lançar vírus, tempo de confusão e salvamento. **A execução gráfica com a Raylib 6.2 do seu Linux ainda precisa ser testada no próprio computador**.

## Ranking

O ranking local persiste em `vector_search_ranking.tsv`. Para ranking entre computadores é necessário hospedar `leaderboard_server.py` num servidor compartilhado; essa versão não pressupõe que já existe servidor mundial.
