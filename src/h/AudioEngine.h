#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <string>

// ---------------------------------------------------------------------------
// Configuracao da miniaudio: o projeto so precisa dela pra tocar arquivos
// .wav (ver samples/*.wav) atraves da API de alto nivel (ma_engine/ma_sound).
// As macros abaixo desativam, em tempo de compilacao, so o que o projeto
// comprovadamente nao usa -- o codigo da lib continua intacto, ninguem mexeu
// em miniaudio.h.
//
// Elas TEM que ficar aqui, antes do #include, e nao em AudioEngine.cpp: toda
// unidade de traducao que inclui este header (Instrumento.cpp, MesaDeDJ.cpp,
// main.cpp...) precisa ver exatamente as mesmas macros, senao os tipos da
// miniaudio ficam com layout diferente em cada .cpp -> ODR violation.
//
//  * codecs/geracao: nenhum arquivo em samples/ e mp3/flac/ogg, e o projeto
//    nunca chama ma_encoder nem ma_waveform/ma_noise.
//  * backends fora do escopo do CMakeLists (Windows/Linux/macOS): Android,
//    Web e as variantes BSD.
//  * backends legados do Windows: o projeto ja assume Windows 10+ (ver
//    Console::habilitarAnsi(), que so faz sentido nesse SO), entao WASAPI
//    sozinho basta.
//  * JACK no Linux: nicho de audio profissional: ALSA e PulseAudio cobrem
//    o caso comum e continuam ativos.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// AudioEngine
// Responsabilidade unica: conversar com a placa de som.
// Nenhuma regra de negocio da mesa de DJ mora aqui.
// ---------------------------------------------------------------------------
class AudioEngine
{
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
        bool ok_ = false;
};

// ---------------------------------------------------------------------------
// Amostra
// Um arquivo .wav ja decodificado na memoria, pronto para ser disparado.
// Decodificar uma vez no carregamento evita I/O de disco a cada batida
// (I/O dentro do loop faria a batida "engasgar" e atrasar o BPM).
// ---------------------------------------------------------------------------
class Amostra
{
    public:
        Amostra() = default;
        ~Amostra();

        Amostra(const Amostra&) = delete;
        Amostra& operator=(const Amostra&) = delete;

        bool carregar(AudioEngine& engine, const std::string& caminho);
        void disparar(); // toca do inicio (re-trigger)
        void definirVolume(float v); // 0.0 a 1.0
        bool carregada() const { return carregada_; }

    private:
        ma_sound som_{};
        bool carregada_ = false;
};
#endif