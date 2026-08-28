#include "MesaDeDJ.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Console.h"

MesaDeDJ::MesaDeDJ(AudioEngine& audio) : audio_(audio) {}

MesaDeDJ::~MesaDeDJ() {
    pararPainel();
    encerrarTudo();
}

bool MesaDeDJ::adicionar(const std::string& nome, const std::string& arquivo,
                         int bpm, std::string& erro) {
    {
        std::lock_guard<std::mutex> lg(faixasMtx_);
        auto it = std::find_if(faixas_.begin(), faixas_.end(),
                               [&](const auto& f) { return f->nome() == nome; });
        if (it != faixas_.end()) {
            erro = "ja existe uma faixa chamada '" + nome + "'";
            return false;
        }
    }

    auto novo = std::make_shared<Instrumento>(nome, arquivo, bpm, audio_);

    // Carregar o .wav pode demorar alguns ms: fazemos isso FORA do lock,
    // enquanto o resto da mesa continua tocando normalmente.
    if (!novo->carregarAmostra() && audio_.pronto()) {
        erro = "nao consegui carregar o arquivo '" + arquivo + "'";
        return false;
    }

    novo->iniciar();

    {
        std::lock_guard<std::mutex> lg(faixasMtx_);
        faixas_.push_back(std::move(novo));
    }
    return true;
}

bool MesaDeDJ::remover(const std::string& nome) {
    std::shared_ptr<Instrumento> alvo;
    {
        std::lock_guard<std::mutex> lg(faixasMtx_);
        auto it = std::find_if(faixas_.begin(), faixas_.end(),
                               [&](const auto& f) { return f->nome() == nome; });
        if (it == faixas_.end()) return false;
        alvo = *it;              // segura o objeto vivo pelo shared_ptr
        faixas_.erase(it);
    }
    // encerrar() faz join na thread. Fazemos isso com o faixasMtx_ JA LIBERADO,
    // senao o painel ficaria travado esperando a faixa morrer.
    alvo->encerrar();
    return true;
}

std::shared_ptr<Instrumento> MesaDeDJ::buscar(const std::string& nome) const {
    std::lock_guard<std::mutex> lg(faixasMtx_);
    auto it = std::find_if(faixas_.begin(), faixas_.end(),
                           [&](const auto& f) { return f->nome() == nome; });
    return it == faixas_.end() ? nullptr : *it;
}

std::vector<std::shared_ptr<Instrumento>> MesaDeDJ::todas() const {
    std::lock_guard<std::mutex> lg(faixasMtx_);
    return faixas_;   // copia dos ponteiros: quem chama trabalha sem o lock
}

void MesaDeDJ::tocarTodas()  { for (auto& f : todas()) f->tocar();  }
void MesaDeDJ::pausarTodas() { for (auto& f : todas()) f->pausar(); }

void MesaDeDJ::encerrarTudo() {
    std::vector<std::shared_ptr<Instrumento>> copia;
    {
        std::lock_guard<std::mutex> lg(faixasMtx_);
        copia.swap(faixas_);
    }
    for (auto& f : copia) f->encerrar();
}

// --------------------------- painel ao vivo --------------------------------

void MesaDeDJ::iniciarPainel() {
    {
        std::lock_guard<std::mutex> lg(painelMtx_);
        if (painelLigado_) return;
        painelParar_  = false;
        painelLigado_ = true;
    }
    painel_ = std::thread(&MesaDeDJ::loopPainel, this);
}

void MesaDeDJ::pararPainel() {
    {
        std::lock_guard<std::mutex> lg(painelMtx_);
        if (!painelLigado_) return;
        painelParar_ = true;
    }
    painelCv_.notify_all();
    if (painel_.joinable()) painel_.join();

    std::lock_guard<std::mutex> lg(painelMtx_);
    painelLigado_ = false;
}

bool MesaDeDJ::painelLigado() const {
    std::lock_guard<std::mutex> lg(painelMtx_);
    return painelLigado_;
}

void MesaDeDJ::loopPainel() {
    while (true) {
        desenhar();

        std::unique_lock<std::mutex> lock(painelMtx_);
        // Espera 2 segundos, mas acorda na hora se mandarem parar.
        painelCv_.wait_for(lock, std::chrono::seconds(2),
                           [this] { return painelParar_; });
        if (painelParar_) return;
    }
}

void MesaDeDJ::desenhar() const {
    // Monta o texto inteiro ANTES de travar o console: assim o lock fica
    // preso pelo menor tempo possivel.
    auto faixas = todas();

    std::ostringstream out;
    out << "+==============================================================+\n"
        << "|                    M E S A   D E   D J                       |\n"
        << "+--------------+-----------+-------+--------+------------------+\n"
        << "| FAIXA        | ESTADO    |  BPM  |  VOL   | BATIDAS          |\n"
        << "+--------------+-----------+-------+--------+------------------+\n";

    if (faixas.empty()) {
        out << "|            (nenhuma faixa carregada)                         |\n";
    }

    for (const auto& f : faixas) {
        Instrumento::Status s = f->status();

        // Barrinha de volume, so pro painel ficar bonito.
        std::string barra;
        int cheios = s.volume / 20;
        for (int i = 0; i < 5; ++i) barra += (i < cheios ? '#' : '.');

        out << "| " << std::left  << std::setw(12) << s.nome.substr(0, 12)
            << " | " << std::setw(9) << paraTexto(s.estado)
            << " | " << std::right << std::setw(5) << s.bpm
            << " | " << barra << " "
            << " | " << std::setw(16) << s.batidas << " |\n";

        if (!s.comSom) {
            out << "|   ^ sem audio (arquivo nao carregado)                        |\n";
        }
    }

    out << "+--------------+-----------+-------+--------+------------------+\n"
        << " play <faixa> | pause <faixa> | stop <faixa> | bpm <faixa> <n>\n"
        << " vol <faixa> <0-100> | add <nome> <arquivo.wav> [bpm]\n"
        << " play all | pause all | list | painel on|off | ajuda | sair\n\n"
        << "DJ> ";

    std::lock_guard<std::mutex> lg(Console::mutexTela());
    Console::limpar();
    std::cout << out.str() << std::flush;
}
