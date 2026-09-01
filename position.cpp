// position.cpp — 棋局实现（移植自 xiangqi1）
#include "position.h"
#include "movegen.h"
#include <cstring>
#include <cstdlib> // abs

// 初始局面：0-15 红（0帅 1-2士 3-4相 5-6马 7-8车 9-10炮 11-15兵）
// 16-31 黑（16将 17-18士 19-20象 21-22马 23-24车 25-26炮 27-31卒）
// 坐标：x∈[1,9] 左→右，y∈[1,10] 下→上（红方视角 y=10 为底）
namespace {
// 初始子坐标 [man] = (x,y)
const int initX[32] = {
  5,4,6,3,7,2,8,1,9,2,8,1,3,5,7,9,   // 红
  5,4,6,3,7,2,8,1,9,2,8,1,3,5,7,9    // 黑
};
const int initY[32] = {
  10,10,10,10,10,10,10,10,10,8,8,7,7,7,7,7, // 红
  1,1,1,1,1,1,1,1,1,3,3,4,4,4,4,4            // 黑
};
}

Position::Position() { reset(); }

void Position::reset() {
  for (int i = 0; i < 32; i++) { manX_[i] = initX[i]; manY_[i] = initY[i]; }
  side_ = RED;
  fixMap();
}

int Position::sideToMove() const { return side_; }
void Position::setSideToMove(int s) { side_ = s; }

int Position::pieceAt(int x, int y) const { return map_[x][y]; }
int Position::manX(int man) const { return manX_[man]; }
int Position::manY(int man) const { return manY_[man]; }

void Position::fixMap() {
  // 全置 EMPTY(32)，边界 x=0/10、y=0/11 保持 EMPTY（与原版 _defaultmap 语义一致）
  for (int x = 0; x < 11; x++)
    for (int y = 0; y < 12; y++) map_[x][y] = EMPTY;
  for (int i = 0; i < 32; i++) {
    if (manX_[i]) map_[manX_[i]][manY_[i]] = i;
  }
}

bool Position::makeMove(const Move& m) {
  if (m.man < 0 || m.man >= 32) return false;
  if (manX_[m.man] != m.fromX || manY_[m.man] != m.fromY) return false;
  if (m.fromX < 1 || m.fromX > 9 || m.fromY < 1 || m.fromY > 10) return false;
  if (m.toX < 1 || m.toX > 9 || m.toY < 1 || m.toY > 10) return false;
  // 目标有己方子 → 非法
  int target = map_[m.toX][m.toY];
  if (target != EMPTY && sideOfMan(target) == side_) return false;
  // CanGo 语义校验
  if (!canGo(map_, m.man, m.fromX, m.fromY, m.toX, m.toY)) return false;

  // 执行走子（记录 capture 供 unmakeMove）
  manX_[m.man] = m.toX; manY_[m.man] = m.toY;
  if (target != EMPTY) { manX_[target] = 0; manY_[target] = 0; }
  map_[m.toX][m.toY] = m.man;
  map_[m.fromX][m.fromY] = EMPTY;
  return true;
}

void Position::unmakeMove() {
  // 简化：position 层不维护走子历史；UCCI 层通过 position 命令重建
  // 搜索层使用 unmakeMoveEx 按保存的着法还原
}

void Position::unmakeMoveEx(const Move& m, int fromX, int fromY, int cap) {
  // 将 man 从 (toX,toY) 移回 (fromX,fromY)，恢复被吃子 cap
  manX_[m.man] = fromX; manY_[m.man] = fromY;
  map_[m.fromX][m.fromY] = m.man;
  map_[m.toX][m.toY] = EMPTY;
  if (cap != EMPTY) { manX_[cap] = m.toX; manY_[cap] = m.toY; map_[m.toX][m.toY] = cap; }
}

bool Position::isLegalMove(const Move& m) const {
  if (m.man < 0 || m.man >= 32) return false;
  if (manX_[m.man] != m.fromX || manY_[m.man] != m.fromY) return false;
  if (m.fromX < 1 || m.fromX > 9 || m.fromY < 1 || m.fromY > 10) return false;
  if (m.toX < 1 || m.toX > 9 || m.toY < 1 || m.toY > 10) return false;
  int target = map_[m.toX][m.toY];
  if (target != EMPTY && sideOfMan(target) == side_) return false;
  if (!canGo(map_, m.man, m.fromX, m.fromY, m.toX, m.toY)) return false;

  // 走子后己方不被将军
  Position tmp = *this;
  if (!tmp.makeMove(m)) return false;
  tmp.setSideToMove(side_);
  return !tmp.inCheck(side_);
}

bool Position::inCheck(int side) const {
  // 找该方将/帅
  int k = kingOfSide(side);
  if (manX_[k] == 0) return false; // 已被吃（搜索层处理胜负）
  return canAttack(*this, side == RED ? BLACK : RED, manX_[k], manY_[k]);
}

int Position::legalMoveCount() const {
  std::vector<Move> moves;
  genLegalMoves(*this, moves);
  return (int)moves.size();
}

// ---- UCCI 坐标转换（a0-i9）----
// file a-i → x=1..9；rank 0-9 → y=10-rank（rank=10-y）
// 注：static 函数无局面上下文，仅解析坐标（man/capture 由调用方补全）
bool Position::parseMove(const char* s, Move& m) {
  if (!s || std::strlen(s) < 4) return false;
  char f1 = s[0], r1 = s[1], f2 = s[2], r2 = s[3];
  if (f1 < 'a' || f1 > 'i' || f2 < 'a' || f2 > 'i') return false;
  if (r1 < '0' || r1 > '9' || r2 < '0' || r2 > '9') return false;
  int fromX = f1 - 'a' + 1, toX = f2 - 'a' + 1;
  int fromY = 10 - (r1 - '0'), toY = 10 - (r2 - '0');
  if (fromX < 1 || fromX > 9 || toX < 1 || toX > 9) return false;
  if (fromY < 1 || fromY > 10 || toY < 1 || toY > 10) return false;
  m.fromX = fromX; m.fromY = fromY; m.toX = toX; m.toY = toY;
  m.man = EMPTY;   // 由调用方（UCCI 层）用局面补全
  m.capture = EMPTY;
  return true;
}

void Position::toUcci(const Move& m, char out[5]) {
  // x→file(a-i), y→rank(10-y 的字符)
  out[0] = (char)('a' + m.fromX - 1);
  out[1] = (char)('0' + (10 - m.fromY));
  out[2] = (char)('a' + m.toX - 1);
  out[3] = (char)('0' + (10 - m.toY));
  out[4] = 0;
}

bool Position::fromFen(const char* fen) {
  // 解析 8 段式中国象棋 FEN：rnbakabnr/9/.../RNBAKABNR w - - 0 1
  if (!fen) return false;
  // 先清空所有子
  for (int i = 0; i < 32; i++) { manX_[i] = 0; manY_[i] = 0; }
  const char* p = fen;
  // 棋盘段：10 行，用 '/' 分隔。FEN 第一行 = 黑方底线（y=1），最后一行 = 红方底线（y=10）
  int nextRed[7] = {0,1,3,5,7,9,11};   // 每种红子的下一个子号
  int nextBlk[7] = {16,17,19,21,23,25,27};
  int y = 1; // 第一行是 y=1（黑方底线）
  int x = 1;
  int placed = 0;
  while (*p && *p != ' ') {
    char c = *p;
    if (c == '/') { y++; x = 1; p++; continue; }
    if (c >= '1' && c <= '9') { x += (c - '0'); p++; continue; }
    int type7 = -1; int side = -1; int man = -1;
    switch (c) {
      case 'K': type7=0; side=RED; man=0; break;
      case 'A': type7=1; side=RED; man=1; break;
      case 'B': case 'E': type7=2; side=RED; man=3; break;
      case 'N': case 'H': type7=3; side=RED; man=5; break;
      case 'R': type7=4; side=RED; man=7; break;
      case 'C': type7=5; side=RED; man=9; break;
      case 'P': type7=6; side=RED; man=11; break;
      case 'k': type7=0; side=BLACK; man=16; break;
      case 'a': type7=1; side=BLACK; man=17; break;
      case 'b': case 'e': type7=2; side=BLACK; man=19; break;
      case 'n': case 'h': type7=3; side=BLACK; man=21; break;
      case 'r': type7=4; side=BLACK; man=23; break;
      case 'c': type7=5; side=BLACK; man=25; break;
      case 'p': type7=6; side=BLACK; man=27; break;
      default: return false; // 非法字符
    }
    // 分配子号（同类型递增）
    if (side == RED) { man = nextRed[type7]; nextRed[type7]++; }
    else { man = nextBlk[type7]; nextBlk[type7]++; }
    if (man < 0 || man >= 32 || x < 1 || x > 9 || y < 1 || y > 10) return false;
    manX_[man] = x; manY_[man] = y;
    x++; placed++;
    p++;
  }
  // 走子方
  while (*p == ' ') p++;
  if (*p == 'w') side_ = RED;
  else if (*p == 'b') side_ = BLACK;
  else return false;
  fixMap();
  return placed > 0;
}

// ---- canGo 移植（原版 BaseDef.cpp CanGo，无送将检查）----
bool isNormal(int mantype, int x, int y) {
  if (x < 1 || x > 9 || y < 1 || y > 10) return false;
  switch (mantype) {
  case RED_K:
    if (x > 6 || x < 4 || y < 8) return false; break;
  case RED_S:
    if (!((x==4&&y==10)||(x==4&&y==8)||(x==5&&y==9)||(x==6&&y==10)||(x==6&&y==8))) return false; break;
  case RED_X:
    if (!((x==1&&y==8)||(x==3&&y==10)||(x==3&&y==6)||(x==5&&y==8)||(x==7&&y==10)||(x==7&&y==6)||(x==9&&y==8))) return false; break;
  case RED_B:
    if (y > 7) return false;
    if (y > 5 && x % 2 == 0) return false; break;
  case BLACK_K:
    if (x > 6 || x < 4 || y > 3) return false; break;
  case BLACK_S:
    if (!((x==4&&y==1)||(x==4&&y==3)||(x==5&&y==2)||(x==6&&y==1)||(x==6&&y==3))) return false; break;
  case BLACK_X:
    if (!((x==1&&y==3)||(x==3&&y==1)||(x==3&&y==5)||(x==5&&y==3)||(x==7&&y==1)||(x==7&&y==5)||(x==9&&y==3))) return false; break;
  case BLACK_B:
    if (y < 4) return false;
    if (y < 6 && x % 2 == 0) return false; break;
  default: break;
  }
  return true;
}

bool canGo(const int manmap[11][12], int man, int xfrom, int yfrom, int xto, int yto) {
  int mt = manToType(man);
  if (!isNormal(mt, xto, yto)) {
    // 王对脸（飞将）：帅/将可沿同列吃对方将
    if (mt == RED_K || mt == BLACK_K) {
      if (mt == RED_K && manToType(manmap[xto][yto]) == BLACK_K) {
        for (int j = yfrom-1; j > 0; j--) {
          if (manmap[xfrom][j] != EMPTY) return manToType(manmap[xfrom][j]) == BLACK_K;
        }
        return false;
      }
      if (manToType(manmap[xto][yto]) == RED_K) {
        for (int j = yfrom+1; j < 11; j++) {
          if (manmap[xfrom][j] != EMPTY) return manToType(manmap[xfrom][j]) == RED_K;
        }
        return false;
      }
    }
    return false;
  }
  // 目标不能是己方子
  if (manmap[xto][yto] != EMPTY && sideOfMan(manmap[xto][yto]) == sideOfMan(man)) return false;

  switch (mt) {
  case RED_B: // 红兵
    if (yto > yfrom) return false;
    if (yfrom - yto + abs(xto - xfrom) > 1) return false;
    break;
  case BLACK_B: // 黑卒
    if (yto < yfrom) return false;
    if (yto - yfrom + abs(xto - xfrom) > 1) return false;
    break;
  case RED_S:
  case BLACK_S:
    if (abs(yfrom-yto) > 1 || abs(xfrom-xto) > 1) return false;
    break;
  case RED_X:
  case BLACK_X:
    if (abs(xfrom-xto) != 2 || abs(yfrom-yto) != 2) return false;
    if (manmap[(xfrom+xto)/2][(yfrom+yto)/2] != EMPTY) return false; // 象眼
    break;
  case RED_K:
  case BLACK_K:
    if (abs(yfrom-yto) + abs(xfrom-xto) > 1) return false;
    break;
  case RED_J:
  case BLACK_J: { // 车：直线，无阻挡
    if (yfrom != yto && xfrom != xto) return false;
    if (yfrom == yto) {
      for (int i = (xfrom<xto?xfrom+1:xto+1); i < (xfrom<xto?xto:xfrom); i++)
        if (manmap[i][yfrom] != EMPTY) return false;
    } else {
      for (int j = (yfrom<yto?yfrom+1:yto+1); j < (yfrom<yto?yto:yfrom); j++)
        if (manmap[xfrom][j] != EMPTY) return false;
    }
    break;
  }
  case RED_P:
  case BLACK_P: { // 炮：直线；吃子需隔一子
    if (yfrom != yto && xfrom != xto) return false;
    if (manmap[xto][yto] == EMPTY) { // 不吃子：无阻挡
      if (yfrom == yto) {
        for (int i = (xfrom<xto?xfrom+1:xto+1); i < (xfrom<xto?xto:xfrom); i++)
          if (manmap[i][yfrom] != EMPTY) return false;
      } else {
        for (int j = (yfrom<yto?yfrom+1:yto+1); j < (yfrom<yto?yto:yfrom); j++)
          if (manmap[xfrom][j] != EMPTY) return false;
      }
    } else { // 吃子：需恰好一个炮架
      int count = 0;
      if (yfrom == yto) {
        for (int i = (xfrom<xto?xfrom+1:xto+1); i < (xfrom<xto?xto:xfrom); i++)
          if (manmap[i][yfrom] != EMPTY) count++;
      } else {
        for (int j = (yfrom<yto?yfrom+1:yto+1); j < (yfrom<yto?yto:yfrom); j++)
          if (manmap[xfrom][j] != EMPTY) count++;
      }
      if (count != 1) return false;
    }
    break;
  }
  case RED_M:
  case BLACK_M: { // 马：走日 + 蹩马腿
    if (!((abs(xto-xfrom)==1&&abs(yto-yfrom)==2)||(abs(xto-xfrom)==2&&abs(yto-yfrom)==1))) return false;
    int i, j;
    if      (xto-xfrom == 2) { i = xfrom+1; j = yfrom; }
    else if (xfrom-xto == 2) { i = xfrom-1; j = yfrom; }
    else if (yto-yfrom == 2) { i = xfrom; j = yfrom+1; }
    else                     { i = xfrom; j = yfrom-1; }
    if (manmap[i][j] != EMPTY) return false;
    break;
  }
  default: return false;
  }
  return true;
}
