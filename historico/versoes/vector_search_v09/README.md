# VECTOR SEARCH 0.9 — O Caçador

Projeto em C11 com Raylib 6.2.0, ambiente 3D procedural por seed.

## Instalação no Linux Mint

Você já precisa ter instalado Raylib, compilador GCC e dependências X11.

```bash
cd ~/simulador3d
cp main.c main_v08_backup.c
unzip -o ~/Downloads/vector_search_v09.zip
cp vector_search_v09/main.c main.c
cp vector_search_v09/Makefile Makefile
mkdir -p assets
test -f assets/player.obj || cp vector_search_v09/assets/player.obj assets/player.obj
make clean && make
./simulador
```

O `Makefile` adiciona as bibliotecas X11 que faltam ao linkar a Raylib estática (`libraylib.a`).

Para iniciar com seed específica:

```bash
./simulador 123456
```

## Jogabilidade

- Existem 8 andares, 8 × 8 chunks por andar, no mundo de 96 × 96 × 48 unidades.
- A seed seleciona 6 andares diferentes com exatamente 1 triângulo-chave por andar; 2 andares não possuem chaves.
- Carregue até **duas chaves**, volte ao hexágono na origem `(0,0,0)` e pressione F para encaixá-las.
- O **Coletor roxo** tenta lacrar chaves ou roubar chaves ainda não encaixadas.
- O **Caçador vermelho** persegue o jogador. No contato sem escudo, reinicia o progresso da partida em memória **mantendo a mesma seed**. O arquivo salvo com Ctrl+S não é excluído.
- Você pode defender passagens com blocos movíveis; as duas entidades respeitam colisões com esses blocos.
- O bônus laranja aparece **uma vez por andar**. Seu chunk é indicado no mapa mesmo que não tenha sido explorado. Ao tocá-lo, concede por 32 s: corrida mais rápida, pulo mais alto e uma proteção que repele uma investida do Caçador (consumindo o efeito).
- Pressione **B** estando no chão ou em um bloco para colocar um círculo bege (migalha). As migalhas aparecem também no mapa. Máximo: 640.
- As salas exploradas ficam sombreadas no mapa, **sem marcador da posição do jogador**. Os antigos círculos amarelos automáticos foram removidos.
- Uma seta vermelha tridimensional acompanha a câmera e aponta para o hexágono de origem, inclusive quando ele está em outro andar.
- A sala atual possui paredes, chão e teto coloridos com tons pouco saturados. Os quartos laterais do **mesmo andar** aparecem em aramado verde. O teto tem abertura correspondente ao poço do andar superior; o último andar tem teto completo.
- Os blocos gerados são preenchidos com cores intensas; podem ser pegos com clique esquerdo e recolocados com o próximo clique. V cancela a remoção e restaura o bloco.
- O modelo `assets/player.obj` é estático e substituível. A colisão é independente do OBJ. Prévia do modelo no menu; P recarrega o OBJ.
- Q alterna a câmera auxiliar que observa a sala da chave mais próxima ainda não coletada; quando o jogador estiver nessa sala, também aparece na imagem.

## Controles

| Controle | Ação |
|---|---|
| 1 / N (menu) | Novo jogo |
| 2 / L (menu) | Carregar último save |
| P (menu) | Recarregar player.obj |
| WASD | Caminhar |
| Mouse | Olhar |
| Espaço | Pular |
| Shift | Correr |
| Clique esquerdo | Pegar/colocar bloco |
| V | Devolver bloco pego à posição original |
| F | Coletar até duas chaves / depositar no hexágono |
| B | Deixar migalha bege (somente apoiado) |
| Clique direito | Abrir/fechar mapa (movimento e inimigos pausados) |
| Roda do mouse no mapa | Escolher andar |
| Q | Alternar câmera auxiliar |
| Ctrl+S | Salvar em `vector_search_save.dat` |
| R | Novo mundo, nova seed |
| Shift+R | Reiniciar mundo na mesma seed |
| ESC | Voltar ao menu |

## Salvamento

O save é gravado no diretório atual como `vector_search_save.dat`, com gravação temporária e renomeação. Inclui progresso das chaves, blocos, posição, duas entidades, bônus e migalhas. O leitor também reconhece o formato da versão 0.8 e o arquivo legado `simulador_save.dat` da 0.6. Ao importar saves antigos, as chaves ainda não coletadas passam a seguir a nova regra: seis andares distintos. Qualquer chave carregada além da capacidade nova de duas é depositada automaticamente no hexágono, sem perda de progresso.

## Testes efetuados

- Compilação de sintaxe C11 usando cabeçalhos de teste locais (o ambiente de execução aqui não possui biblioteca Raylib instalada).
- 20.000 seeds verificadas: seis chaves em andares diferentes e localização do bônus dentro dos limites.
- Testes isolados de lógica: capacidade de duas chaves e depósito, migalhas, bônus e colisões em portas/paredes/teto/poço.
- Gravação e leitura do save 0.9; leitura e migração de um save 0.8 gerado para teste.

**O jogo gráfico ainda deve ser compilado e executado no Linux Mint do usuário.**
