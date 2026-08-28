# Mesa de DJ — simulador multithread em C++

Cada instrumento é uma **thread independente** que toca um arquivo `.wav` em loop.
O DJ controla cada faixa por comandos de texto, pausando e retomando sem afetar as demais.

---

## Estrutura do projeto

| Arquivo | Responsabilidade |
|---|---|
| `main.cpp` | Thread principal: lê e interpreta os comandos do DJ |
| `Instrumento.h/.cpp` | **1 faixa = 1 thread.** Loop de batidas, pausa/retomada, mutex e condition_variable |
| `MesaDeDJ.h/.cpp` | Coleção de faixas + thread do painel ao vivo (2s) |
| `AudioEngine.h/.cpp` | Camada de áudio (miniaudio). Isola a biblioteca do resto do código |
| `Console.h/.cpp` | Mutex do console e códigos ANSI |
| `samples/` | Amostras `.wav` já prontas |

Separar em módulos não é enfeite: o `Instrumento` não sabe o que é `std::cout`,
e a `MesaDeDJ` não sabe o que é miniaudio. Dá para trocar o motor de áudio
mexendo em um arquivo só.

---

## Como compilar

Antes de tudo, o arquivo `miniaudio.h` precisa estar na raiz do projeto
(ele já vem no pacote; se faltar, baixe em
<https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h>).

### Linux / macOS
```bash
g++ -std=c++17 -O2 main.cpp Console.cpp AudioEngine.cpp Instrumento.cpp MesaDeDJ.cpp \
    -o mesa-dj -lpthread -ldl -lm
./mesa-dj
```

### Windows — MinGW / MSYS2
```bash
g++ -std=c++17 -O2 main.cpp Console.cpp AudioEngine.cpp Instrumento.cpp MesaDeDJ.cpp -o mesa-dj.exe
mesa-dj.exe
```

### Windows — Visual Studio (MSVC)
```bat
cl /std:c++17 /EHsc /O2 main.cpp Console.cpp AudioEngine.cpp Instrumento.cpp MesaDeDJ.cpp
```

### Qualquer sistema — CMake
```bash
cmake -B build && cmake --build build
./build/mesa-dj
```

> O executável procura a pasta `samples/` a partir do diretório onde você
> **executa** o programa. Se abrir pelo Visual Studio, ajuste o "working
> directory" ou copie a pasta `samples` para junto do `.exe`.

---

## Comandos

```
play  <faixa>              retoma a faixa
pause <faixa>              pausa (a thread continua viva, só bloqueada)
stop  <faixa>              encerra a thread e tira a faixa da mesa
bpm   <faixa> <20-300>     muda as batidas por minuto
vol   <faixa> <0-100>      muda o volume
add   <nome> <arq.wav> [bpm]   adiciona faixa com a música tocando
play all | pause all       controla todas de uma vez
list                       desenha o painel uma vez
painel on | painel off     liga/desliga o painel automático de 2s
ajuda | sair
```

Exemplo de sessão:
```
pause synth
bpm bateria 140
vol baixo 40
add guitarra samples/synth.wav 75
play synth
sair
```

---

## Os conceitos, na prática (o que explicar na apresentação)

### 1. Uma thread por instrumento
`Instrumento::iniciar()` cria a `std::thread` que roda `loop()`. O loop é sempre
o mesmo ciclo: **toca a amostra → dorme `60000/BPM` ms → repete**.

### 2. Pausar sem espera ocupada
O jeito errado, que quase todo mundo escreve primeiro:

```cpp
while (pausado) { }            // queima 100% de um núcleo à toa
```

O jeito certo usa `std::condition_variable`:

```cpp
cv_.wait(lock, [this]{ return encerrar_ || estado_ == Estado::Tocando; });
```

A thread fica **bloqueada pelo sistema operacional**, com 0% de CPU, até que
alguém chame `notify_all()`. O predicado (a lambda) também protege contra
*spurious wakeups* — condition variables podem acordar sozinhas, e sem o
predicado a faixa voltaria a tocar sem ninguém ter mandado.

### 3. Encerrar é diferente de pausar
São duas flags separadas: `estado_` e `encerrar_`. Se fossem a mesma coisa,
uma faixa pausada nunca conseguiria ser encerrada — ela estaria dormindo
e ninguém a acordaria. Por isso todo `wait` testa `encerrar_` também.
Nada de `exit()`, `terminate()` ou matar thread na força bruta: sinaliza-se
a flag, dá-se `notify_all()` e faz-se `join()`.

### 4. O sleep é o BPM
Em vez de `std::this_thread::sleep_for` (que ficaria "surdo" durante a espera),
usamos `cv_.wait_for(...)` com timeout. Resultado: a espera dura o intervalo do
BPM, **mas** um `pause` ou um `sair` acorda a thread imediatamente, sem esperar
a batida terminar. Testado: 100 BPM em 3 s = 5 batidas; 200 BPM = 10 batidas.

### 5. Dois níveis de exclusão mútua
- `Instrumento::mtx_` protege o estado de **uma** faixa (estado, BPM, volume, contador).
- `MesaDeDJ::faixasMtx_` protege o **vector de faixas**, porque o comando `add`
  insere itens enquanto a thread do painel percorre a lista. Sem esse mutex, um
  `push_back` que realoca o vector invalidaria o iterador do painel — e você
  ganha um crash aleatório que só aparece na hora da apresentação.
- `Console::mutexTela()` protege o `std::cout`, que também é recurso
  compartilhado entre a thread do painel e a de comandos.

### 6. Como evitamos deadlock
Duas regras seguidas o tempo todo no código:

1. **Ordem fixa de travas:** `faixasMtx_` → `mtx_` do instrumento, nunca o inverso.
   Na prática nem chegamos a segurar as duas: copiamos os `shared_ptr` sob o
   `faixasMtx_`, soltamos o lock, e só então chamamos métodos da faixa.
2. **Nunca fazer operação demorada com o lock na mão.** O `join()` em
   `encerrar()`, o disparo do áudio no `loop()` e o carregamento do `.wav` em
   `adicionar()` acontecem todos **fora** da seção crítica. Um `join()` feito
   com o mutex preso trava para sempre: a thread que morre precisa justamente
   daquele mutex para terminar.

### 7. RAII
O destrutor de `Instrumento` chama `encerrar()`, o de `MesaDeDJ` para o painel e
encerra todas as faixas. Nenhuma thread vaza, mesmo se o programa sair por uma
exceção.

---

## Ideias para ir além

- Um `bpm global` que multiplica o BPM de todas as faixas (útil para "acelerar a música").
- `mute`/`solo` por faixa.
- Gravar a sequência de comandos com timestamps e reproduzir depois.
- Trocar o contador de batidas por uma barra que pisca no momento exato da batida.
