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
    encerrar();   // RAII: a thread nunca vaza, mesmo se houver excecao
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
    } // já iniciado

    estado_ = Estado::Tocando;
    thread_ = thread(&Instrumento::loop, this);
    // A thread nova vai tentar pegar mtx_ e ficar bloqueada ate sairmos
    // deste escopo. Nao ha deadlock: liberamos o lock em seguida.
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
    cv_.notify_all();   // notifica FORA do lock: a thread acordada nao
                        // precisa esperar a gente soltar o mutex
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
    cv_.notify_all();   // acorda a thread mesmo se ela estiver pausada

    // O join() acontece FORA do lock. Se tentassemos dar join segurando
    // mtx_, a thread nunca conseguiria adquirir o mutex para terminar
    // -> deadlock classico.

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
        volume_     = clamp(volume, 0, 100);
        volumeSujo_ = true;
    }
    cv_.notify_all();
}

Instrumento::Status Instrumento::status() const
{
    lock_guard<mutex> lg(mtx_);
    // Copiamos tudo de uma vez: quem recebe o Status ve um retrato coerente,
    // nunca um "meio caminho" entre duas alteracoes.
    return Status{nome_, arquivo_, estado_, bpm_, volume_, batidas_, amostra_.carregada()};
}

long Instrumento::intervaloMs() const
{
    return 60000L / bpm_; // BPM -> milissegundos entre batidas
}

// ---------------------------------------------------------------------------
// O CORACAO DO PROJETO: o loop que roda na thread do instrumento.
// ---------------------------------------------------------------------------
void Instrumento::loop()
{
    while (true)
    {
        int  volumeParaAplicar = -1;
        long esperaMs          = 0;

        // ---- Secao critica 1: decidir se toca ----
        {
            unique_lock<mutex> lock(mtx_);

            // Dorme aqui enquanto estiver pausado. O predicado tambem protege
            // contra "spurious wakeups" (a cv pode acordar sozinha).
            
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
        // ---- lock liberado ANTES de tocar ----
        // Manter o mutex durante o disparo do audio seguraria os comandos do
        // DJ sem necessidade. Regra de ouro: lock curto, so em torno do dado.

        if (volumeParaAplicar >= 0) 
        {
            amostra_.definirVolume(volumeParaAplicar / 100.0f);
        }

        amostra_.disparar();

        // ---- Secao critica 2: dormir o intervalo do BPM ----
        {
            unique_lock<mutex> lock(mtx_);

            // Em vez de sleep_for (que ficaria "surdo"), usamos wait_for:
            // se o DJ mandar pausar ou sair, acordamos na hora, sem esperar
            // a batida terminar.
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