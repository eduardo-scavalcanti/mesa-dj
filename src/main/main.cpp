// ---------------------------------------------------------------------------
//  MESA DE DJ - simulador multithread
//
//  Arquitetura de threads:
//    * 1 thread por instrumento -> toca a amostra em loop (Instrumento)
//    * 1 thread de painel -> redesenha o status a cada 2s (MesaDeDJ)
//    * 1 thread principal -> le e interpreta os comandos (este arquivo)
// ---------------------------------------------------------------------------

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "AudioEngine.h"
#include "Console.h"
#include "MesaDeDJ.h"

namespace
{

// Toda escrita no console passa pelo mutex compartilhado.
void imprimir(const std::string& texto)
{
    std::lock_guard<std::mutex> lg(Console::mutexTela());
    std::cout << texto << "\n";
}

void ajuda()
{
    imprimir(
        "\nCOMANDOS\n"
        "  play  <faixa>            retoma a faixa\n"
        "  pause <faixa>            pausa a faixa (a thread continua viva)\n"
        "  stop  <faixa>            encerra a thread da faixa e remove da mesa\n"
        "  bpm   <faixa> <20-300>   muda as batidas por minuto\n"
        "  vol   <faixa> <0-100>    muda o volume\n"
        "  add   <nome> <arq.wav> [bpm]   adiciona faixa com a musica rodando\n"
        "  play all | pause all     controla todas de uma vez\n"
        "  list                     desenha o painel uma vez\n"
        "  painel on | painel off   liga/desliga o painel automatico (2s)\n"
        "  ajuda                    mostra esta lista\n"
        "  sair                     encerra tudo com seguranca\n");
}

int paraInt(const std::string& s, int padrao)
{
    try
    {
        return std::stoi(s);
    }

    catch (...)
    {
        return padrao;
    }
}

} // namespace

int main()
{
    Console::habilitarAnsi();

    AudioEngine audio;
    audio.iniciar(); // se falhar, a mesa roda em modo silencioso

    MesaDeDJ mesa(audio);

    // ---- faixas iniciais ----
    struct { const char* nome; const char* arquivo; int bpm; } iniciais[] = 
    {
        {"bateria", "samples/bateria.wav", 100},
        {"hihat",   "samples/hihat.wav",   200},
        {"baixo",   "samples/baixo.wav",    50},
        {"synth",   "samples/synth.wav",    25},
    };

    for (const auto& i : iniciais)
    {
        std::string erro;

        if (!mesa.adicionar(i.nome, i.arquivo, i.bpm, erro)) 
        {
            imprimir(std::string("[aviso] ") + i.nome + ": " + erro);
        }
    }

    ajuda();
    mesa.iniciarPainel();

    // ---- loop de comandos (thread principal) ----
    std::string linha;

    while (std::getline(std::cin, linha))
    {
        std::istringstream in(linha);
        std::string cmd, arg1, arg2;
        in >> cmd >> arg1 >> arg2;

        if (cmd.empty()) continue;

        if (cmd == "sair" || cmd == "exit" || cmd == "quit")
        {
            break;
        }

        else if (cmd == "ajuda" || cmd == "help")
        {
            ajuda();
        }

        else if (cmd == "list")
        {
            mesa.desenhar();
        }

        else if (cmd == "painel")
        {
            if (arg1 == "on")
            {
                mesa.iniciarPainel();
            }

            else if (arg1 == "off")
            {
                mesa.pararPainel();
                imprimir("[ok] painel desligado");
            }

            else imprimir("[erro] use: painel on | painel off");
        }

        else if (cmd == "add")
        {
            if (arg1.empty() || arg2.empty())
            {
                imprimir("[erro] use: add <nome> <arquivo.wav> [bpm]");
                continue;
            }

            std::string bpmTexto;
            in >> bpmTexto;
            int bpm = bpmTexto.empty() ? 100 : paraInt(bpmTexto, 100);

            std::string erro;

            if (mesa.adicionar(arg1, arg2, bpm, erro))
                imprimir("[ok] faixa '" + arg1 + "' entrou na mesa");
            else
                imprimir("[erro] " + erro);
        }

        else if (cmd == "play" && arg1 == "all")
        {
            mesa.tocarTodas();
        }

        else if (cmd == "pause" && arg1 == "all")
        {
            mesa.pausarTodas();
        }

        else if (cmd == "play" || cmd == "pause" || cmd == "stop" ||
                 cmd == "bpm"  || cmd == "vol")
        {
            if (arg1.empty())
            {
                imprimir("[erro] falta o nome da faixa");
                continue;
            }

            auto faixa = mesa.buscar(arg1);
            if (!faixa && cmd != "stop")
            {
                imprimir("[erro] faixa '" + arg1 + "' não existe");
                continue;
            }

            if (cmd == "play")  
            {
                faixa->tocar();
            }

            else if (cmd == "pause") 
            {
                faixa->pausar();
            }

            else if (cmd == "stop")
            {
                if (mesa.remover(arg1)) 
                {
                    imprimir("[ok] '" + arg1 + "' encerrada");
                }
                else                    
                {
                    imprimir("[erro] faixa não encontrada");
                }
            }

            else if (cmd == "bpm")
            {
                if (arg2.empty())
                {
                    imprimir("[erro] use: bpm <faixa> <20-300>");
                    continue;
                }

                faixa->definirBpm(paraInt(arg2, 100));
            }

            else if (cmd == "vol")
            {
                if (arg2.empty())
                {
                    imprimir("[erro] use: vol <faixa> <0-100>");
                    continue;
                }
                faixa->definirVolume(paraInt(arg2, 80));
            }
        }

        else
        {
            imprimir("[erro] comando desconhecido: '" + cmd + "' (digite 'ajuda')");
        }
    }

    // ---- desligamento ordenado ----
    // Primeiro o painel para de desenhar, depois cada thread de instrumento
    // recebe o sinal de encerrar e sofre join. Nada de exit() abrupto.
    mesa.pararPainel();
    mesa.encerrarTudo();

    {
        std::lock_guard<std::mutex> lg(Console::mutexTela());
        Console::limpar();
        std::cout << "Todas as threads foram encerradas com seguranca.\n Até a próxima!\n";
    }
    return 0;
}