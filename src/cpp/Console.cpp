#include "Console.h"

#include <iostream>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

namespace Console
{

std::mutex& mutexTela()
{
    // Singleton "Meyers": criado na primeira chamada, thread-safe por padrao
    // desde o C++11.
    static std::mutex m;
    return m;
}

void habilitarAnsi()
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &modo))
    {
        SetConsoleMode(h, modo | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void limpar()
{
    // \033[2J limpa a tela, \033[H leva o cursor para a posicao (1,1).
    std::cout << "\033[2J\033[H";
}

} // namespace Console