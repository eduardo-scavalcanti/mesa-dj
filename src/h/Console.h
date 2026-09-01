#ifndef CONSOLE_H
#define CONSOLE_H
#include <mutex>

using namespace std;

namespace Console
{

mutex& mutexTela();

void habilitarAnsi();

void limpar();

} // namespace Console

#endif // CONSOLE_H
