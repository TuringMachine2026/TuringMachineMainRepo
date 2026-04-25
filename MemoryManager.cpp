#include "MemoryManager.h"
#include <stdexcept>

void MemoryManager::Allocate(const std::string &name, int initialValue,
                             size_t padding) {
  if (symbolTable_.count(name)) {
    throw std::runtime_error("MemoryManager Error: Redefinition of variable '" +
                             name + "'");
  }
  if (nextAddress_ > 'z') {
    throw std::runtime_error(
        "MemoryManager Error: Out of address space ('a'-'z' exhausted)");
  }

  char address = nextAddress_++;
  symbolTable_[name] = address;
  variables_.push_back({name, address, initialValue, padding});
}

char MemoryManager::GetAddress(const std::string &name) const {
  auto it = symbolTable_.find(name);
  if (it == symbolTable_.end()) {
    throw std::runtime_error("MemoryManager Error: Undefined variable '" +
                             name + "'");
  }
  return it->second;
}

void MemoryManager::Deploy(TuringMachine &tm) const {
  // КРИТИЧЕСКИЙ ФИКС: Ставим "стену" в самое начало ленты.
  tm.SetTapeContent(0, '^');

  // Начинаем с индекса 1.
  int head = 1;

  for (const auto &var : variables_) {
    // Пишем заголовок переменной (например, #a:)
    tm.SetTapeContent(head++, '#');
    tm.SetTapeContent(head++, var.address);
    tm.SetTapeContent(head++, ':');

    // Пишем унарное значение
    for (int i = 0; i < var.initialValue; ++i) {
      tm.SetTapeContent(head++, '1');
    }

    // Закрываем переменную
    tm.SetTapeContent(head++, '#');

    // Формируем защитный интервал (Padding) для избежания buffer overflow
    for (size_t i = 0; i < var.padding; ++i) {
      tm.SetTapeContent(head++, '_');
    }
  }
}

int MemoryManager::GetDecimalValue(const TuringMachine &tm,
                                   const std::string &name) const {
  char address = GetAddress(name);
  const auto &tape = tm.GetTape();

  bool inVariableBlock = false;
  int value = 0;

  for (const auto &[pos, symbol] : tape) {
    // Нашли маркер нашей переменной
    if (symbol == address) {
      inVariableBlock = true;
      continue;
    }

    // Если мы внутри переменной
    if (inVariableBlock) {
      if (symbol == ':')
        continue;
      if (symbol == '1') {
        value++; // Считаем унарные единицы
      }
      if (symbol == '#') {
        break; // Конец блока переменной
      }
    }
  }
  return value;
}