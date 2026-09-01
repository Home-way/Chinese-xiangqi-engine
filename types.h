#pragma once
// types.h — 常量与基础类型（引擎核心）
// 移植自 xiangqi1：子号 0-15 红、16-31 黑；坐标 x∈[1,9], y∈[1,10]

#include <cstdint>

// 走子方
enum Side : int { RED = 0, BLACK = 1 };

// 棋盘空点
enum { EMPTY = 32 };

// 棋子类型（ManToType 语义：0帅 1士 2相 3马 4车 5炮 6兵；黑方 +7）
enum {
  RED_K = 0, RED_S = 1, RED_X = 2, RED_M = 3, RED_J = 4, RED_P = 5, RED_B = 6,
  BLACK_K = 7, BLACK_S = 8, BLACK_X = 9, BLACK_M = 10, BLACK_J = 11, BLACK_P = 12, BLACK_B = 13
};

// 子号 → 类型（与 ManToIcon 一致）
inline int manToType(int man) {
  static const int t[33] = {
    0,1,1,2,2,3,3,4,4,5,5,6,6,6,6,6,
    7,8,8,9,9,10,10,11,11,12,12,13,13,13,13,13,-1
  };
  return t[man];
}

// 子号 → 所属方（RED/BLACK）
inline int sideOfMan(int man) {
  static const int s[33] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,-1
  };
  return s[man];
}

// 一方的将/帅子号
inline int kingOfSide(int side) { return side == RED ? 0 : 16; }

// 一方的子号范围
inline int fistOfSide(int side) { return side == RED ? 0 : 16; }
inline int lastOfSide(int side) { return side == RED ? 15 : 31; }

// 着法
struct Move {
  int man = EMPTY;      // 所走子号
  int fromX = 0, fromY = 0;  // 起点
  int toX = 0, toY = 0;      // 终点
  int capture = EMPTY;       // 被吃子号（无吃则为 EMPTY）
  bool operator==(const Move& o) const {
    return man == o.man && fromX == o.fromX && fromY == o.fromY &&
           toX == o.toX && toY == o.toY && capture == o.capture;
  }
};
