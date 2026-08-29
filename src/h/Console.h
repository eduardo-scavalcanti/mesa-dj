#ifndef CONSOLE_H
#define CONSOLE_H

#include <mutex>

// ---------------------------------------------------------------------------
// O console (std::cout) tambem e um RECURSO COMPARTILHADO.
// Se a thread do painel e a thread de comandos escreverem ao mesmo tempo,
// o texto sai embaralhado. Por isso toda escrita passa por este mutex.
// ---------------------------------------------------------------------------
namespace Console
{

// Mutex global do console. Use sempre com std::lock_guard.
std::mutex& mutexTela();

// Habilita codigos ANSI no Windows 10+ (no Linux/macOS ja funcionam).
void habilitarAnsi();

// Limpa a tela e joga o cursor no canto superior esquerdo.
// ATENCAO: chame ja segurando o mutexTela().
void limpar();

} // namespace Console

#endif // CONSOLE_H
