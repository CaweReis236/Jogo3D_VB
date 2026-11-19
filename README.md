# Vector Search 1.4

Jogo experimental em **C11 + Raylib**. Explore seis andares, colete seis
chaves, capture dezoito virus construindo com blocos e sobreviva aos inimigos.
O jogo gera o mundo por seed e grava a partida em MP4 com camera ortografica.

## Requisitos (Linux Mint)

- GCC, make, pkg-config e Raylib 6.x instalados;
- FFmpeg no `PATH` para gravar MP4;
- Bibliotecas X11/OpenGL disponiveis para a Raylib estatica.

```bash
sudo apt install build-essential pkg-config make ffmpeg
git clone <URL_DO_SEU_REPOSITORIO>
cd Vector-Search
make
./simulador
```

Se a Raylib foi instalada em `/usr/local`, `pkg-config --modversion raylib`
deve exibir a versao. O `Makefile` inclui explicitamente `-lX11` e as demais
bibliotecas do Linux para evitar referencias indefinidas.

## Modelos 3D por pasta

```
assets/
  Cawe/
    player.obj       # arquivo preferencial para a pasta
    player_1.obj     # variante opcional, preservada
    Cawe.mtl
    Hands.png
    Head.png
    Roupa.png
  Cawe_Alternativo/
    player.obj       # copia da variante player_1.obj
    Cawe.mtl
    Hands.png
    Head.png
    Roupa.png
  Exemplo/
    player.obj
    player.mtl
    player.png
  MeuNovoModelo/
    player.obj
    player.mtl
    textura.png
```

**Seta esquerda/direita no menu**: seleciona o personagem; o painel 3D muda
imediatamente. **P**: procura novamente os modelos no disco e recarrega o
selecionado. A escolha e guardada em `.vector_search_modelo` (arquivo local,
ignorado no Git). Se nenhuma pasta com OBJ for encontrada, aparece o boneco
reserva. A colisao e sempre de 1,65 unidade, independentemente da malha.

O carregador usa `player.obj` caso ele exista; caso contrario, usa o primeiro
arquivo `.obj` em ordem alfabetica. **Uma pasta equivale a um modelo na lista.**
O organizador cria automaticamente `Cawe_Alternativo/` a partir de
`player_1.obj` e copia as texturas/MTL correspondentes, permitindo escolher
**ambas as variantes** no menu. Para outros modelos, crie uma pasta separada.

As texturas sao carregadas pelo `mtllib` no OBJ e `map_Kd` no arquivo MTL.
Mantenha OBJ, MTL e PNG/JPG juntos; respeite maiusculas/minusculas (Linux).
Para um modelo simples com uma unica textura, ha fallback para `player.png`,
`texture.png` ou `textura.png` na mesma pasta. Para varias texturas, configure
cada material no Blender e exporte o MTL. Exemplo:

```text
# trecho do player.obj
mtllib Cawe.mtl
```

```text
# trecho do Cawe.mtl
newmtl Cabeca
map_Kd Head.png
```

## Controles

| Comando | Acao |
|---|---|
| ← / → no menu | Trocar personagem |
| P no menu | Reexaminar pastas de modelos e recarregar |
| 1 / N | Novo mundo |
| 2 / L | Carregar ultimo save |
| WASD / mouse | Andar / olhar |
| Espaco / Shift | Pular / correr |
| Clique esquerdo | Pegar / colocar bloco em grade 1×1×1 |
| Clique direito | Agarrar / lancar virus |
| Tab | Mapa com virus identificados |
| B | Deixar uma migalha no chao |
| F | Pegar chave / inserir no hexagono |
| E | Usar power-up azul |
| V | Cancelar transporte de bloco |
| Q | Alternar camera auxiliar |
| Ctrl+S | Salvar |
| F2 / Shift+F2 | Reiniciar mundo com outra / mesma seed |
| Esc | Voltar ao menu |

## Arquivos

- `main.c`, `Makefile`: jogo e compilacao.
- `leaderboard_server.py`: servidor opcional de placar com SQLite. **Nao existe
  ranking mundial ate que seja hospedado em um servidor acessivel.**
- `assets/<pasta>/`: modelos e texturas.
- `historico/versoes/`: versoes antigas completas.
- `historico/fontes_antigas/`: fontes antigos que estavam soltos na raiz.
- `vector_search_v11_save.dat`, `vector_search_ranking.tsv`: dados pessoais
  gerados localmente, **nao enviados ao GitHub**.
- `simulador`: executavel compilado (nao versionado).

## Publicacao no GitHub

Antes de publicar, confira `git status --short` e `git add -n .` para garantir
que nenhum save, arquivo pessoal ou segredo seja incluido. Recomenda-se iniciar
como repositorio privado e torná-lo publico quando revisar os assets/licencas.

```bash
cd ~/simulador3d
git init -b main
git add .
git diff --cached --stat
git commit -m "Vector Search 1.4: menu de modelos e historico organizado"
# com GitHub CLI instalado e autenticado:
gh repo create Vector-Search --private --source=. --remote=origin --push
```

Se a pasta ja possuir `.git`, **nao rode `git init` novamente**: confira
`git status` e `git remote -v` para nao sobrescrever a configuracao.

Para enviar atualizacoes posteriores:

```bash
git add .
git commit -m "Descreva a alteracao"
git push
```

## Notas

A versao 1.4 deriva da 1.3 e altera principalmente a selecao e o carregamento
dos modelos. O codigo precisa ser compilado e testado em Linux com Raylib real.
A gravacao utiliza FFmpeg; os videos ficam na pasta de Videos configurada no
Linux e nao sao enviados ao GitHub.
