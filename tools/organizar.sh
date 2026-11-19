#!/usr/bin/env bash
# Migra a pasta existente sem apagar saves, rankings, modelos, historicos ou .git.
set -euo pipefail
shopt -s nullglob

KIT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJETO="${1:-$HOME/simulador3d}"
PROJETO="$(cd "$PROJETO" && pwd)"

if [[ ! -f "$PROJETO/main.c" || ! -f "$PROJETO/Makefile" ]]; then
    printf 'ERRO: nao encontrei main.c e Makefile em %s\n' "$PROJETO" >&2
    exit 1
fi
if [[ "$KIT" == "$PROJETO" ]]; then
    echo 'ERRO: extraia o kit fora de simulador3d (por exemplo em Downloads).' >&2
    exit 1
fi
for obrigatorio in "$KIT/main.c" "$KIT/Makefile" "$KIT/README.md" "$KIT/.gitignore"; do
    if [[ ! -f "$obrigatorio" ]]; then
        echo "ERRO: arquivo ausente no kit: $obrigatorio" >&2
        exit 1
    fi
done

# O backup fica FORA do repositorio e inclui ate arquivos ignorados pelo Git.
STAMP="$(date +%Y%m%d_%H%M%S)"
BACKUP="$HOME/$(basename "$PROJETO")_backup_$STAMP.tar.gz"
if [[ -e "$BACKUP" ]]; then
    echo "ERRO: ja existe $BACKUP. Tente novamente." >&2
    exit 1
fi
printf '[1/6] Backup integral: %s\n' "$BACKUP"
tar -czf "$BACKUP" -C "$(dirname "$PROJETO")" "$(basename "$PROJETO")"

mkdir -p "$PROJETO/historico/fontes_antigas" \
         "$PROJETO/historico/versoes" \
         "$PROJETO/assets/Cawe" \
         "$PROJETO/tools"

# Verifica colisoes de nomes ANTES de mover qualquer coisa.
for a in "$PROJETO"/main_backup*.c "$PROJETO"/main_v*.c; do
    [[ -f "$a" ]] || continue
    dest="$PROJETO/historico/fontes_antigas/$(basename "$a")"
    if [[ -e "$dest" ]]; then echo "ERRO: destino ja existe: $dest" >&2; exit 1; fi
done
for a in "$PROJETO"/simulador_v[0-9]* "$PROJETO"/vector_search_v[0-9]*; do
    [[ -d "$a" ]] || continue
    dest="$PROJETO/historico/versoes/$(basename "$a")"
    if [[ -e "$dest" ]]; then echo "ERRO: destino ja existe: $dest" >&2; exit 1; fi
done
for a in "$PROJETO"/assets/*; do
    [[ -f "$a" ]] || continue
    case "${a##*.}" in
        obj|OBJ|mtl|MTL|png|PNG|jpg|JPG|jpeg|JPEG|webp|WEBP)
            dest="$PROJETO/assets/Cawe/$(basename "$a")"
            if [[ -e "$dest" ]]; then echo "ERRO: destino ja existe: $dest" >&2; exit 1; fi
            ;;
    esac
done

printf '[2/6] Reunindo os fontes antigos...\n'
for a in "$PROJETO"/main_backup*.c "$PROJETO"/main_v*.c; do
    [[ -f "$a" ]] && mv "$a" "$PROJETO/historico/fontes_antigas/"
done
printf '[3/6] Reunindo as versoes antigas...\n'
for a in "$PROJETO"/simulador_v[0-9]* "$PROJETO"/vector_search_v[0-9]*; do
    [[ -d "$a" ]] && mv "$a" "$PROJETO/historico/versoes/"
done

printf '[4/6] Organizando os OBJ/MTL/texturas...\n'
for a in "$PROJETO"/assets/*; do
    [[ -f "$a" ]] || continue
    case "${a##*.}" in
        obj|OBJ|mtl|MTL|png|PNG|jpg|JPG|jpeg|JPEG|webp|WEBP)
            mv "$a" "$PROJETO/assets/Cawe/"
            ;;
    esac
done
# Se o projeto ja tinha player.obj E player_1.obj, oferecemos os dois
# no menu. Criamos uma segunda pasta com a variante e suas texturas.
# A pasta original Cawe continua intocada, incluindo player_1.obj.
if [[ -f "$PROJETO/assets/Cawe/player_1.obj" && ! -d "$PROJETO/assets/Cawe_Alternativo" ]]; then
    mkdir -p "$PROJETO/assets/Cawe_Alternativo"
    cp "$PROJETO/assets/Cawe/player_1.obj" "$PROJETO/assets/Cawe_Alternativo/player.obj"
    for original in "$PROJETO"/assets/Cawe/*; do
        [[ -f "$original" ]] || continue
        case "${original##*.}" in
            mtl|MTL|png|PNG|jpg|JPG|jpeg|JPEG|webp|WEBP)
                cp "$original" "$PROJETO/assets/Cawe_Alternativo/"
                ;;
        esac
    done
fi
# Exemplos fornecidos pelo projeto nunca substituem modelos pessoais.
if [[ ! -d "$PROJETO/assets/Exemplo" ]]; then
    cp -a "$KIT/assets/Exemplo" "$PROJETO/assets/Exemplo"
fi

printf '[5/6] Instalando codigo e documentacao...\n'
cp "$KIT/main.c" "$PROJETO/main.c"
cp "$KIT/Makefile" "$PROJETO/Makefile"
cp "$KIT/README.md" "$PROJETO/README.md"
cp "$KIT/.gitignore" "$PROJETO/.gitignore"
cp "$KIT/historico_README.md" "$PROJETO/historico/README.md"
cp "$KIT/organizar.sh" "$PROJETO/tools/organizar.sh"
chmod +x "$PROJETO/tools/organizar.sh"
if [[ ! -f "$PROJETO/leaderboard_server.py" ]]; then
    cp "$KIT/leaderboard_server.py" "$PROJETO/leaderboard_server.py"
fi

printf '[6/6] Finalizado. Conferir estrutura e testar compilacao.\n'
echo
printf 'BACKUP: %s\n' "$BACKUP"
printf 'PROJETO: %s\n' "$PROJETO"
cat <<'EOF'
Proximos comandos:
  cd ~/simulador3d
  tree -L 3
  make clean && make
  ./simulador
Depois de testar, revise git status e git add -n . antes de publicar.
EOF
