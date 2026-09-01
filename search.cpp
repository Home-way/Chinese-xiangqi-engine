// search.cpp — 搜索实现：迭代加深 + 朴素极大极小 + 时间控制
// 忠实移植原版棋力（前三候选 + 随机挑选），不增强算法
#include "search.h"
#include "movegen.h"
#include "eval.h"
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>

namespace {

// 顶层候选（原版前三候选语义简化：取前 3 个 best 用随机挑选）
struct Candidate { Move move; int score; };

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// 朴素极大极小（negamax），无 alpha-beta
// 返回该局面下当前走子方的分数（红方视角，stm 的分数）
// depth: 剩余层数；pos: 当前局面；nodeCount: 节点计数；deadline: 截止时间（0=不限）
int negamax(Position& pos, int depth, uint64_t& nodeCount, int64_t deadline, int stm) {
  nodeCount++;
  std::vector<Move> moves;
  genLegalMoves(pos, moves);
  if (moves.empty()) {
    // 无合法着法：若被将军 = 被将死（±9700），否则困毙（±9700 原版语义）
    return pos.inCheck(stm) ? -9700 : 9800;
  }
  if (depth == 0) {
    int score = evaluate(pos);
    return stm == RED ? score : -score;
  }
  int best = -20000;
  for (const Move& m : moves) {
    // 执行走子（需保存用于还原）
    int fromX = pos.manX(m.man), fromY = pos.manY(m.man);
    int cap = pos.pieceAt(m.toX, m.toY);
    // 吃将立即判胜
    if (cap == 0 || cap == 16) return 9999;
    bool ok = pos.makeMove(m);
    if (!ok) continue;
    pos.setSideToMove(stm == RED ? BLACK : RED);
    int val = -negamax(pos, depth-1, nodeCount, deadline, stm == RED ? BLACK : RED);
    // 还原
    pos.setSideToMove(stm);
    // 用 unmakeMove 语义还原（直接反操作）
    pos.unmakeMoveEx(m, fromX, fromY, cap);
    if (val > best) best = val;
    if (deadline && nowMs() >= deadline) break;
  }
  return best;
}

} // namespace

Move search(const Position& pos, const SearchConfig& cfg, SearchInfo* info) {
  int64_t t0 = nowMs();
  int64_t deadline = cfg.timeMs > 0 ? t0 + cfg.timeMs : 0;
  uint64_t nodes = 0;

  Position cur = pos;
  int stm = cur.sideToMove();

  std::vector<Move> legal;
  genLegalMoves(cur, legal);
  if (legal.empty()) {
    if (info) { info->depth = 0; info->nodes = 0; info->elapsedMs = 0; info->score = 0; }
    Move none; none.man = EMPTY;
    return none; // 无合法着法
  }

  // 迭代加深：depth 1 → maxDepth
  int maxDepth = cfg.maxDepth > 0 ? cfg.maxDepth : 6;
  int bestScore = 0;
  Move bestMove = legal[0];

  // 顶层候选（原版前三候选语义）
  std::vector<Candidate> top;
  top.reserve(3);

  for (int d = 1; d <= maxDepth; d++) {
    // 顶层：评估每步
    for (const Move& m : legal) {
      Position pm = cur;
      if (!pm.makeMove(m)) continue;
      pm.setSideToMove(stm == RED ? BLACK : RED);
      int val = -negamax(pm, d-1, nodes, deadline, stm == RED ? BLACK : RED);
      // 维护 top 候选（降序，最多 3 个）：按分数插入合适位置
      bool inserted = false;
      for (size_t i = 0; i < top.size(); i++) {
        if (val > top[i].score) {
          top.insert(top.begin() + (int)i, Candidate{m, val});
          if (top.size() > 3) top.pop_back();
          inserted = true;
          break;
        }
      }
      if (!inserted && top.size() < 3) top.push_back(Candidate{m, val});
      if (val > bestScore) { bestScore = val; bestMove = m; }
      if (deadline && nowMs() >= deadline) break;
    }
    if (info) { info->depth = d; info->score = bestScore; }
    if (deadline && nowMs() >= deadline) break;
  }

  // 原版"前三候选 + 随机挑选"：从 top 3 中随机选（保留原版行为，便于与原版对比）
  static bool seeded = false;
  if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }
  if (!top.empty()) {
    int pick = rand() % (int)top.size();
    bestMove = top[pick].move;
    bestScore = top[pick].score;
  }

  if (info) {
    info->nodes = nodes;
    info->elapsedMs = (double)(nowMs() - t0);
  }
  return bestMove;// 
}
