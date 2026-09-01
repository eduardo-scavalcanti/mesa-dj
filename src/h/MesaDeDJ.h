#ifndef MESA_DE_DJ_H
#define MESA_DE_DJ_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Instrumento.h"

// ---------------------------------------------------------------------------
// MesaDeDJ
// Guarda as faixas e roda a thread do painel ao vivo.
//
// SEGUNDO PONTO DE SINCRONIZACAO DO PROJETO:
// o vector de faixas e lido pela thread do painel a cada 2s enquanto a thread
// de comandos pode inserir/remover itens ("add guitarra" com a musica rodando).
// Sem o faixasMtx_, um push_back que realoca o vector invalidaria o iterador
// do painel -> crash aleatorio, dificil de reproduzir.
//
// ORDEM DE TRAVAS (para nunca ter deadlock):
//   faixasMtx_  ->  Instrumento::mtx_
// Nunca o contrario. Na pratica: pegamos os shared_ptr sob faixasMtx_,
// soltamos o lock, e so entao chamamos metodos do Instrumento.
// ---------------------------------------------------------------------------
class MesaDeDJ
{
public:
    explicit MesaDeDJ(AudioEngine& audio);
    ~MesaDeDJ();

    MesaDeDJ(const MesaDeDJ&) = delete;
    MesaDeDJ& operator=(const MesaDeDJ&) = delete;

    // Cria, carrega o .wav e ja poe a thread para rodar.
    bool adicionar(const std::string& nome, const std::string& arquivo,
                   int bpm, std::string& erro);

    bool remover(const std::string& nome);

    std::shared_ptr<Instrumento> buscar(const std::string& nome) const;
    std::vector<std::shared_ptr<Instrumento>> todas() const;

    void tocarTodas();
    void pausarTodas();
    void encerrarTudo();

    void iniciarPainel();
    void pararPainel();
    bool painelLigado() const;

    // Desenha o painel uma unica vez (usado pelo comando "list").
    void desenhar() const;

private:
    void loopPainel();

    AudioEngine& audio_;

    mutable std::mutex faixasMtx_;
    std::vector<std::shared_ptr<Instrumento>> faixas_;

    std::thread             painel_;
    mutable std::mutex      painelMtx_;
    std::condition_variable painelCv_;
    bool                    painelParar_  = false;
    bool                    painelLigado_ = false;
};

#endif // MESA_DE_DJ_H