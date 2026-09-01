#pragma once
// movegen.h — 着法生成（移植自 xiangqi1 CMoveList::EnumMove）

#include "types.h"
#include "position.h"
#include <vector>

// 生成 sideToMove 一方的“伪合法”着法（含吃将等，不校验送将）
void genPseudoMoves(const Position& pos, std::vector<Move>& out);

// 生成合法着法（伪合法 + 过滤送将）
void genLegalMoves(const Position& pos, std::vector<Move>& out);

// 某方棋子能否攻击到 (tx,ty)（供 inCheck 使用）
bool canAttack(const Position& pos, int side, int tx, int ty);
