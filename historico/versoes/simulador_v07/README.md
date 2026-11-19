# VECTOR SYSTEM 0.7

## Inicio rapido (Linux Mint + raylib 6.x)

```bash
make
./simulador
```

Com seed especifica: `./simulador 123456` (selecione 1 no menu).

Se surgir erro `referencia nao definida para XInternAtom`, use o Makefile incluido: as flags `-lX11 ...` precisam aparecer **depois** de `-lraylib` no link.

## Modelo substituivel

Substitua `assets/player.obj` por um arquivo OBJ estatico qualquer. O programa carrega o OBJ a partir do **diretorio onde voce executa `./simulador`** (execute na raiz do projeto). No menu use **P** para recarregar o OBJ apos substituir o arquivo.

- Unidades em metros: altura esperada **1.65** (pes em Y=0).
- Eixo Y e vertical. O personagem olha para -Z.
- Mantenha o centro dos pes em X=0, Z=0.
- Nao ha animacoes (OBJ e estatico). O OBJ e **visual**; o colisor do jogo nao muda e tem raio 0.30 e altura 1.65.
- Prefira um OBJ com a geometria triangulada, sem referencias a texturas externas.
- No menu a regua amarela marca 1.65 m para voce verificar o tamanho.
- Se o arquivo estiver ausente, aparece um modelo reserva.

## Cameras

A **janela inferior direita** e uma camera auxiliar: enquadra a sala inteira que contem a chave nao coletada mais proxima. Ela nao informa coordenadas. Se voce entrar nessa sala, o player.obj aparece nela na posicao real. Depois de obter todas as chaves, a camera passa a enquadrar o hexagono central. `Q` liga/desliga a camera.

## Controles

- **Menu:** `1`/`N` novo jogo, `2`/`L` carregar ultimo save, `P` recarregar o OBJ.
- **Jogo:** WASD andar, mouse olhar, Espaco pular, Shift correr, clique esquerdo pegar/soltar blocos, F coletar/encaixar chaves, PageUp/PageDown no centro da sala para usar o elevador, botao direito abre/fecha mapa, roda muda camada do mapa, Ctrl+S salvar, R novo mundo, Shift+R mesma seed, Q camera auxiliar, Esc voltar ao menu.

## Salvar e carregar

O arquivo `simulador_save.dat` e gravado no diretorio de execucao ao pressionar Ctrl+S. O formato do save foi **mantido compativel com a versao 0.6**. O salvamento inclui seed, posicao, objetos removidos/adicionados, chaves e mapa visitado. O OBJ e lido do arquivo quando voce inicia o jogo; ele nao e incluido no save.

## Colisao corrigida

O movimento utiliza sub-passos de no maximo 0.04 unidades por eixo, com testes de AABB para caixas e todas as paredes que intersectam a altura do jogador; isso reduz atravesse de quinas ao pular. Mesmo em alta velocidade, o personagem nao deve atravessar as paredes por tunneling.

## Limites

- O mundo possui ate 96 x 96 x 96 unidades, 8x8 salas por camada e 8 camadas.
- Elevadores ligam os andares. O OBJ nao recebe fisica nem animacao.
- As modificacoes de mundo continuam limitadas aos arrays de blocos do jogo original.
- A camera observa a sala de uma chave ainda nao coletada: nao e uma visao de localizacao nem uma segunda janela do sistema.
