VECTOR SYSTEM 0.6 - Linux / raylib 6.2

Mundo: 8 x 8 x 8 salas, cada uma 12 unidades => 96 x 96 x 96.
6 chaves triangulares geradas por seed; F coleta e F no hexagono central encaixa.
WASD andar, mouse olhar, ESPACO pular, SHIFT correr.
Clique esquerdo pega e coloca blocos azuis.
PAGE UP / PAGE DOWN: elevador vertical no centro de qualquer sala.
Botao direito: abre/fecha mapa; roda do mouse troca andar no mapa.
Mapa exibe somente salas visitadas, sem mostrar posicao atual.
CTRL+S: salva em simulador_save.dat na pasta atual.
ESC: retorna ao menu; 1 inicia novo, 2 carrega ultimo save.
R: novo mundo, SHIFT+R: reiniciar a mesma seed (sem salvar).

Instalar:
  unzip -o simulador_v06.zip
  cd simulador_v06
  make
  ./simulador
  ./simulador 12345  # seed para novo jogo

OBS: novo jogo e R apagam o progresso da sessao, mas nao sobrescrevem
seu arquivo salvo ate voce pressionar CTRL+S.
O arquivo save e binario e local; nao ha autosave.
