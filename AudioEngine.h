#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <string>
#include "miniaudio.h"

// ---------------------------------------------------------------------------
// AudioEngine
// Responsabilidade unica: conversar com a placa de som.
// Nenhuma regra de negocio da mesa de DJ mora aqui.
// ---------------------------------------------------------------------------
class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    // O engine controla um recurso do sistema operacional: nao pode ser copiado.
    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool iniciar();
    void finalizar();
    bool pronto() const { return ok_; }
    ma_engine* nativo() { return &engine_; }

private:
    ma_engine engine_{};
    bool      ok_ = false;
};

// ---------------------------------------------------------------------------
// Amostra
// Um arquivo .wav ja decodificado na memoria, pronto para ser disparado.
// Decodificar uma vez no carregamento evita I/O de disco a cada batida
// (I/O dentro do loop faria a batida "engasgar" e atrasar o BPM).
// ---------------------------------------------------------------------------
class Amostra {
public:
    Amostra() = default;
    ~Amostra();

    Amostra(const Amostra&)            = delete;
    Amostra& operator=(const Amostra&) = delete;

    bool carregar(AudioEngine& engine, const std::string& caminho);
    void disparar();                    // toca do inicio (re-trigger)
    void definirVolume(float v);        // 0.0 a 1.0
    bool carregada() const { return carregada_; }

private:
    ma_sound som_{};
    bool     carregada_ = false;
};

#endif // AUDIO_ENGINE_H
