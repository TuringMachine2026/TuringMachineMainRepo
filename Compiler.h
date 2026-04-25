#ifndef COMPILER_H
#define COMPILER_H

#include "MacrosForTuring.h"
#include "MemoryManager.h"
#include "TuringMachine.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct BlockScope {
  enum Type { WHILE, FOR, IF, ELSE } type;
  std::string startState;
  std::string endState;
  std::string nextBlockState;
  std::vector<std::string> stepTokens;
};

struct PrintHook {
  std::string displayName;
  std::string internalName;
};

class Compiler {
private:
  TuringMachine &tm_;
  MemoryManager &mem_;
  int stateCounter_ = 0;
  std::vector<BlockScope> scopes_;
  std::unordered_map<std::string, PrintHook> printHooks_;

  std::string NextState() {
    return "q_auto_" + std::to_string(++stateCounter_);
  }

  std::vector<std::string> Tokenize(const std::string &line) {
    std::vector<std::string> tokens;
    std::string token;
    for (size_t i = 0; i < line.length(); ++i) {
      if (std::isspace(line[i])) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
      } else if (i + 1 < line.length() &&
                 (line.substr(i, 2) == "++" || line.substr(i, 2) == "--")) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
        tokens.push_back(line.substr(i, 2));
        ++i;
      } else if (std::string("=+-*(){};<>").find(line[i]) !=
                 std::string::npos) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
        tokens.push_back(std::string(1, line[i]));
      } else {
        token += line[i];
      }
    }
    if (!token.empty())
      tokens.push_back(token);
    return tokens;
  }

  bool IsNumber(const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
  }

  char GetAddressFor(const std::string &token) {
    if (IsNumber(token))
      return mem_.GetAddress("_c" + token);
    return mem_.GetAddress(token);
  }

  void CompileStatement(const std::vector<std::string> &tokens,
                        const std::string &currentState,
                        const std::string &nextState) {
    if (tokens[0] == "print" && tokens.size() == 2) {
      std::string printState = NextState() + "_print";
      std::string internalName =
          IsNumber(tokens[1]) ? "_c" + tokens[1] : tokens[1];
      printHooks_[printState] = {tokens[1], internalName};
      tm_.AddRule(currentState, '^', printState, '^', Direction::Stay);
      tm_.AddRule(printState, '^', nextState, '^', Direction::Stay);
    } else if (tokens.size() == 2 && tokens[1] == "++") {
      GenerateIncrement(tm_, currentState, GetAddressFor(tokens[0]), nextState);
    } else if (tokens.size() == 2 && tokens[1] == "--") {
      GenerateDecrement(tm_, currentState, GetAddressFor(tokens[0]), nextState);
    } else if (tokens.size() == 3 && tokens[1] == "=") {
      if (tokens[0] != tokens[2]) {
        GenerateAssign(tm_, currentState, GetAddressFor(tokens[0]),
                       GetAddressFor(tokens[2]), nextState);
      } else {
        tm_.AddRule(currentState, '^', nextState, '^', Direction::Stay);
      }
    } else if (tokens.size() == 5 && tokens[1] == "=") {
      std::string dest = tokens[0], arg1 = tokens[2], op = tokens[3],
                  arg2 = tokens[4];
      if (op == "+") {
        if (dest == arg1)
          GenerateAdd(tm_, currentState, GetAddressFor(dest),
                      GetAddressFor(arg2), nextState);
        else if (dest == arg2)
          GenerateAdd(tm_, currentState, GetAddressFor(dest),
                      GetAddressFor(arg1), nextState);
        else {
          std::string mid = currentState + "_m";
          GenerateAssign(tm_, currentState, GetAddressFor(dest),
                         GetAddressFor(arg1), mid);
          GenerateAdd(tm_, mid, GetAddressFor(dest), GetAddressFor(arg2),
                      nextState);
        }
      } else if (op == "-") {
        if (dest == arg1)
          GenerateSubtract(tm_, currentState, GetAddressFor(dest),
                           GetAddressFor(arg2), nextState);
        else {
          std::string mid = currentState + "_m";
          GenerateAssign(tm_, currentState, GetAddressFor(dest),
                         GetAddressFor(arg1), mid);
          GenerateSubtract(tm_, mid, GetAddressFor(dest), GetAddressFor(arg2),
                           nextState);
        }
      } else if (op == "*") {
        std::string t1 = currentState + "_t1", t2 = currentState + "_t2";
        GenerateClear(tm_, currentState, mem_.GetAddress("_temp"), t1);
        GenerateMultiply(tm_, t1, mem_.GetAddress("_temp"), GetAddressFor(arg1),
                         GetAddressFor(arg2), t2);
        GenerateAssign(tm_, t2, GetAddressFor(dest), mem_.GetAddress("_temp"),
                       nextState);
      }
    }
  }

public:
  Compiler(TuringMachine &tm, MemoryManager &mem) : tm_(tm), mem_(mem) {}

  void Compile(const std::vector<std::string> &sourceCode) {
    // Внутренний регистр ALU всегда должен быть самым большим (int)
    try {
      mem_.Allocate("_temp", 0, 10000);
    } catch (...) {
    }
    try {
      mem_.Allocate("_c0", 0, 0);
    } catch (...) {
    } // Константы не растут, им не нужен буфер!

    // ====================================================================
    // ПАС 1: Строгая Система Типов (Strict Typing Allocation)
    // ====================================================================
    for (const auto &line : sourceCode) {
      auto tokens = Tokenize(line);
      for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "bool") {
          if (i + 1 < tokens.size())
            try {
              mem_.Allocate(tokens[i + 1], 0, 1);
            } catch (...) {
            }
        } else if (tokens[i] == "byte") {
          if (i + 1 < tokens.size())
            try {
              mem_.Allocate(tokens[i + 1], 0, 255);
            } catch (...) {
            }
        } else if (tokens[i] == "int") {
          if (i + 1 < tokens.size())
            try {
              mem_.Allocate(tokens[i + 1], 0, 10000);
            } catch (...) {
            }
        } else if (IsNumber(tokens[i])) {
          // Выделяем скрытые константы с буфером 0
          try {
            mem_.Allocate("_c" + tokens[i], std::stoi(tokens[i]), 0);
          } catch (...) {
          }
        }
      }
    }

    // ====================================================================
    // ПАС 2: Генерация AST
    // ====================================================================
    std::string currentState = "start";
    for (const auto &line : sourceCode) {
      auto tokens = Tokenize(line);
      if (tokens.empty())
        continue;

      // МАГИЯ: Стираем ключевые слова типов, чтобы код вида "int x = 5"
      // превратился в "x = 5" и наш парсер разобрал его как раньше!
      auto it = std::remove_if(
          tokens.begin(), tokens.end(), [](const std::string &t) {
            return t == "bool" || t == "byte" || t == "int";
          });
      tokens.erase(it, tokens.end());

      if (tokens.empty())
        continue;

      if (tokens[0] == "while") {
        std::string cond = NextState() + "_c";
        std::string body = NextState() + "_b";
        std::string end = NextState() + "_e";
        tm_.AddRule(currentState, '^', cond, '^', Direction::Stay);
        GenerateCompare(tm_, cond, GetAddressFor(tokens[2]), GetAddressFor("0"),
                        body, end, end);
        scopes_.push_back({BlockScope::WHILE, cond, end, "", {}});
        currentState = body;
      } else if (tokens[0] == "for") {
        auto it1 = std::find(tokens.begin(), tokens.end(), ";");
        auto it2 = std::find(it1 + 1, tokens.end(), ";");
        auto it3 = std::find(it2 + 1, tokens.end(), ")");
        if (it1 == tokens.end() || it2 == tokens.end() || it3 == tokens.end())
          continue;

        std::vector<std::string> initTokens(tokens.begin() + 2, it1);
        std::vector<std::string> condTokens(it1 + 1, it2);
        std::vector<std::string> stepTokens(it2 + 1, it3);

        std::string afterInit = NextState() + "_for_init";
        CompileStatement(initTokens, currentState, afterInit);
        currentState = afterInit;

        std::string cond = NextState() + "_for_cond";
        std::string body = NextState() + "_for_body";
        std::string end = NextState() + "_for_end";

        tm_.AddRule(currentState, '^', cond, '^', Direction::Stay);

        std::string leftArg = condTokens[0], op = condTokens[1],
                    rightArg = condTokens[2];
        if (op == "<")
          GenerateCompare(tm_, cond, GetAddressFor(leftArg),
                          GetAddressFor(rightArg), end, body, end);
        else if (op == ">")
          GenerateCompare(tm_, cond, GetAddressFor(leftArg),
                          GetAddressFor(rightArg), body, end, end);

        scopes_.push_back({BlockScope::FOR, cond, end, "", stepTokens});
        currentState = body;
      } else if (tokens[0] == "if") {
        std::string cond = NextState() + "_if_c";
        std::string body = NextState() + "_if_b";
        std::string nextBlock = NextState() + "_if_next";
        std::string endChain = NextState() + "_if_end";
        tm_.AddRule(currentState, '^', cond, '^', Direction::Stay);
        GenerateCompare(tm_, cond, GetAddressFor(tokens[2]), GetAddressFor("0"),
                        body, nextBlock, nextBlock);
        scopes_.push_back({BlockScope::IF, cond, endChain, nextBlock, {}});
        currentState = body;
      } else if (tokens[0] == "}" && tokens.size() > 1 && tokens[1] == "else") {
        BlockScope scope = scopes_.back();
        scopes_.pop_back();
        tm_.AddRule(currentState, '^', scope.endState, '^', Direction::Stay);
        if (tokens.size() > 2 && tokens[2] == "if") {
          std::string body = NextState() + "_elif_b";
          std::string nextBlock = NextState() + "_elif_next";
          GenerateCompare(tm_, scope.nextBlockState, GetAddressFor(tokens[4]),
                          GetAddressFor("0"), body, nextBlock, nextBlock);
          scopes_.push_back({BlockScope::IF,
                             scope.nextBlockState,
                             scope.endState,
                             nextBlock,
                             {}});
          currentState = body;
        } else {
          scopes_.push_back(
              {BlockScope::ELSE, scope.nextBlockState, scope.endState, "", {}});
          currentState = scope.nextBlockState;
        }
      } else if (tokens[0] == "}") {
        if (scopes_.empty())
          continue;
        BlockScope scope = scopes_.back();
        scopes_.pop_back();

        if (scope.type == BlockScope::WHILE) {
          tm_.AddRule(currentState, '^', scope.startState, '^',
                      Direction::Stay);
          currentState = scope.endState;
        } else if (scope.type == BlockScope::FOR) {
          std::string stepState = NextState() + "_for_step";
          CompileStatement(scope.stepTokens, currentState, stepState);
          currentState = stepState;
          tm_.AddRule(currentState, '^', scope.startState, '^',
                      Direction::Stay);
          currentState = scope.endState;
        } else if (scope.type == BlockScope::IF ||
                   scope.type == BlockScope::ELSE) {
          tm_.AddRule(currentState, '^', scope.endState, '^', Direction::Stay);
          if (scope.type == BlockScope::IF) {
            tm_.AddRule(scope.nextBlockState, '^', scope.endState, '^',
                        Direction::Stay);
          }
          currentState = scope.endState;
        }
      } else {
        std::string next = NextState();
        CompileStatement(tokens, currentState, next);
        currentState = next;
      }
    }
    tm_.AddRule(currentState, '^', "halt", '^', Direction::Stay);
  }

  void Execute() {
    while (tm_.GetCurrentState() != "halt") {
      std::string curr = tm_.GetCurrentState();

      if (printHooks_.count(curr)) {
        auto hook = printHooks_[curr];
        std::cout << ">> " << hook.displayName << " = "
                  << mem_.GetDecimalValue(tm_, hook.internalName) << "\n";
      }

      if (!tm_.Step()) {
        std::cerr << "[Hardware Fault] Неожиданная остановка процессора.\n";
        break;
      }
    }
  }
};

#endif // COMPILER_H