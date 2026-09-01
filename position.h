#pragma once
// position.h — 棋局：32 子坐标 + side；走子/还原/合法性/将军检测
// 移植自 xiangqi1 CFace + FixManMap + CanGo 语义

#include "types.h"

class Position {
public:
  Position();                       // 初始局面（红先）
  void reset();                     // 设为初始局面

  int  sideToMove() const;          // 当前走子方 RED/BLACK
  void setSideToMove(int s);

  int  pieceAt(int x, int y) const; // 该点棋子编号，空为 EMPTY
  int  manX(int man) const;         // 某子 x（0 表示已被吃）
  int  manY(int man) const;

  bool makeMove(const Move& m);     // 执行走子；返回是否合法（非法=未修改局面）
  void unmakeMove();                // 还原一步（须与 makeMove 配对，本实现由 UCCI 层重建）
  void unmakeMoveEx(const Move& m, int fromX, int fromY, int cap); // 搜索层用：按保存的着法还原
  bool isLegalMove(const Move& m) const; // 伪合法且走后不送将
  bool inCheck(int side) const;     // 一方是否被将军
  int  legalMoveCount() const;      // 当前走子方合法着法总数

  // 供 UCCI 层：4 字符串转 Move / 反向（a0-i9）
  static bool parseMove(const char* s, Move& m);
  static void toUcci(const Move& m, char out[5]);
  // 从中国象棋 FEN（8 段式）设置局面；返回是否成功
  bool fromFen(const char* fen);

  // 内部：从坐标拿子（供 movegen 用）
  int mapAt(int x, int y) const { return map_[x][y]; }
  // 是否空点
  bool isEmpty(int x, int y) const { return map_[x][y] == EMPTY; }

private:
  int manX_[32], manY_[32]; // 每子坐标（0=已被吃）
  int map_[11][12];         // 11x12 网格（FixManMap 同步维护，边界 0/32）
  int side_;                // 当前走子方

  void fixMap();            // 由 manX_/manY_ 重建 map_
};

// 走子合法性判断（CanGo 移植，伪合法：不含送将检查）
bool canGo(const int manmap[11][12], int man, int xfrom, int yfrom, int xto, int yto);

// 位置是否在规则区域内（IsNormal 移植）
bool isNormal(int mantype, int x, int y);
