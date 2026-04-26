#ifndef MACROS_FOR_TURING
#define MACROS_FOR_TURING

#include "TuringMachine.h"
#include <string>

// Единый алфавит. * - флаг для сложения, @ - флаг для
// умножения/сравнения/вычитания ^ - МАРКЕР АБСОЛЮТНОГО НАЧАЛА ЛЕНТЫ (The Wall)
const std::string ALPHABET = "01_:#abcdefghijklmnopqrstuvwxyz*@^";

void GenerateMoveRightUntil(TuringMachine &tm, const std::string &startState,
                            char target, const std::string &endState) {
  for (char s : ALPHABET) {
    if (s != target) {
      tm.AddRule(startState, s, startState, s, Direction::Right);
    }
  }
  tm.AddRule(startState, target, endState, target, Direction::Stay);
}

void GenerateMoveLeftUntil(TuringMachine &tm, const std::string &startState,
                           char target, const std::string &endState) {
  for (char s : ALPHABET) {
    if (s != target) {
      tm.AddRule(startState, s, startState, s, Direction::Left);
    }
  }
  tm.AddRule(startState, target, endState, target, Direction::Stay);
}

void GenerateReturnToStart(TuringMachine &tm, const std::string &state,
                           const std::string &nextState) {
  for (char s : ALPHABET) {
    if (s != '^') {
      tm.AddRule(state, s, state, s, Direction::Left);
    }
  }
  tm.AddRule(state, '^', nextState, '^', Direction::Stay);
}

void GenerateIncrement(TuringMachine &tm, const std::string &startState,
                       char varName, const std::string &nextState) {
  std::string prefix = startState + "_inc_";
  std::string s1 = prefix + "seek", s2 = prefix + "write",
              s3 = prefix + "return";

  GenerateMoveRightUntil(tm, startState, varName, s1);
  tm.AddRule(s1, varName, s1, varName, Direction::Right);
  tm.AddRule(s1, ':', s1, ':', Direction::Right);
  tm.AddRule(s1, '1', s1, '1', Direction::Right);
  tm.AddRule(s1, '#', s2, '1', Direction::Right);

  tm.AddRule(s2, '_', s3, '#', Direction::Stay);
  tm.AddRule(s2, '#', s3, '#', Direction::Stay);

  GenerateReturnToStart(tm, s3, nextState);
}

void GenerateCompare(TuringMachine &tm, const std::string &startState,
                     char xName, char yName, const std::string &stateGreater,
                     const std::string &stateLess,
                     const std::string &stateEqual) {

  std::string prefix = startState + "_cmp_";
  std::string sStartLoop = prefix + "start_loop", sFindX = prefix + "find_x";
  std::string sGoY = prefix + "go_y", sCheckY = prefix + "check_y";
  std::string sCheckYEmpty = prefix + "check_y_empty";

  GenerateReturnToStart(tm, startState, sStartLoop);

  GenerateMoveRightUntil(tm, sStartLoop, xName, sFindX);
  tm.AddRule(sFindX, xName, sFindX, xName, Direction::Right);
  tm.AddRule(sFindX, ':', sFindX, ':', Direction::Right);
  tm.AddRule(sFindX, '@', sFindX, '@', Direction::Right);

  tm.AddRule(sFindX, '1', sGoY, '@', Direction::Stay);
  tm.AddRule(sFindX, '#', sCheckYEmpty, '#', Direction::Stay);

  std::string sFindYStart = prefix + "find_y_start";
  GenerateReturnToStart(tm, sGoY, sFindYStart);

  GenerateMoveRightUntil(tm, sFindYStart, yName, sCheckY);
  tm.AddRule(sCheckY, yName, sCheckY, yName, Direction::Right);
  tm.AddRule(sCheckY, ':', sCheckY, ':', Direction::Right);
  tm.AddRule(sCheckY, '@', sCheckY, '@', Direction::Right);

  std::string sLoopBack = prefix + "loop_back";
  tm.AddRule(sCheckY, '1', sLoopBack, '@', Direction::Stay);
  GenerateReturnToStart(tm, sLoopBack, sStartLoop);

  tm.AddRule(sCheckY, '#', prefix + "cleanup_G", '#', Direction::Stay);

  std::string sYFindStart = sCheckYEmpty + "_find_start";
  GenerateReturnToStart(tm, sCheckYEmpty, sYFindStart);

  std::string sYFind = sCheckYEmpty + "_find";
  GenerateMoveRightUntil(tm, sYFindStart, yName, sYFind);
  tm.AddRule(sYFind, yName, sYFind, yName, Direction::Right);
  tm.AddRule(sYFind, ':', sYFind, ':', Direction::Right);
  tm.AddRule(sYFind, '@', sYFind, '@', Direction::Right);

  tm.AddRule(sYFind, '1', prefix + "cleanup_L", '1', Direction::Stay);
  tm.AddRule(sYFind, '#', prefix + "cleanup_E", '#', Direction::Stay);

  for (std::string res : {"_G", "_L", "_E"}) {
    std::string st = prefix + "cleanup" + res;
    std::string finalState =
        (res == "_G" ? stateGreater : (res == "_L" ? stateLess : stateEqual));

    std::string cx = st + "_cx";
    GenerateReturnToStart(tm, st, cx);
    std::string cxFind = cx + "_find";
    GenerateMoveRightUntil(tm, cx, xName, cxFind);
    tm.AddRule(cxFind, xName, cxFind, xName, Direction::Right);
    tm.AddRule(cxFind, ':', cxFind, ':', Direction::Right);
    tm.AddRule(cxFind, '1', cxFind, '1', Direction::Right);
    tm.AddRule(cxFind, '@', cxFind, '1', Direction::Right);
    tm.AddRule(cxFind, '#', st + "_cy", '#', Direction::Stay);

    std::string cy = st + "_cy_start";
    GenerateReturnToStart(tm, st + "_cy", cy);
    std::string cyFind = cy + "_find";
    GenerateMoveRightUntil(tm, cy, yName, cyFind);
    tm.AddRule(cyFind, yName, cyFind, yName, Direction::Right);
    tm.AddRule(cyFind, ':', cyFind, ':', Direction::Right);
    tm.AddRule(cyFind, '1', cyFind, '1', Direction::Right);
    tm.AddRule(cyFind, '@', cyFind, '1', Direction::Right);
    tm.AddRule(cyFind, '#', st + "_finish", '#', Direction::Stay);

    GenerateReturnToStart(tm, st + "_finish", finalState);
  }
}

void GenerateClear(TuringMachine &tm, const std::string &startState, char xName,
                   const std::string &nextState) {
  std::string prefix = startState + "_clr_";
  std::string sFind = prefix + "find", sErase = prefix + "erase";
  std::string sBack = prefix + "back", sWrite = prefix + "write";

  GenerateMoveRightUntil(tm, startState, xName, sFind);

  tm.AddRule(sFind, xName, sFind, xName, Direction::Right);
  tm.AddRule(sFind, ':', sErase, ':', Direction::Right);

  tm.AddRule(sErase, '1', sErase, '_', Direction::Right);
  tm.AddRule(sErase, '#', sBack, '_', Direction::Left);

  tm.AddRule(sBack, '_', sBack, '_', Direction::Left);
  tm.AddRule(sBack, ':', sWrite, ':', Direction::Right);

  tm.AddRule(sWrite, '_', prefix + "return", '#', Direction::Stay);
  GenerateReturnToStart(tm, prefix + "return", nextState);
}

void GenerateAdd(TuringMachine &tm, const std::string &startState, char xName,
                 char yName, const std::string &nextState) {
  std::string prefix = startState + "_add_";
  std::string sStartLoop = prefix + "start_loop", sCheckY = prefix + "check_y";
  std::string sGoStartX = prefix + "go_start_x", sFindX = prefix + "find_x";
  std::string sWriteX = prefix + "write_x", sGoNext = prefix + "go_next";
  std::string sRestore = prefix + "restore";

  GenerateReturnToStart(tm, startState, sStartLoop);

  GenerateMoveRightUntil(tm, sStartLoop, yName, sCheckY);
  tm.AddRule(sCheckY, yName, sCheckY, yName, Direction::Right);
  tm.AddRule(sCheckY, ':', sCheckY, ':', Direction::Right);
  tm.AddRule(sCheckY, '*', sCheckY, '*', Direction::Right);

  tm.AddRule(sCheckY, '1', sGoStartX, '*', Direction::Stay);
  tm.AddRule(sCheckY, '#', sRestore, '#', Direction::Stay);

  GenerateReturnToStart(tm, sGoStartX, sFindX);

  GenerateMoveRightUntil(tm, sFindX, xName, sWriteX);
  tm.AddRule(sWriteX, xName, sWriteX, xName, Direction::Right);
  tm.AddRule(sWriteX, ':', sWriteX, ':', Direction::Right);
  tm.AddRule(sWriteX, '1', sWriteX, '1', Direction::Right);

  tm.AddRule(sWriteX, '#', sGoNext, '1', Direction::Right);

  tm.AddRule(sGoNext, '_', prefix + "loop_back", '#', Direction::Stay);
  tm.AddRule(sGoNext, '#', prefix + "loop_back", '#', Direction::Stay);

  GenerateReturnToStart(tm, prefix + "loop_back", sStartLoop);

  GenerateReturnToStart(tm, sRestore, prefix + "clean_y");
  GenerateMoveRightUntil(tm, prefix + "clean_y", yName, prefix + "cleaning");

  tm.AddRule(prefix + "cleaning", yName, prefix + "cleaning", yName,
             Direction::Right);
  tm.AddRule(prefix + "cleaning", ':', prefix + "cleaning", ':',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '1', prefix + "cleaning", '1',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '*', prefix + "cleaning", '1',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '#', prefix + "finish", '#', Direction::Stay);

  GenerateReturnToStart(tm, prefix + "finish", nextState);
}

void GenerateDecrement(TuringMachine &tm, const std::string &startState,
                       char xName, const std::string &nextState) {
  std::string prefix = startState + "_dec_";
  std::string sFind = prefix + "find", sScan = prefix + "scan";
  std::string sCheck = prefix + "check", sErase = prefix + "erase";

  GenerateMoveRightUntil(tm, startState, xName, sFind);
  tm.AddRule(sFind, xName, sFind, xName, Direction::Right);
  tm.AddRule(sFind, ':', sScan, ':', Direction::Right);

  tm.AddRule(sScan, '1', sScan, '1', Direction::Right);
  tm.AddRule(sScan, '#', sCheck, '#', Direction::Left);

  tm.AddRule(sCheck, '1', sErase, '#', Direction::Right);
  tm.AddRule(sErase, '#', prefix + "return", '_', Direction::Stay);

  tm.AddRule(sCheck, ':', prefix + "return", ':', Direction::Stay);
  GenerateReturnToStart(tm, prefix + "return", nextState);
}

void GenerateSubtract(TuringMachine &tm, const std::string &startState,
                      char xName, char yName, const std::string &nextState) {
  std::string prefix = startState + "_sub_";
  std::string sFindY = prefix + "find_y", sCheckY = prefix + "check_y";
  std::string sGoStartX = prefix + "go_start_x", sRestore = prefix + "restore";

  GenerateMoveRightUntil(tm, startState, yName, sCheckY);
  tm.AddRule(sCheckY, yName, sCheckY, yName, Direction::Right);
  tm.AddRule(sCheckY, ':', sCheckY, ':', Direction::Right);
  tm.AddRule(sCheckY, '@', sCheckY, '@', Direction::Right);

  tm.AddRule(sCheckY, '1', sGoStartX, '@', Direction::Stay);
  tm.AddRule(sCheckY, '#', sRestore, '#', Direction::Stay);

  std::string sDecX = prefix + "do_dec_x";
  GenerateReturnToStart(tm, sGoStartX, sDecX);

  std::string sLoopBack = prefix + "loop_back";
  GenerateDecrement(tm, sDecX, xName, sLoopBack);

  GenerateReturnToStart(tm, sLoopBack, startState);

  GenerateReturnToStart(tm, sRestore, prefix + "clean_y");
  GenerateMoveRightUntil(tm, prefix + "clean_y", yName, prefix + "cleaning");

  tm.AddRule(prefix + "cleaning", yName, prefix + "cleaning", yName,
             Direction::Right);
  tm.AddRule(prefix + "cleaning", ':', prefix + "cleaning", ':',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '1', prefix + "cleaning", '1',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '@', prefix + "cleaning", '1',
             Direction::Right);

  tm.AddRule(prefix + "cleaning", '#', prefix + "finish", '#', Direction::Stay);
  GenerateReturnToStart(tm, prefix + "finish", nextState);
}

void GenerateMultiply(TuringMachine &tm, const std::string &startState,
                      char xName, char yName, char zName,
                      const std::string &nextState) {
  std::string prefix = startState + "_mul_";
  std::string sLoopZ = prefix + "loop_z", sCheckZ = prefix + "check_z";
  std::string sDoAdd = prefix + "do_add", sRestoreZ = prefix + "restore_z";

  GenerateReturnToStart(tm, startState, sLoopZ);
  GenerateMoveRightUntil(tm, sLoopZ, zName, sCheckZ);

  tm.AddRule(sCheckZ, zName, sCheckZ, zName, Direction::Right);
  tm.AddRule(sCheckZ, ':', sCheckZ, ':', Direction::Right);
  tm.AddRule(sCheckZ, '@', sCheckZ, '@', Direction::Right);

  tm.AddRule(sCheckZ, '1', sDoAdd, '@', Direction::Stay);
  tm.AddRule(sCheckZ, '#', sRestoreZ, '#', Direction::Stay);

  GenerateAdd(tm, sDoAdd, xName, yName, sLoopZ);

  GenerateReturnToStart(tm, sRestoreZ, prefix + "clean_z");
  GenerateMoveRightUntil(tm, prefix + "clean_z", zName, prefix + "cleaning");

  tm.AddRule(prefix + "cleaning", zName, prefix + "cleaning", zName,
             Direction::Right);
  tm.AddRule(prefix + "cleaning", ':', prefix + "cleaning", ':',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '1', prefix + "cleaning", '1',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '@', prefix + "cleaning", '1',
             Direction::Right);
  tm.AddRule(prefix + "cleaning", '#', prefix + "finish", '#', Direction::Stay);

  GenerateReturnToStart(tm, prefix + "finish", nextState);
}

void GenerateAssign(TuringMachine &tm, const std::string &startState,
                    char destName, char srcName, const std::string &nextState) {
  std::string sClear = startState + "_assign_clear";
  GenerateClear(tm, startState, destName, sClear);
  GenerateAdd(tm, sClear, destName, srcName, nextState);
}

void GenerateDivMod(TuringMachine &tm, const std::string &startState,
                    char xName, char yName, char qName, char rName,
                    char zeroName, const std::string &nextState) {
  std::string prefix = startState + "_divmod_";
  std::string sClearQ = prefix + "clear_q";
  std::string sAssignR = prefix + "assign_r";
  std::string sLoop = prefix + "loop";
  std::string sDoSub = prefix + "do_sub";
  std::string sDoInc = prefix + "do_inc";

  GenerateCompare(tm, startState, yName, zeroName, sClearQ, nextState,
                  nextState);
  GenerateClear(tm, sClearQ, qName, sAssignR);
  GenerateAssign(tm, sAssignR, rName, xName, sLoop);
  GenerateCompare(tm, sLoop, rName, yName, sDoSub, nextState, sDoSub);
  GenerateSubtract(tm, sDoSub, rName, yName, sDoInc);
  GenerateIncrement(tm, sDoInc, qName, sLoop);
}

#endif // MACROS_FOR_TURING