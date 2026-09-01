#include "Instrumento.h"
#include <algorithm>
#include <chrono>

using namespace std;

string paraTexto(Estado e)
{
    switch (e)
    {
        case Estado::Tocando:
        return "TOCANDO";

        case Estado::Pausado:
        return "PAUSADO";

        default:
        return "PARADO ";
    }
}

Instrumento::Instrumento(string nome, string arquivo, int bpm, AudioEngine& audio)
    :nome_(std::move(nome)),
    arquivo_(std::move(arquivo)),
    audio_(audio),
    bpm_(clamp(bpm, BPM_MIN, BPM_MAX)) {}

Instrumento::~Instrumento()
{
    encerrar();
}

bool Instrumento::carregarAmostra()
{
    return amostra_.carregar(audio_, arquivo_);
}

void Instrumento::iniciar()
{
    lock_guard<mutex> lg(mtx_);

    if (thread_.joinable())
    {
        return;
    }

    estado_ = Estado::Tocando;
    thread_ = thread(&Instrumento::loop, this);
}

void Instrumento::tocar()
{
    {
        lock_guard<mutex> lg(mtx_);

        if (encerrar_)
        {
            return;
        }

        estado_ = Estado::Tocando;
    }
    cv_.notify_all();
}

void Instrumento::pausar()
{
    {
        lock_guard<mutex> lg(mtx_);

        if (encerrar_)
        {
            return;
        }

        estado_ = Estado::Pausado;
    }
    cv_.notify_all();
}

void Instrumento::encerrar()
{
    {
        lock_guard<mutex> lg(mtx_);
        encerrar_ = true;
        estado_   = Estado::Parado;
    }
    cv_.notify_all();

    if (thread_.joinable())
    {
        thread_.join();
    }
}

void Instrumento::definirBpm(int bpm)
{
    {
        lock_guard<mutex> lg(mtx_);
        bpm_ = clamp(bpm, BPM_MIN, BPM_MAX);
    }
    cv_.notify_all();
}

void Instrumento::definirVolume(int volume)
{
    {
        lock_guard<mutex> lg(mtx_);
        volume_ = clamp(volume, 0, 100);
        volumeSujo_ = true;
    }
    cv_.notify_all();
}

Instrumento::Status Instrumento::status() const
{
    lock_guard<mutex> lg(mtx_);
    return Status{nome_, arquivo_, estado_, bpm_, volume_, batidas_, amostra_.carregada()};
}

long Instrumento::intervaloMs() const
{
    return 60000L / bpm_;
}

void Instrumento::loop()
{
    while (true)
    {
        int  volumeParaAplicar = -1;
        long esperaMs          = 0;

        {
            unique_lock<mutex> lock(mtx_);

            cv_.wait(lock, [this]
            {
                return encerrar_ || estado_ == Estado::Tocando;
            });

            if (encerrar_)
            {
                return;
            }

            if (volumeSujo_)
            {
                volumeParaAplicar = volume_;
                volumeSujo_       = false;
            }

            ++batidas_;
            esperaMs = intervaloMs();
        }

        if (volumeParaAplicar >= 0)
        {
            amostra_.definirVolume(volumeParaAplicar / 100.0f);
        }

        amostra_.disparar();

        {
            unique_lock<mutex> lock(mtx_);

            cv_.wait_for(lock, chrono::milliseconds(esperaMs), [this]
            {
                return encerrar_ || estado_ != Estado::Tocando;
            });

            if (encerrar_)
            {
                return;
            }
        }
    }
}
