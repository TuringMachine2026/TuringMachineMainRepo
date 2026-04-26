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
  std::string type;
};

class Compiler {
private:
  TuringMachine &tm_;
  MemoryManager &mem_;
  int stateCounter_ = 0;
  std::vector<BlockScope> scopes_;
  std::unordered_map<std::string, PrintHook> printHooks_;
  std::unordered_map<std::string, std::string> varTypes_;

  std::string NextState() {
    return "q_auto_" + std::to_string(++stateCounter_);
  }

  // Обновленный лексер с поддержкой "строк" и 'символов'
  std::vector<std::string> Tokenize(const std::string &line) {
    std::vector<std::string> tokens;
    std::string token;
    bool inStr = false;
    for (size_t i = 0; i < line.length(); ++i) {
      if (line[i] == '"' || line[i] == '\'') {
        token += line[i];
        if (inStr) {
          tokens.push_back(token);
          token.clear();
          inStr = false;
        } else {
          inStr = true;
        }
      } else if (inStr) {
        token += line[i];
      } else if (std::isspace(line[i])) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
      } else if (i + 1 < line.length() &&
                 (line.substr(i, 2) == "++" || line.substr(i, 2) == "--" ||
                  line.substr(i, 2) == "+=" || line.substr(i, 2) == "-=" ||
                  line.substr(i, 2) == "*=" || line.substr(i, 2) == "/=" ||
                  line.substr(i, 2) == "%=" || line.substr(i, 2) == "==" ||
                  line.substr(i, 2) == "!=" || line.substr(i, 2) == "<=" ||
                  line.substr(i, 2) == ">=")) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
        tokens.push_back(line.substr(i, 2));
        ++i;
      } else if (std::string("=+-*/%(){};<>").find(line[i]) !=
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
    if (s.empty())
      return false;
    bool hasDot = false;
    for (char c : s) {
      if (c == '.') {
        if (hasDot)
          return false;
        hasDot = true;
      } else if (!isdigit(c))
        return false;
    }
    return true;
  }

  bool IsCharLiteral(const std::string &s) {
    return s.length() == 3 && s.front() == '\'' && s.back() == '\'';
  }

  int ParseNumStr(const std::string &s) {
    if (s.find('.') == std::string::npos)
      return std::stoi(s);
    size_t dot = s.find('.');
    int ip = std::stoi(s.substr(0, dot));
    std::string fp = s.substr(dot + 1);
    while (fp.length() < 2)
      fp += "0";
    return ip * 100 + std::stoi(fp.substr(0, 2));
  }

  std::string GetConstantName(const std::string &token) {
    std::string clean = token;
    std::replace(clean.begin(), clean.end(), '.', '_');
    return "_c" + clean;
  }

  char GetAddressFor(const std::string &token) {
    if (IsNumber(token))
      return mem_.GetAddress(GetConstantName(token));
    if (IsCharLiteral(token))
      return mem_.GetAddress("_c" + std::to_string((int)token[1]));
    return mem_.GetAddress(token);
  }

  void CompileStatement(const std::vector<std::string> &tokens,
                        const std::string &currentState,
                        const std::string &nextState) {
    if (varTypes_.count(tokens[0]) && varTypes_[tokens[0]] == "int_complex") {
      std::string dest = tokens[0];
      if (tokens.size() == 3 && tokens[1] == "=") {
        std::string mid = NextState();
        CompileStatement({dest + "_r", "=", tokens[2] + "_r"}, currentState,
                         mid);
        CompileStatement({dest + "_i", "=", tokens[2] + "_i"}, mid, nextState);
        return;
      } else if (tokens.size() == 5 && tokens[1] == "=" &&
                 (tokens[3] == "+" || tokens[3] == "-")) {
        std::string mid = NextState();
        CompileStatement(
            {dest + "_r", "=", tokens[2] + "_r", tokens[3], tokens[4] + "_r"},
            currentState, mid);
        CompileStatement(
            {dest + "_i", "=", tokens[2] + "_i", tokens[3], tokens[4] + "_i"},
            mid, nextState);
        return;
      }
    }

    if (tokens[0] == "print" && tokens.size() >= 2) {
      std::string printArg = tokens[1];
      std::string printState = NextState() + "_print";
      std::string internalName = "";
      std::string type = "int";

      if (printArg.front() == '"') {
        type = "string_literal";
        internalName = printArg;
      } else if (printArg.front() == '\'') {
        type = "char_literal";
        internalName = printArg;
      } else {
        internalName =
            IsNumber(printArg) ? GetConstantName(printArg) : printArg;
        type = varTypes_.count(printArg) ? varTypes_[printArg] : "int";
      }

      printHooks_[printState] = {printArg, internalName, type};
      tm_.AddRule(currentState, '^', printState, '^', Direction::Stay);
      tm_.AddRule(printState, '^', nextState, '^', Direction::Stay);
    } else if (tokens.size() == 2 && tokens[1] == "++") {
      GenerateIncrement(tm_, currentState, GetAddressFor(tokens[0]), nextState);
    } else if (tokens.size() == 2 && tokens[1] == "--") {
      GenerateDecrement(tm_, currentState, GetAddressFor(tokens[0]), nextState);
    } else if (tokens.size() == 3 &&
               (tokens[1] == "+=" || tokens[1] == "-=" || tokens[1] == "*=" ||
                tokens[1] == "/=" || tokens[1] == "%=")) {
      std::string dest = tokens[0], op = tokens[1].substr(0, 1),
                  arg2 = tokens[2];
      CompileStatement({dest, "=", dest, op, arg2}, currentState, nextState);
    } else if (tokens.size() == 3 && tokens[1] == "=") {
      if (tokens[0] != tokens[2])
        GenerateAssign(tm_, currentState, GetAddressFor(tokens[0]),
                       GetAddressFor(tokens[2]), nextState);
      else
        tm_.AddRule(currentState, '^', nextState, '^', Direction::Stay);
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
      } else if (op == "/") {
        std::string destType = varTypes_.count(dest) ? varTypes_[dest] : "int";
        std::string arg1Type =
            varTypes_.count(arg1)
                ? varTypes_[arg1]
                : (IsNumber(arg1)
                       ? (arg1.find('.') != std::string::npos ? "float" : "int")
                       : "int");
        std::string arg2Type =
            varTypes_.count(arg2)
                ? varTypes_[arg2]
                : (IsNumber(arg2)
                       ? (arg2.find('.') != std::string::npos ? "float" : "int")
                       : "int");

        std::string t_q = currentState + "_q";
        char zeroAddr = GetAddressFor("0");

        // Умное деление: int / int -> float. Умножаем делимое на 100!
        if (destType == "float" && arg1Type != "float" && arg2Type != "float") {
          std::string t_mul = currentState + "_fmul";
          GenerateClear(tm_, currentState, mem_.GetAddress("_temp"),
                        t_mul + "_1");
          GenerateMultiply(tm_, t_mul + "_1", mem_.GetAddress("_temp"),
                           GetAddressFor(arg1), GetAddressFor("100"),
                           t_mul + "_2");
          GenerateDivMod(tm_, t_mul + "_2", mem_.GetAddress("_temp"),
                         GetAddressFor(arg2), mem_.GetAddress("_rem"),
                         mem_.GetAddress("_rem2"), zeroAddr, t_q);
          GenerateAssign(tm_, t_q, GetAddressFor(dest), mem_.GetAddress("_rem"),
                         nextState);
        } else {
          GenerateDivMod(tm_, currentState, GetAddressFor(arg1),
                         GetAddressFor(arg2), mem_.GetAddress("_temp"),
                         mem_.GetAddress("_rem"), zeroAddr, t_q);
          GenerateAssign(tm_, t_q, GetAddressFor(dest),
                         mem_.GetAddress("_temp"), nextState);
        }
      } else if (op == "%") {
        std::string t_r = currentState + "_r";
        char zeroAddr = GetAddressFor("0");
        GenerateDivMod(tm_, currentState, GetAddressFor(arg1),
                       GetAddressFor(arg2), mem_.GetAddress("_temp"),
                       mem_.GetAddress("_rem"), zeroAddr, t_r);
        GenerateAssign(tm_, t_r, GetAddressFor(dest), mem_.GetAddress("_rem"),
                       nextState);
      }
    } else {
      tm_.AddRule(currentState, '^', nextState, '^', Direction::Stay);
    }
  }

public:
  Compiler(TuringMachine &tm, MemoryManager &mem) : tm_(tm), mem_(mem) {}

  void Compile(const std::vector<std::string> &sourceCode) {
    try {
      mem_.Allocate("_temp", 0, 10000);
    } catch (...) {
    }
    try {
      mem_.Allocate("_rem", 0, 10000);
    } catch (...) {
    }
    try {
      mem_.Allocate("_rem2", 0, 10000);
    } catch (...) {
    }
    try {
      mem_.Allocate("_c0", 0, 0);
    } catch (...) {
    }
    try {
      mem_.Allocate("_c100", 100, 0);
    } catch (...) {
    } // Константа 100 для float division

    for (const auto &line : sourceCode) {
      auto tokens = Tokenize(line);
      for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "float") {
          varTypes_[tokens[i + 1]] = "float";
          if (i + 1 < tokens.size())
            try {
              mem_.Allocate(tokens[i + 1], 0, 10000);
            } catch (...) {
            }
        } else if (tokens[i] == "int_complex") {
          varTypes_[tokens[i + 1]] = "int_complex";
          if (i + 1 < tokens.size()) {
            try {
              mem_.Allocate(tokens[i + 1] + "_r", 0, 10000);
            } catch (...) {
            }
            try {
              mem_.Allocate(tokens[i + 1] + "_i", 0, 10000);
            } catch (...) {
            }
          }
        } else if (tokens[i] == "char") {
          varTypes_[tokens[i + 1]] = "char";
          if (i + 1 < tokens.size())
            try {
              mem_.Allocate(tokens[i + 1], 0, 255);
            } catch (...) {
            }
        } else if (tokens[i] == "int" || tokens[i] == "byte" ||
                   tokens[i] == "bool") {
          varTypes_[tokens[i + 1]] = tokens[i];
          int pad =
              (tokens[i] == "bool") ? 1 : (tokens[i] == "byte" ? 255 : 10000);
          if (i + 1 < tokens.size())
            try {
              mem_.Allocate(tokens[i + 1], 0, pad);
            } catch (...) {
            }
        } else if (IsCharLiteral(tokens[i])) {
          int ascii = (int)tokens[i][1];
          try {
            mem_.Allocate("_c" + std::to_string(ascii), ascii, 0);
          } catch (...) {
          }
        } else if (IsNumber(tokens[i])) {
          try {
            mem_.Allocate(GetConstantName(tokens[i]), ParseNumStr(tokens[i]),
                          0);
          } catch (...) {
          }
        }
      }
    }

    std::string currentState = "start";
    for (const auto &line : sourceCode) {
      auto tokens = Tokenize(line);
      if (tokens.empty())
        continue;

      auto it = std::remove_if(
          tokens.begin(), tokens.end(), [](const std::string &t) {
            return t == "bool" || t == "byte" || t == "int" || t == "float" ||
                   t == "int_complex" || t == "char";
          });
      tokens.erase(it, tokens.end());

      if (tokens.empty())
        continue;

      if (tokens[0] == "while") {
        std::string cond = NextState() + "_c", body = NextState() + "_b",
                    end = NextState() + "_e";
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

        std::string cond = NextState() + "_for_cond",
                    body = NextState() + "_for_body",
                    end = NextState() + "_for_end";
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
        std::string cond = NextState() + "_if_c", body = NextState() + "_if_b";
        std::string nextBlock = NextState() + "_if_next",
                    endChain = NextState() + "_if_end";
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
          std::string body = NextState() + "_elif_b",
                      nextBlock = NextState() + "_elif_next";
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

        if (hook.type == "string_literal") {
          // Вывод сырого текста без кавычек
          std::cout << hook.internalName.substr(1,
                                                hook.internalName.length() - 2)
                    << "\n";
        } else if (hook.type == "char" || hook.type == "char_literal") {
          int val = mem_.GetDecimalValue(tm_, hook.internalName);
          std::cout << ">> " << hook.displayName << " = '" << (char)val
                    << "'\n";
        } else if (hook.type == "int_complex") {
          int r = mem_.GetDecimalValue(tm_, hook.displayName + "_r");
          int i = mem_.GetDecimalValue(tm_, hook.displayName + "_i");
          std::cout << ">> " << hook.displayName << " = " << r << " + " << i
                    << "i\n";
        } else {
          int val = mem_.GetDecimalValue(tm_, hook.internalName);
          if (hook.type == "float") {
            std::cout << ">> " << hook.displayName << " = " << (val / 100)
                      << "." << (val % 100 < 10 ? "0" : "") << (val % 100)
                      << "\n";
          } else {
            std::cout << ">> " << hook.displayName << " = " << val << "\n";
          }
        }
      }

      if (!tm_.Step()) {
        std::cerr << "[Hardware Fault] Неожиданная остановка процессора.\n";
        break;
      }
    }
  }
};

#endif // COMPILER_H