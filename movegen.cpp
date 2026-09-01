// movegen.cpp — 着法生成实现（移植自 xiangqi1 EnumMove + canGo）
#include "movegen.h"
#include <cstdlib>

namespace {
// 从 manmap 判断某方棋子能否攻击到 (tx,ty)
bool canPieceAttack(const int manmap[11][12], int man, int tx, int ty) {
  if (man < 0 || man >= 32) return false;
  int x = 0, y = 0;
  // 需要棋子坐标：这里由调用方传入（canAttack 遍历己方棋子）
  (void)manmap; (void)x; (void)y;
  return false;
}
}

// 生成伪合法着法：遍历己方每子，用 canGo 枚举可达位置（移植 EnumMove）
void genPseudoMoves(const Position& pos, std::vector<Move>& out) {
  int side = pos.sideToMove();
  int nb = fistOfSide(side), nk = lastOfSide(side);
  int map[11][12];
  for (int x = 0; x < 11; x++)
    for (int y = 0; y < 12; y++) map[x][y] = pos.mapAt(x, y);

  auto tryAdd = [&](int man, int x, int y) {
    if (canGo(map, man, pos.manX(man), pos.manY(man), x, y)) {
      Move m;
      m.man = man;
      m.fromX = pos.manX(man); m.fromY = pos.manY(man);
      m.toX = x; m.toY = y;
      m.capture = map[x][y];
      out.push_back(m);
    }
  };

  for (int man = nb; man <= nk; man++) {
    int mx = pos.manX(man), my = pos.manY(man);
    if (mx == 0) continue; // 已被吃
    int mt = manToType(man);
    switch (mt) {
    case RED_K:
    case BLACK_K: {
      // 王对脸飞将
      int oppKing = kingOfSide(side == RED ? BLACK : RED);
      if (pos.manX(oppKing) != 0) {
        // 同列中间无子 → 可飞将吃
        bool clear = true;
        for (int j = my-1; j > pos.manY(oppKing); j--) {
          if (map[pos.manX(oppKing)][j] != EMPTY) { clear = false; break; }
        }
        if (clear) tryAdd(man, mx, pos.manY(oppKing));
      }
      tryAdd(man, mx+1, my); tryAdd(man, mx-1, my);
      tryAdd(man, mx, my+1); tryAdd(man, mx, my-1);
      break;
    }
    case RED_S:
    case BLACK_S:
      tryAdd(man, mx-1, my-1); tryAdd(man, mx+1, my+1);
      tryAdd(man, mx+1, my-1); tryAdd(man, mx-1, my+1);
      break;
    case RED_X:
    case BLACK_X:
      tryAdd(man, mx-2, my-2); tryAdd(man, mx+2, my+2);
      tryAdd(man, mx+2, my-2); tryAdd(man, mx-2, my+2);
      break;
    case RED_M:
    case BLACK_M:
      tryAdd(man, mx-2, my-1); tryAdd(man, mx+2, my+1);
      tryAdd(man, mx+2, my-1); tryAdd(man, mx-2, my+1);
      tryAdd(man, mx-1, my-2); tryAdd(man, mx+1, my+2);
      tryAdd(man, mx+1, my-2); tryAdd(man, mx-1, my+2);
      break;
    case RED_J:
    case BLACK_J: // 车：4 方向直线
      for (int x = 1; x <= 9; x++) tryAdd(man, x, my);
      for (int y = 1; y <= 10; y++) tryAdd(man, mx, y);
      break;
    case RED_P:
    case BLACK_P: // 炮：直线（canGo 处理炮架）
      for (int x = 1; x <= 9; x++) tryAdd(man, x, my);
      for (int y = 1; y <= 10; y++) tryAdd(man, mx, y);
      break;
    case RED_B: // 红兵
      if (my < 6) { tryAdd(man, mx+1, my); tryAdd(man, mx-1, my); }
      tryAdd(man, mx, my-1);
      break;
    case BLACK_B: // 黑卒
      if (my > 5) { tryAdd(man, mx+1, my); tryAdd(man, mx-1, my); }
      tryAdd(man, mx, my+1);
      break;
    default: break;
    }
  }
}

// 合法着法：伪合法 + 走子后己方不被将军
void genLegalMoves(const Position& pos, std::vector<Move>& out) {
  std::vector<Move> pseudo;
  genPseudoMoves(pos, pseudo);
  for (const Move& m : pseudo) {
    if (pos.isLegalMove(m)) out.push_back(m);
  }
}

// 某方棋子能否攻击到 (tx,ty)（供 inCheck）
bool canAttack(const Position& pos, int side, int tx, int ty) {
  // 用该方棋子逐个尝试走到 (tx,ty) 是否合法（canGo 语义，无送将检查）
  int map[11][12];
  for (int x = 0; x < 11; x++)
    for (int y = 0; y < 12; y++) map[x][y] = pos.mapAt(x, y);
  int nb = fistOfSide(side), nk = lastOfSide(side);
  for (int man = nb; man <= nk; man++) {
    if (pos.manX(man) == 0) continue;
    if (canGo(map, man, pos.manX(man), pos.manY(man), tx, ty)) return true;
  }
  return false;
}
