#ifndef TURING_MACHINE_H
#define TURING_MACHINE_H
#include <iostream>
#include <map>
#include <string>

enum class Direction { Left = -1, Stay = 0, Right = 1 };

// Структура, описывающая реакцию машины (что делать дальше)
struct Action {
  std::string nextState;
  char writeSymbol;
  Direction move;
};

class TuringMachine {
private:
  // временная лента. Потом тип LazySequence<char>
  std::map<int, char> tape;

  int head;                 // Текущая позиция каретки
  std::string currentState; // Текущее состояние машины

  // Ключ: пара <Текущее состояние, Читаемый символ>
  std::map<std::pair<std::string, char>, Action> rules;

  // Чтение символа с ленты (если ячейка пуста, возвращаем пробел/пустышку '_')
  char ReadTape() {
    if (tape.find(head) == tape.end()) {
      return '_';
    }
    return tape[head];
  }

public:
  // Конструктор
  TuringMachine(const std::string &startState = "q0") {
    head = 0;
    currentState = startState;
  }

  // Метод для добавления правила (его потом будет массово вызывать наш
  // компилятор)
  void AddRule(const std::string &state, char readSym,
               const std::string &nextState, char writeSym, Direction dir) {
    rules[{state, readSym}] = {nextState, writeSym, dir};
  }

  // Записать начальные данные на ленту
  void SetTapeContent(int position, char symbol) { tape[position] = symbol; }

  // Предоставляет доступ к памяти только для чтения (для MemoryManager)
  const std::map<int, char> &GetTape() const { return tape; }

  // Один такт работы машины
  bool Step() {
    if (currentState == "halt") {
      return false;
    }

    char currentSymbol = ReadTape();
    auto key = std::make_pair(currentState, currentSymbol);

    auto it = rules.find(key);
    if (it == rules.end()) {
      // НЕЯВНАЯ ПРОВЕРКА: Спасает от зависания, если компилятор забыл дописать
      // правила
      std::cerr << "Ошибка исполнения МТ: Нет правила для состояния '"
                << currentState << "' и символа '" << currentSymbol << "'\n";
      return false;
    }

    Action action = it->second;
    tape[head] = action.writeSymbol;
    head += static_cast<int>(action.move);
    currentState = action.nextState;

    return true;
  }
  // Отдает текущее состояние машины (для системных прерываний)
  std::string GetCurrentState() const { return currentState; }

  // Запуск машины до остановки
  void Run() {
    while (Step()) {
      // Цикл крутится, пока Step() возвращает true
    }
  }

  // Временный метод для вывода ленты (пока нет графики)
  void PrintTape() {
    std::cout << "Состояние: " << currentState << " | Каретка на: " << head
              << "\nЛента: ";
    for (auto const &[pos, symbol] : tape) {
      std::cout << "[" << pos << "]=" << symbol << " ";
    }
    std::cout << "\n\n";
  }
};
#endif // TURING_MACHINE_H