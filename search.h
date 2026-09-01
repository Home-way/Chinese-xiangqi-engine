#pragma once
// search.h — 搜索：迭代加深 + 朴素极大极小 + 时间控制
// ⚠️ 平台契约：search() 接口定稿后只增不改（后续编译流水线依赖）

#include "types.h"
#include "position.h"
#include <cstdint>

struct SearchConfig {
  int    maxDepth  = 6;   // 迭代加深上限
  int64_t timeMs   = 0;   // 0=不限时（按深度走满）；>0 为时限（毫秒）
};

struct SearchInfo {
  int      depth = 0;
  uint64_t nodes = 0;
  double   elapsedMs = 0;
  int      score = 0;     // 当前最佳分数（红方视角）
};

// 返回最佳着法；无合法着法时返回 {man=EMPTY}（UCCI 层输出 nobestmove）
Move search(const Position& pos, const SearchConfig& cfg, SearchInfo* info = nullptr);
 
