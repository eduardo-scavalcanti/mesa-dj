#ifndef INSTRUMENTO_H
#define INSTRUMENTO_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "AudioEngine.h"

enum class Estado { Parado, Tocando, Pausado };
std::string paraTexto(Estado e);

// ---------------------------------------------------------------------------
// Instrumento = 1 faixa = 1 thread.
//
// A thread roda um loop: toca a amostra -> dorme o intervalo do BPM -> repete.
//
// PONTOS DE SINCRONIZACAO:
//  * mtx_ protege TODO o estado mutavel (estado_, bpm_, volume_, batidas_).
//  * cv_ e usada para pausar SEM espera ocupada (busy-wait). A thread fica
//    realmente bloqueada pelo SO, consumindo 0% de CPU, ate alguem sinalizar.
//  * encerrar_ e uma flag separada do estado: "pausar" nao mata a thread e
//    "encerrar" consegue acordar uma thread que esta pausada.
// ---------------------------------------------------------------------------
class Instrumento
{
public:
    // Fotografia consistente do estado, tirada de uma vez so sob o lock.
    typedef struct Status
    {
        std::string   nome;
        std::string   arquivo;
        Estado        estado;
        int           bpm;
        int           volume;
        unsigned long batidas;
        bool          comSom;
    } Status;

    Instrumento(std::string nome, std::string arquivo, int bpm, AudioEngine& audio);
    ~Instrumento();

    // Contem mutex e thread: nao pode ser copiado nem movido.
    Instrumento(const Instrumento&)            = delete;
    Instrumento& operator=(const Instrumento&) = delete;

    bool carregarAmostra();   // chame ANTES de iniciar()
    void iniciar();           // cria a thread e ja comeca tocando
    void tocar();             // retoma
    void pausar();            // pausa sem matar a thread
    void encerrar();          // sinaliza o fim e faz join

    void definirBpm(int bpm);
    void definirVolume(int volume);   // 0 a 100

    Status status() const;
    const std::string& nome() const { return nome_; }  // const: seguro sem lock

    static constexpr int BPM_MIN = 20;
    static constexpr int BPM_MAX = 300;

private:
    void loop();                    // corpo da thread
    long intervaloMs() const;       // requer o lock adquirido

    const std::string nome_;        // imutavel apos a construcao
    std::string       arquivo_;
    AudioEngine&      audio_;
    Amostra           amostra_;     // so a thread do instrumento a dispara

    // ---- estado compartilhado, sempre sob mtx_ ----
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    Estado                  estado_      = Estado::Parado;
    bool                    encerrar_    = false;
    int                     bpm_;
    int                     volume_      = 80;
    bool                    volumeSujo_  = true;   // precisa aplicar no audio?
    unsigned long           batidas_     = 0;
    // ----------------------------------------------

    std::thread thread_;
};

#endif // INSTRUMENTO_H