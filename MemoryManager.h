#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "TuringMachine.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Описание блока памяти на ленте
struct Variable {
  std::string name; // Имя в высокоуровневом коде (например, "counter")
  char address; // Односимвольный физический адрес на ленте ('a', 'b', 'c'...)
  int initialValue;       // Стартовое значение (количество единиц)
  size_t padding = 10000; // Зарезервированное пустое пространство (отступ)
};

class MemoryManager {
private:
  std::vector<Variable> variables_;
  std::unordered_map<std::string, char> symbolTable_;
  char nextAddress_ = 'a'; // Доступное адресное пространство: 'a' - 'z'

  // Стандартный запас прочности для роста переменных
  static constexpr size_t DEFAULT_PADDING = 15;

public:
  // Регистрация переменной в памяти
  void Allocate(const std::string &name, int initialValue = 0,
                size_t padding = DEFAULT_PADDING);

  // Разрешение имени (получение физического адреса для макросов)
  char GetAddress(const std::string &name) const;

  // Физическая "прошивка" памяти на ленту МТ
  void Deploy(TuringMachine &tm) const;

  // Декодирование унарного значения с ленты МТ в C++ int
  int GetDecimalValue(const TuringMachine &tm, const std::string &name) const;
};

#endif // MEMORY_MANAGER_H
