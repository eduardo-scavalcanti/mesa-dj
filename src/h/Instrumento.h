#ifndef INSTRUMENTO_H
#define INSTRUMENTO_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "AudioEngine.h"

using namespace std;

enum class Estado
{
    Parado,
    Tocando,
    Pausado
};
string paraTexto(Estado e);

class Instrumento
{
public:
    typedef struct Status
    {
        string        nome;
        string        arquivo;
        Estado        estado;
        int           bpm;
        int           volume;
        unsigned long batidas;
        bool          comSom;
    } Status;

    Instrumento(string nome, string arquivo, int bpm, AudioEngine& audio);
    ~Instrumento();

    Instrumento(const Instrumento&)            = delete;
    Instrumento& operator=(const Instrumento&) = delete;

    bool carregarAmostra();
    void iniciar();
    void tocar();
    void pausar();
    void encerrar();

    void definirBpm(int bpm);
    void definirVolume(int volume);

    Status status() const;
    const string& nome() const { return nome_; }

    static constexpr int BPM_MIN = 20;
    static constexpr int BPM_MAX = 300;

private:
    void loop();
    long intervaloMs() const;

    const string nome_;
    string       arquivo_;
    AudioEngine& audio_;
    Amostra      amostra_;

    mutable mutex      mtx_;
    condition_variable cv_;
    Estado                  estado_      = Estado::Parado;
    bool                    encerrar_    = false;
    int                     bpm_;
    int                     volume_      = 80;
    bool                    volumeSujo_  = true;
    unsigned long           batidas_     = 0;

    thread thread_;
};

#endif // INSTRUMENTO_H
