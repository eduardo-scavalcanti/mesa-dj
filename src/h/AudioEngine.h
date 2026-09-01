#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <string>

using namespace std;

#define MA_NO_ENCODING
#define MA_NO_MP3
#define MA_NO_FLAC
#define MA_NO_VORBIS
#define MA_NO_GENERATION

#define MA_NO_DSOUND
#define MA_NO_WINMM
#define MA_NO_JACK
#define MA_NO_OSS
#define MA_NO_SNDIO
#define MA_NO_AUDIO4
#define MA_NO_AAUDIO
#define MA_NO_OPENSL
#define MA_NO_WEBAUDIO

#include "miniaudio.h"

class AudioEngine
{
    public:
            AudioEngine() = default;
            ~AudioEngine();

        AudioEngine(const AudioEngine&)            = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        bool iniciar();
        void finalizar();
        bool pronto() const { return ok_; }
        ma_engine* nativo() { return &engine_; }

    private:
        ma_engine engine_{};
        bool ok_ = false;
};

class Amostra
{
    public:
        Amostra() = default;
        ~Amostra();

        Amostra(const Amostra&) = delete;
        Amostra& operator=(const Amostra&) = delete;

        bool carregar(AudioEngine& engine, const string& caminho);
        void disparar();
        void definirVolume(float v);
        bool carregada() const { return carregada_; }

    private:
        ma_sound som_{};
        bool carregada_ = false;
};
#endif
