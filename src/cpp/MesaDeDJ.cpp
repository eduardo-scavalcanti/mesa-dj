#include "MesaDeDJ.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "Console.h"

using namespace std;

MesaDeDJ::MesaDeDJ(AudioEngine& audio) : audio_(audio) {}

MesaDeDJ::~MesaDeDJ()
{
    pararPainel();
    encerrarTudo();
}

bool MesaDeDJ::adicionar(const string& nome, const string& arquivo,
                         int bpm, string& erro)
{
    {
        lock_guard<mutex> lg(faixasMtx_);
        auto it = find_if(faixas_.begin(), faixas_.end(),
                               [&](const auto& f) { return f->nome() == nome; });
                    
        if (it != faixas_.end())
        {
            erro = "ja existe uma faixa chamada '" + nome + "'";
            return false;
        }
    }

    auto novo = make_shared<Instrumento>(nome, arquivo, bpm, audio_);

    // Carregar o .wav pode demorar alguns ms: fazemos isso FORA do lock,
    // enquanto o resto da mesa continua tocando normalmente.

    if (!novo->carregarAmostra() && audio_.pronto())
    {
        erro = "nao consegui carregar o arquivo '" + arquivo + "'";
        return false;
    }

    novo->iniciar();

    {
        lock_guard<mutex> lg(faixasMtx_);
        faixas_.push_back(std::move(novo));
    }
    return true;
}

bool MesaDeDJ::remover(const string& nome)
{
    shared_ptr<Instrumento> alvo;
    {
        lock_guard<mutex> lg(faixasMtx_);
        auto it = find_if(faixas_.begin(), faixas_.end(),
                               [&](const auto& f) { return f->nome() == nome; });

        if (it == faixas_.end()) 
        {
            return false;
        }

        alvo = *it; // segura o objeto vivo pelo shared_ptr
        faixas_.erase(it);
    }
    // encerrar() faz join na thread. Fazemos isso com o faixasMtx_ JA LIBERADO,
    // senao o painel ficaria travado esperando a faixa morrer.
    alvo->encerrar();
    return true;
}

shared_ptr<Instrumento> MesaDeDJ::buscar(const string& nome) const
{
    lock_guard<mutex> lg(faixasMtx_);
    auto it = find_if(faixas_.begin(), faixas_.end(),
    [&](const auto& f) { return f->nome() == nome; });

    return it == faixas_.end() ? nullptr : *it;
}

vector<shared_ptr<Instrumento>> MesaDeDJ::todas() const
{
    lock_guard<mutex> lg(faixasMtx_);
    return faixas_; // copia dos ponteiros: quem chama trabalha sem o lock
}

void MesaDeDJ::tocarTodas()
{
    for (auto& f : todas()) f->tocar();
}

void MesaDeDJ::pausarTodas()
{
    for (auto& f : todas()) f->pausar();
}

void MesaDeDJ::encerrarTudo()
{
    vector<shared_ptr<Instrumento>> copia;
    {
        lock_guard<mutex> lg(faixasMtx_);
        copia.swap(faixas_);
    }

    for (auto& f : copia) f->encerrar();
}

// --------------------------- painel ao vivo --------------------------------

void MesaDeDJ::iniciarPainel()
{
    {
        lock_guard<mutex> lg(painelMtx_);

        if (painelLigado_)
        {
            return;
        }

        painelParar_  = false;
        painelLigado_ = true;
    }
    painel_ = thread(&MesaDeDJ::loopPainel, this);
}

void MesaDeDJ::pararPainel()
{
    {
        lock_guard<mutex> lg(painelMtx_);
        if (!painelLigado_) 
        {
            return;
        }

        painelParar_ = true;
    }
    painelCv_.notify_all();

    if (painel_.joinable())
    {
        painel_.join();
    }

    lock_guard<mutex> lg(painelMtx_);
    painelLigado_ = false;
}

bool MesaDeDJ::painelLigado() const
{
    lock_guard<mutex> lg(painelMtx_);
    return painelLigado_;
}

void MesaDeDJ::loopPainel()
{
    while (true)
    {
        desenhar();

        unique_lock<mutex> lock(painelMtx_);
        // Espera 2 segundos, mas acorda na hora se mandarem parar.
        painelCv_.wait_for(lock, chrono::seconds(2),
                           [this] { return painelParar_; });

        if (painelParar_) 
        {
            return;
        }
    }
}

void MesaDeDJ::desenhar() const
{
    // Monta o texto inteiro ANTES de travar o console: assim o lock fica
    // preso pelo menor tempo possivel.
    auto faixas = todas();

    ostringstream out;
    out << "+==============================================================+\n"
        << "|                    M E S A   D E   D J  🎧🎤                  |\n"
        << "+--------------+-----------+-------+--------+------------------+\n"
        << "| FAIXA        | ESTADO    |  BPM  |  VOL   | BATIDAS          |\n"
        << "+--------------+-----------+-------+--------+------------------+\n";

    if (faixas.empty())
    {
        out << "|            (nenhuma faixa carregada ainda)                      |\n";
    }

    for (const auto& f : faixas)
    {
        Instrumento::Status s = f->status();

        // Barrinha de volume, so pro painel ficar bonito.
        string barra;
        int cheios = s.volume / 20;

        for (int i = 0; i < 5; ++i) barra += (i < cheios ? '#' : '.');

        out << " | " << left  << setw(12) << s.nome.substr(0, 12)
            << " | " << setw(9) << paraTexto(s.estado)
            << " | " << right << setw(5) << s.bpm
            << " | " << barra << " "
            << " | " << setw(16) << s.batidas << " |\n";

        if (!s.comSom)
        {
            out << " |   ^ sem áudio (arquivo não carregado)                        |\n";
        }
    }

    out << "+--------------+-----------+-------+--------+------------------+\n"
        << " play <faixa> | pause <faixa> | stop <faixa> | bpm <faixa> <n>\n"
        << " vol <faixa> <0-100> | add <nome> <arquivo.wav> [bpm]\n"
        << " play all | pause all | list | painel on|off | ajuda | sair\n\n"
        << "DJ> ";

    lock_guard<mutex> lg(Console::mutexTela());
    Console::limpar();
    cout << out.str() << flush;
}