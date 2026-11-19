# VECTOR SEARCH 0.8

Simulador/jogo procedural em C11 + Raylib 6.x, com salas, construcao, camera de chaves, modelo OBJ substituivel e salvamento.

## Instalar no Linux Mint (raylib compilada estaticamente)

```bash
cd ~/simulador3d
cp main.c main_v07_backup.c
unzip -o ~/Downloads/vector_search_v08.zip
cp vector_search_v08/main.c ./main.c
cp vector_search_v08/Makefile ./Makefile
mkdir -p assets
# Nao apaga seu OBJ personalizado:
test -e assets/player.obj || cp vector_search_v08/assets/player.obj assets/player.obj
make
./simulador
```

Seu arquivo `simulador_save.dat` **nao e alterado**. O novo save se chama `vector_search_save.dat`.

Se o ZIP foi baixado para uma pasta diferente, ajuste o caminho. Como alternativa, copie a pasta extraida inteira e rode `make` dentro dela.

Seed especifica: `./simulador 123456` e escolha Novo Mundo (1/N).

## Mudancas

- Nome: **VECTOR SEARCH**.
- **6 chaves** triangulares SOLIDAS/douradas, encaixadas no hexagono de `(0,0,0)` com F.
- **8 andares**, cada um com 6 m de altura: total **96 x 96 x 48**, dentro do limite 100 x 100 x 100.
- Sem elevadores. Um conjunto de **8 plataformas/blocos** em cada sala permite subir, pulando de degrau em degrau. Os degraus fixos nao sao removiveis para evitar um mundo permanentemente intransitavel.
- Piso do **andar 0 e inteiro**. Todos os andares superiores possuem uma **abertura quadrada no centro**, por onde e possivel descer.
- Blocos soltos e blocos colocados ficam **preenchidos com cores vivas**, determinadas pela seed e suas coordenadas; o piso e as paredes da sala atual mantem tons suaves.
- **O COLETOR**, esfera violeta: procura chaves no andar atual e tenta lacra-las temporariamente (18 s). Se voce possui uma chave nao encaixada, ele passa a perseguir voce; ao tocar, pode devolver uma chave ao ponto onde ela estava originalmente. Nao mata e nao remove progresso definitivamente.
- O Coletor **respeita as paredes e os blocos**, portanto voce pode criar barricadas nos corredores. Duas caixas lado a lado podem bloquear uma porta. A escadaria estrutural e atravessavel apenas por ele, por ser uma entidade especial.
- Ao recolocar um bloco, o jogo testa primeiro a face visada e depois varios pontos proximos. Se nao encontrar lugar seguro, aperte **V** para devolver o bloco ao local onde estava antes da coleta, sem reiniciar.
- Paredes externas **sem aberturas** e limites corrigidos: o mundo vai de -54 a +42 em X e Z, preservando exatamente 96 unidades com os chunks atuais.
- Na camera principal, **somente a sala atual** e preenchida; salas laterais **do MESMO andar** usam grade/paredes aramadas verdes. Salas de cima ou de baixo nao sao desenhadas a distancia. A camera auxiliar ainda observa a sala da chave mais proxima, como antes.
- Mapa por botao direito continua sem mostrar a posicao do jogador. Salas visitadas ficam marcadas com circulos amarelos.
- `assets/player.obj` estatico e substituivel. O menu possui a previa com regua de 1,65 m (P recarrega o OBJ); o personagem aparece na camera de chave quando esta na sala filmada.

## Controles

| Entrada | Acao |
|---|---|
| 1 / N no menu | Novo jogo |
| 2 / L no menu | Carregar ultimo save |
| P no menu | Recarregar player.obj |
| WASD + mouse | Caminhar e olhar |
| Espaco | Pular / subir pelas plataformas |
| Shift | Correr |
| Clique esquerdo | Pegar bloco solto / colocar o que esta carregando |
| V | Cancelar coleta, devolvendo o bloco para seu lugar anterior |
| F | Coletar chave / encaixar as chaves no hexagono central |
| Botao direito | Abrir/fechar mapa (movimento bloqueado) |
| Roda do mouse no mapa | Consultar andar, sem indicar a sua posicao |
| Q | Ativar/desativar camera secundaria |
| Ctrl + S | Salvar manualmente |
| R | Reiniciar com nova seed (nao sobrescreve save) |
| Shift + R | Reiniciar com a mesma seed |
| Esc | Voltar ao menu |

**Saves**: `vector_search_save.dat` salva seed, progresso das chaves, salas visitadas, blocos e status do Coletor. Se nao existir, o jogo tenta importar `simulador_save.dat` criado nas versoes 0.6/0.7. Como a altura dos andares mudou de 12 para 6 m e a geometria dos blocos mudou, uma partida antiga pode ter um posicionamento parcialmente diferente; prefira iniciar um mundo novo para esta versao. O save antigo nunca e sobrescrito.

## Compilar

```bash
make
./simulador
```

Se precisar compilar sem Makefile:

```bash
gcc -std=c11 -O2 -Wall -Wextra main.c -o simulador \
  $(pkg-config --cflags --libs raylib) \
  -lX11 -lXrandr -lXi -lXcursor -lXinerama -lGL -lm -lpthread -ldl
```

## Testes e limitacoes

- Foram feitos testes de **sintaxe C11 com cabecalhos de teste** (nao e um link completo com Raylib), e testes isolados de colisao nas bordas, queda no poco, colocacao de blocos, bloqueio do Coletor, save/restore e importacao de save antigo.
- O ambiente que gerou este pacote nao tem Raylib instalada. Portanto **ainda e preciso confirmar a compilacao com raylib 6.2.0 e a renderizacao real no Linux Mint**.
- IA simples: o Coletor anda no plano do andar atual; ao mudar de andar, aparece nesse andar apos um intervalo. Nao tem pathfinding complexo e pode ficar preso em construcoes — isso faz parte da mecanica de barricadas.
- As escadas coloridas sao plataformas estaticas fixas; os outros blocos podem ser manipulados. Nao existe simulacao de queda ou peso dos blocos.
- O mapa nao revela a localizacao atual do jogador; a camera secundaria ajuda a identificar a aparencia de uma sala, mas nao fornece coordenadas.
