# VECTOR SEARCH 1.0 — O Desorientador

Jogo procedural 3D em **C11 + Raylib 6.2.0**. Use o `player.obj` existente ou substitua o modelo pelo seu. O OBJ nao possui animacao e **nao altera** a geometria do colisor.

## Instalacao no Linux Mint

Requer a Raylib instalada, GCC, make e as dependencias X11 que voce ja instalou nas versoes anteriores.

```bash
cd ~/simulador3d
cp main.c main_v09_backup.c
unzip -o ~/Downloads/vector_search_v10.zip
cp vector_search_v10/main.c main.c
cp vector_search_v10/Makefile Makefile
mkdir -p assets
# Nao sobrescreve o OBJ personalizado:
test -f assets/player.obj || cp vector_search_v10/assets/player.obj assets/player.obj
make clean && make
./simulador
```

Se o arquivo ZIP nao estiver em `~/Downloads`, altere o caminho. Voce tambem pode executar `make` diretamente dentro da pasta extraida `vector_search_v10`.

Nova partida com seed: `./simulador 123456`.

## Mudancas 1.0

- **Novo inimigo, o Desorientador** (verde-agua): so aparece quando o jogador esta **a ate 3 salas**, contando deslocamento horizontal e vertical, de qualquer chave ainda nao coletada. Ele segue fisicamente o jogador. **Ao tocar no personagem, teleporta-o para uma sala e um andar aleatorios seguros e oculta completamente a HUD por 60 segundos reais**. Desaparece por um tempo depois de agir, evitando teletransportes em sequencia.
- **Salas concluidas:** ao coletar a chave de um andar, as paredes daquele andar ficam **cinzas**, mesmo que o Coletor roube a chave depois. O estado fica salvo.
- **Renderizacao entre andares:** se o personagem estiver proximo ao limite ou estiver no ar, os niveis adjacentes sao renderizados juntos, respeitando salas coloridas proximas e grades aramadas distantes.
- **Inimigos sobem e descem por escadas:** o Coletor e o Cacador seguem os degraus coloridos ja existentes, sem reaparecer instantaneamente em outro andar. As barreiras de blocos continuam impedindo a passagem. O novo Desorientador tambem usa essa movimentacao.
- **Power-up azul:** existe um cubo azul no mundo (sua sala aparece no mapa). Ao coleta-lo, ele vai para o inventario. **Pressione E** para ir a uma sala onde existe chave ainda nao coletada. O bonus e consumido; outro aparece em **outro andar** definido proceduralmente. O preco e permanente na partida: o numero do andar desaparece da HUD e do mapa. O save preserva essa penalidade.
- **Reset por F2**, com `Shift+F2` reiniciando a mesma seed; a tecla R fica livre para atualizar ranking na tela de ranking.
- **Tela de instrucoes** antes de comecar a jogar, tanto para partida nova quanto para carregar save.
- **Vitoria:** quando as seis chaves estao inseridas no hexagono, mostra o tempo da partida e um campo para inserir nome (letras inglesas, numeros, `_` e `-`). ENTER registra; botoes clicaveis abrem ranking e menu.
- **Ranking local persistente:** `vector_search_ranking.tsv`, lista as melhores marcas e continua existindo apos fechar o programa.
- **Ranking global opcional:** inclui o servidor Python `leaderboard_server.py` com banco SQLite, necessario para reunir jogadores em diferentes computadores. **Sem servidor hospedado/acessivel, o ranking exibido como global informa indisponibilidade; somente o local funciona.**

## Controles

| Tecla | Acao |
|---|---|
| WASD | Caminhar |
| Mouse | Olhar em volta |
| Espaco | Pular/subir |
| Shift | Correr |
| Clique esquerdo | Pegar/colocar bloco |
| V | Restaurar bloco carregado |
| F | Coletar/encaixar chaves |
| E | Gastar power-up azul armazenado |
| B | Deixar migalha bege |
| Clique direito | Mapa com partida pausada |
| Roda no mapa | Mudar andar do mapa (numeros ocultos apos usar o azul) |
| Q | Liga/desliga camera auxiliar das chaves |
| Ctrl+S | Salvar partida manualmente |
| F2 | Novo jogo/seed |
| Shift+F2 | Reiniciar mesma seed |
| ESC | Menu |
| P no menu | Recarregar `assets/player.obj` |
| G no menu/vitoria | Abrir ranking |
| ENTER nas instrucoes | Comecar jogo |

No menu, nas instrucoes, na vitoria e no ranking ha botoes clicaveis; os atalhos de teclado continuam funcionando.

## Arquivos persistentes

- **Save:** `vector_search_save.dat` (gravado no diretorio onde o jogo e executado, manualmente com Ctrl+S). Lê saves das versoes 0.9, 0.8 e 0.6; ao salvar, atualiza para 1.0. Salve uma copia dos arquivos antigos antes de atualizar.
- **Ranking local:** `vector_search_ranking.tsv`, independente do save. O Cacador reinicia a partida mas nao apaga esse arquivo.
- **Banco do servidor global:** `vector_search_global.sqlite3` na mesma pasta do script do servidor. A persistencia entre usuarios so acontece quando **todos apontam para o mesmo servidor**.

## Configurar ranking global experimental

O jogo usa `curl` para enviar resultados/consultar o ranking. Instale, se necessario:

```bash
sudo apt install curl python3
```

Para testar no **mesmo computador**, em outro terminal:

```bash
cd ~/simulador3d/vector_search_v10
python3 leaderboard_server.py --host 127.0.0.1 --port 8765
```

No terminal do jogo, antes de iniciar:

```bash
cd ~/simulador3d
export VECTOR_SEARCH_RANKING_URL="http://127.0.0.1:8765"
./simulador
```

Para um **ranking entre computadores no mundo inteiro**, coloque o script num servidor acessivel pela internet, configure um dominio com HTTPS, e aponte a variavel de ambiente de todos os jogadores para esse mesmo endereco **sem barra final**. O servidor usa `/ranking` (GET) e `/submit` (POST); `curl` e necessario no computador do jogador.

**AVISO:** o servidor fornecido e propositalmente minimo para testes/prototipagem. Nao tem autenticacao, controles de abuso ou anticheat; resultados podem ser falsificados. Antes de torna-lo publico, implemente verificacao de resultados, autenticacao e limitacao de requisicoes, e coloque atras de HTTPS/reverse proxy. O arquivo SQLite deve ficar num armazenamento persistente do servidor, nao numa pasta efemera de hospedagem.

## Observacoes

- O mundo tem 8 andares (96x96x48 unidades), seis chaves em andares distintos. O mapa continua sem marcador do jogador.
- O contador de tempo inclui a exploracao ativa, excluindo as telas de menu, instrucoes, mapa e vitoria.
- O inimigo verde-agua e diferente do Coletor roxo e do Cacador vermelho.
- Cada inimigo caminha por escadas; barricadas podem faze-los parar. Esta e uma IA simples, sem busca global de caminhos ou animacao de passos.
- O Power-up azul escolhe a **chave nao coletada mais proxima em distancia 3D**, que nao e necessariamente o andar imediatamente superior/inferior.
- Compilacao grafica com a Raylib instalada nao foi testada neste ambiente; codigo verificado com GCC C11 e cabecalhos de simulacao, alem de testes isolados de geracao, teleporte, tempo de confusao, escadas e save.
