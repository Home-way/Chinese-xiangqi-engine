// eval.cpp — 局面评估实现（忠实移植 xiangqi1 ContactV + SubThink 评估段）
// 纯函数：输入 Position，返回红方视角分数（与原版符号约定一致）

#include "eval.h"
#include "types.h"
#include <cstring>

// BA 位置价值表（Thinkdef.h 移植：k,s,x,m,j,p,b → 0..6）
namespace {
const int BA[2][12][11] = {
  { // 红（红方视角 y 大 = 进攻方向）
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,2,2,3,4,4,4,3,2,2,0},
    { 0,2,2,3,4,4,4,3,2,2,0},
    { 0,1,2,3,3,3,3,3,2,1,0},
    { 0,1,1,1,1,1,1,1,1,1,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0}
  },
  { // 黑
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,1,1,1,1,1,1,1,1,1,0},
    { 0,1,2,3,3,3,3,3,2,1,0},
    { 0,2,2,3,4,4,4,3,2,2,0},
    { 0,2,2,3,4,4,4,3,2,2,0},
    { 0,0,0,0,0,0,0,0,0,0,0},
    { 0,0,0,0,0,0,0,0,0,0,0}
  }
};

// 子力基础值 BV1 / 接触价值权重 BV2（顺序：帅0 士1 相2 马3 车4 炮5 兵6）
const int BV1[7] = {0, 250, 250, 300, 400, 300, 100};
const int BV2[7] = {0, 1, 1, 12, 6, 6, 15};
// 兵卒位置档 BV3
const int BV3[5] = {0, 70, 90, 110, 120};

// ManToType7（32 子号 → 0..6 类型）
inline int manToType7(int man) {
  static const int t7[32] = {
    0,1,1,2,2,3,3,4,4,5,5,6,6,6,6,6,
    0,1,1,2,2,3,3,4,4,5,5,6,6,6,6,6
  };
  return t7[man];
}

// 目标方（红视角：BLACK 的攻击方向用 BA[1]，RED 用 BA[0]）
} // namespace

int evaluate(const Position& pos) {
  // 建立局面数组
  int manx[32], many[32], map[11][12];
  for (int i = 0; i < 32; i++) { manx[i] = pos.manX(i); many[i] = pos.manY(i); }
  for (int x = 0; x < 11; x++)
    for (int y = 0; y < 12; y++) map[x][y] = pos.mapAt(x, y);

  // 接触矩阵 v2[32][32]：v2[i][j]=1 表示 i 子攻击 j 子
  int v1[32], v2[32][32], v3[32], v4[32];
  std::memset(v2, 0, sizeof(v2));
  std::memset(v1, 0, sizeof(v1));
  std::memset(v3, 0, sizeof(v3));
  std::memset(v4, 0, sizeof(v4));

  // ---- ContactV 移植：遍历每子，用 CV 宏记录攻击对 ----
  auto CV = [&](int man, int tx, int ty) {
    int k = map[tx][ty];
    v1[man] += 1;
    if (k != EMPTY) v2[man][k] = 1;
  };

  for (int n = 0; n <= 31; n++) {
    int x = manx[n];
    if (!x) continue; // 已被吃
    int y = many[n];
    int i, j;
    switch (n) {
    case 0: // 红帅
      if (manx[0] == manx[16]) { // 王对脸
        bool flag = false;
        for (j = many[16]+1; j < many[0]; j++)
          if (map[x][j] != EMPTY) { flag = true; break; }
        if (!flag) CV(0, x, many[16]);
      }
      j = y+1; if (j <= 10) CV(0, x, j);
      j = y-1; if (j >= 8)  CV(0, x, j);
      i = x+1; if (i <= 6)  CV(0, i, y);
      i = x-1; if (i >= 4)  CV(0, i, y);
      break;
    case 16: // 黑将
      if (manx[0] == manx[16]) {
        bool flag = false;
        for (j = many[16]+1; j < many[0]; j++)
          if (map[x][j] != EMPTY) { flag = true; break; }
        if (!flag) CV(16, x, many[0]);
      }
      j = y+1; if (j <= 3) CV(16, x, j);
      j = y-1; if (j >= 1) CV(16, x, j);
      i = x+1; if (i <= 6) CV(16, i, y);
      i = x-1; if (i >= 4) CV(16, i, y);
      break;
    case 1: case 2: // 红士
      i = x+1; j = y+1; if (i<=6 && j<=10) CV(n,i,j);
      i = x+1; j = y-1; if (i<=6 && j>=8)  CV(n,i,j);
      i = x-1; j = y+1; if (i>=4 && j<=10) CV(n,i,j);
      i = x-1; j = y-1; if (i>=4 && j>=8)  CV(n,i,j);
      break;
    case 17: case 18: // 黑士
      i = x+1; j = y+1; if (i<=6 && j<=3) CV(n,i,j);
      i = x+1; j = y-1; if (i<=6 && j>=1) CV(n,i,j);
      i = x-1; j = y+1; if (i>=4 && j<=3) CV(n,i,j);
      i = x-1; j = y-1; if (i>=4 && j>=1) CV(n,i,j);
      break;
    case 3: case 4: // 红相
      i = x+2; j = y+2; if (i<=9 && j<=10) if (map[x+1][y+1]==EMPTY) CV(n,i,j);
      i = x+2; j = y-2; if (i<=9 && j>=6)  if (map[x+1][y-1]==EMPTY) CV(n,i,j);
      i = x-2; j = y+2; if (i>=1 && j<=10) if (map[x-1][y+1]==EMPTY) CV(n,i,j);
      i = x-2; j = y-2; if (i>=1 && j>=6)  if (map[x-1][y-1]==EMPTY) CV(n,i,j);
      break;
    case 19: case 20: // 黑象
      i = x+2; j = y+2; if (i<=9 && j<=5) if (map[x+1][y+1]==EMPTY) CV(n,i,j);
      i = x+2; j = y-2; if (i<=9 && j>=1) if (map[x+1][y-1]==EMPTY) CV(n,i,j);
      i = x-2; j = y+2; if (i>=1 && j<=5) if (map[x-1][y+1]==EMPTY) CV(n,i,j);
      i = x-2; j = y-2; if (i>=1 && j>=1) if (map[x-1][y-1]==EMPTY) CV(n,i,j);
      break;
    case 5: case 6: // 红马
      i = x+1; if (map[i][y]==EMPTY) {
        i=x+2; j=y+1; if (i<=9 && j<=10) CV(n,i,j);
        i=x+2; j=y-1; if (i<=9 && j>=1)  CV(n,i,j);
      }
      i = x-1; if (map[i][y]==EMPTY) {
        i=x-2; j=y+1; if (i>=1 && j<=10) CV(n,i,j);
        i=x-2; j=y-1; if (i>=1 && j>=1)  CV(n,i,j);
      }
      j = y+1; if (map[x][j]==EMPTY) {
        i=x+1; j=y+2; if (i<=9 && j<=10) CV(n,i,j);
        i=x-1; j=y+2; if (i>=1 && j<=10) CV(n,i,j);
      }
      j = y-1; if (map[x][j]==EMPTY) {
        i=x+1; j=y-2; if (i<=9 && j>=1) CV(n,i,j);
        i=x-1; j=y-2; if (i>=1 && j>=1) CV(n,i,j);
      }
      break;
    case 21: case 22: // 黑马
      i = x+1; if (map[i][y]==EMPTY) {
        i=x+2; j=y+1; if (i<=9 && j<=10) CV(n,i,j);
        i=x+2; j=y-1; if (i<=9 && j>=1)  CV(n,i,j);
      }
      i = x-1; if (map[i][y]==EMPTY) {
        i=x-2; j=y+1; if (i>=1 && j<=10) CV(n,i,j);
        i=x-2; j=y-1; if (i>=1 && j>=1)  CV(n,i,j);
      }
      j = y+1; if (map[x][j]==EMPTY) {
        i=x+1; j=y+2; if (i<=9 && j<=10) CV(n,i,j);
        i=x-1; j=y+2; if (i>=1 && j<=10) CV(n,i,j);
      }
      j = y-1; if (map[x][j]==EMPTY) {
        i=x+1; j=y-2; if (i<=9 && j>=1) CV(n,i,j);
        i=x-1; j=y-2; if (i>=1 && j>=1) CV(n,i,j);
      }
      break;
    case 7: case 8: case 9: case 10: // 红车
    case 23: case 24: case 25: case 26: { // 黑车
      // 车：4 方向直线（CV 记录沿途遇敌/己）
      for (i = x+1; i <= 9; i++) { CV(n,i,y); if (map[i][y] != EMPTY) break; }
      for (i = x-1; i >= 1; i--) { CV(n,i,y); if (map[i][y] != EMPTY) break; }
      for (j = y+1; j <= 10; j++) { CV(n,x,j); if (map[x][j] != EMPTY) break; }
      for (j = y-1; j >= 1; j--) { CV(n,x,j); if (map[x][j] != EMPTY) break; }
      break;
    }
    case 11: case 12: case 13: case 14: case 15: // 红炮
      // 炮：隔一子打（第一子=炮架，第二子=目标）
      for (i = x+1; i <= 9; i++) {
        if (map[i][y] != EMPTY) { for (i++; i <= 9; i++) { if (map[i][y] != EMPTY) { CV(n,i,y); break; } } break; }
      }
      for (i = x-1; i >= 1; i--) {
        if (map[i][y] != EMPTY) { for (i--; i >= 1; i--) { if (map[i][y] != EMPTY) { CV(n,i,y); break; } } break; }
      }
      for (j = y+1; j <= 10; j++) {
        if (map[x][j] != EMPTY) { for (j++; j <= 10; j++) { if (map[x][j] != EMPTY) { CV(n,x,j); break; } } break; }
      }
      for (j = y-1; j >= 1; j--) {
        if (map[x][j] != EMPTY) { for (j--; j >= 1; j--) { if (map[x][j] != EMPTY) { CV(n,x,j); break; } } break; }
      }
      break;
    case 27: case 28: case 29: case 30: case 31: // 黑炮
      for (i = x+1; i <= 9; i++) {
        if (map[i][y] != EMPTY) { for (i++; i <= 9; i++) { if (map[i][y] != EMPTY) { CV(n,i,y); break; } } break; }
      }
      for (i = x-1; i >= 1; i--) {
        if (map[i][y] != EMPTY) { for (i--; i >= 1; i--) { if (map[i][y] != EMPTY) { CV(n,i,y); break; } } break; }
      }
      for (j = y+1; j <= 10; j++) {
        if (map[x][j] != EMPTY) { for (j++; j <= 10; j++) { if (map[x][j] != EMPTY) { CV(n,x,j); break; } } break; }
      }
      for (j = y-1; j >= 1; j--) {
        if (map[x][j] != EMPTY) { for (j--; j >= 1; j--) { if (map[x][j] != EMPTY) { CV(n,x,j); break; } } break; }
      }
      break;
    default: break;
    }
  }

  // ---- SubThink 评估段移植 ----
  // 语义：评估输入 = "轮到自己走子前"的局面（纯静态评估，不含搜索终止判定）
  // 注意：将死/困毙由 search（genLegalMoves 为空 + inCheck）判定，评估函数不判杀棋
  // 原版 SubThink 的 9700/9800 是在递归搜索中由"无着法/吃王"触发，非静态评估职责
  int stm = pos.sideToMove();

  // 计算每子价值 v1[i] = BV1[type] + v1[i]*BV2[type]；兵加 BV3[BA表]
  for (int i = 0; i < 32; i++) {
    int k = manToType7(i);
    v1[i] = BV1[k] + v1[i] * BV2[k];
    if (k == 6) v1[i] += BV3[BA[sideOfMan(i)][many[i]][manx[i]]];
  }
  // 威胁 v3/v4
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 32; j++) {
      if (v2[i][j]) {
        if (sideOfMan(i) == sideOfMan(j)) { v3[i] += v1[j] >> 5; v4[j]++; }
        else { v3[i] += v1[j] >> 3; v4[j]--; }
      }
    }
  }
  // 求和：红方视角正
  int score = 0;
  for (int i = 0; i < 32; i++) {
    if (manx[i] == 0) continue;
    if (sideOfMan(i) == RED) score += v1[i] + v3[i];
    else score -= v1[i] + v3[i];
  }

  // 防兑子过快启发（原版在顶层/搜索中处理，此处保留核心评分）
  // TODO(W3): 评估函数重构（结构化特征）
  return score;
}
  
