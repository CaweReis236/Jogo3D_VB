#!/usr/bin/env bash
set -Eeuo pipefail

RAYLIB_VERSION="6.2.0"

log() { printf '\n\033[1;32m[Vector Search]\033[0m %s\n' "$*"; }
fail() { printf '\n\033[1;31m[ERRO]\033[0m %s\n' "$*" >&2; exit 1; }
trap 'fail "Falha na linha $LINENO. Confira a mensagem acima."' ERR

[[ "$(uname -s)" == "Linux" ]] || fail "Este instalador requer Linux."
command -v apt-get >/dev/null || fail "Este instalador suporta Ubuntu e Linux Mint (apt)."
[[ $EUID -ne 0 ]] || fail "Execute como usuario comum: bash instalar_vector_search.sh (nao use sudo bash)."
command -v sudo >/dev/null || fail "sudo nao encontrado."

# O script funciona na raiz do repositorio ou na pasta onde foi colocado.
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
[[ -f "$PROJECT_DIR/Makefile" && -f "$PROJECT_DIR/main.c" ]] || fail "Coloque este script na raiz do projeto (junto de main.c e Makefile)."

log "Solicitando permissao para instalar dependencias do sistema..."
sudo -v

log "Instalando compilador, bibliotecas graficas, FFmpeg e codecs..."
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential git cmake pkg-config ca-certificates \
  libasound2-dev libx11-dev libxrandr-dev libxi-dev \
  libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev \
  libxext-dev libxfixes-dev libxrender-dev libxkbcommon-dev \
  ffmpeg xdg-user-dirs python3 gstreamer1.0-libav gstreamer1.0-plugins-ugly

# Evita sobrescrever instalacoes preexistentes quando a versao ja e adequada.
CURRENT_RAYLIB="$(pkg-config --modversion raylib 2>/dev/null || true)"
if [[ "$CURRENT_RAYLIB" == "$RAYLIB_VERSION" ]]; then
  log "Raylib $CURRENT_RAYLIB ja instalada."
else
  log "Compilando Raylib $RAYLIB_VERSION (sem alterar os arquivos do jogo)..."
  BUILD_ROOT="$(mktemp -d)"
  trap 'rm -rf "$BUILD_ROOT"' EXIT
  git clone --depth 1 --branch "$RAYLIB_VERSION" https://github.com/raysan5/raylib.git "$BUILD_ROOT/raylib"
  cmake -S "$BUILD_ROOT/raylib" -B "$BUILD_ROOT/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF
  cmake --build "$BUILD_ROOT/build" --parallel "$(nproc)"
  sudo cmake --install "$BUILD_ROOT/build"
  sudo ldconfig
fi

log "Compilando Vector Search..."
cd "$PROJECT_DIR"
# Usamos o Makefile do proprio projeto, que inclui as dependencias X11.
make clean
make

[[ -x "$PROJECT_DIR/simulador" ]] || fail "O Makefile nao produziu o executavel ./simulador."
log "Instalacao concluida! Raylib: $(pkg-config --modversion raylib)"
printf '\nPara jogar:\n  cd %q\n  ./simulador\n' "$PROJECT_DIR"
printf '\nGravacoes MP4: pasta Videos/Videos do usuario (dependendo da configuracao).\n'
