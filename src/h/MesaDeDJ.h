#ifndef MESA_DE_DJ_H
#define MESA_DE_DJ_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Instrumento.h"

using namespace std;

class MesaDeDJ
{
public:
    explicit MesaDeDJ(AudioEngine& audio);
    ~MesaDeDJ();

    MesaDeDJ(const MesaDeDJ&) = delete;
    MesaDeDJ& operator=(const MesaDeDJ&) = delete;

    bool adicionar(const string& nome, const string& arquivo,
    int bpm, string& erro);

    bool remover(const string& nome);

    shared_ptr<Instrumento> buscar(const string& nome) const;
    vector<shared_ptr<Instrumento>> todas() const;

    void tocarTodas();
    void pausarTodas();
    void encerrarTudo();

    void iniciarPainel();
    void pararPainel();
    bool painelLigado() const;

    void desenhar() const;

private:
    void loopPainel();

    AudioEngine& audio_;

    mutable mutex faixasMtx_;
    vector<shared_ptr<Instrumento>> faixas_;

    thread             painel_;
    mutable mutex      painelMtx_;
    condition_variable painelCv_;
    bool                    painelParar_  = false;
    bool                    painelLigado_ = false;
};

#endif // MESA_DE_DJ_H
