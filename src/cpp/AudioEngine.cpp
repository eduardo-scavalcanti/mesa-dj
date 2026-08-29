// A miniaudio e "header-only": a implementacao dela precisa ser compilada
// em EXATAMENTE UM arquivo .cpp do projeto. Escolhemos este.
#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"
#include <iostream>

// ----------------------------- AudioEngine ---------------------------------

bool AudioEngine::iniciar() 
{
    if (ok_) return true;
    if (ma_engine_init(nullptr, &engine_) != MA_SUCCESS) 
    {
        std::cerr << "[áudio] Não foi possivel abrir o dispositivo de áudio.\n"
                  << "[áudio] A mesa vai rodar em modo silencioso (as batidas "
                     "continuam sendo contadas no painel).\n";
        return false;
    }
    ok_ = true;
    return true;
}

void AudioEngine::finalizar() 
{
    if (ok_) 
    {
        ma_engine_uninit(&engine_);
        ok_ = false;
    }
}

AudioEngine::~AudioEngine() 
{ 
    finalizar(); 
}

// ------------------------------- Amostra -----------------------------------

bool Amostra::carregar(AudioEngine& engine, const std::string& caminho) 
{
    if (!engine.pronto()) return false;

    if (carregada_) 
    {
        ma_sound_uninit(&som_);
        carregada_ = false;
    }

    const ma_uint32 flags = MA_SOUND_FLAG_DECODE // decodifica tudo agora
                          | MA_SOUND_FLAG_NO_SPATIALIZATION; // som 2D simples

    if (ma_sound_init_from_file(engine.nativo(), caminho.c_str(),
                                flags, nullptr, nullptr, &som_) != MA_SUCCESS) 
    {
        return false;
    }

    carregada_ = true;
    return true;
}

void Amostra::disparar() 
{
    if (!carregada_) 
    {
        return;
    }

    ma_sound_seek_to_pcm_frame(&som_, 0);  // volta pro inicio...
    ma_sound_start(&som_);  // ...e toca de novo
}

void Amostra::definirVolume(float v) 
{
    if (carregada_) ma_sound_set_volume(&som_, v);
}

Amostra::~Amostra() 
{
    if (carregada_) ma_sound_uninit(&som_);
}