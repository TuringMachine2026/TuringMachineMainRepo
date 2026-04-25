#include "Compiler.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main() {
  TuringMachine tm("start");
  MemoryManager mem;
  Compiler compiler(tm, mem);

  std::string filename = "program.txt";
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "[Ошибка] Не удалось открыть файл: " << filename << "\n";
    return 1;
  }

  std::vector<std::string> sourceCode;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      sourceCode.push_back(line);
    }
  }
  file.close();

  std::cout << "[Compiler] Считывание кода завершено. Трансляция...\n";
  compiler.Compile(sourceCode);
  mem.Deploy(tm);

  std::cout << "[Processor] Выполнение программы:\n";
  std::cout << "------------------------------------\n";

  // Запускаем исполнение через ОС компилятора, чтобы перехватывать print
  compiler.Execute();

  std::cout << "------------------------------------\n";
  std::cout << "[Processor] Программа успешно завершена.\n";

  return 0;
}