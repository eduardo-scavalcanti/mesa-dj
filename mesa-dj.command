#!/bin/bash
# mesa-dj.command — equivalente do mesa-dj.exe para macOS.
# Duplo-clique neste arquivo no Finder: abre o Terminal, compila (se
# necessario) e roda o Mesa de DJ. Pode tambem ser chamado por ./mesa-dj.command.

set -e
cd "$(dirname "$0")"

BIN="mesa-dj-mac"
FONTES=(src/cpp/main.cpp src/cpp/Console.cpp src/cpp/AudioEngine.cpp src/cpp/Instrumento.cpp src/cpp/MesaDeDJ.cpp)
CABECALHOS=(src/h/Console.h src/h/AudioEngine.h src/h/Instrumento.h src/h/MesaDeDJ.h)

precisa_compilar=0
if [ ! -x "$BIN" ]; then
    precisa_compilar=1
else
    for f in "${FONTES[@]}" "${CABECALHOS[@]}"; do
        if [ "$f" -nt "$BIN" ]; then
            precisa_compilar=1
            break
        fi
    done
fi

if [ "$precisa_compilar" = "1" ]; then
    if ! command -v g++ &> /dev/null; then
        echo "g++ nao encontrado."
        echo "Instale as Command Line Tools da Apple com: xcode-select --install"
        read -r -p "Pressione Enter para fechar..."
        exit 1
    fi
    echo "Compilando Mesa de DJ..."
    g++ -std=c++17 -O2 -Isrc/h "${FONTES[@]}" -o "$BIN" -lpthread
fi

./"$BIN"

echo
read -r -p "Pressione Enter para fechar..."