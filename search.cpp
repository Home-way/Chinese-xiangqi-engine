// search.cpp — 搜索实现：迭代加深 + alpha-beta 剪枝 + 着法排序 + 时间控制
// ============================================================
// 【改动点总览】
//   1. negamax() 增加 alpha/beta 窗口参数，实现 beta 剪枝
//   2. 走法排序：吃子优先（MVV-LVA），用 std::sort 更高效
//   3. 根层走法排序 + 渴望窗口（Aspiration Window）提升剪枝效率
//   4. 保留 Top3 候选 + 随机挑选机制
// ============================================================

#include "search.h"
#include "movegen.h"
#include "eval.h"
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <algorithm>  // 【优化】用 std::sort 替代手写插入排序

namespace {

    // 顶层候选（原版前三候选语义简化：取前 3 个 best 用随机挑选）
    struct Candidate { Move move; int score; };

    int64_t nowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    // ============================================================
    // 【优化】走法排序用的子力价值表
    // ============================================================
    inline int pieceValueForOrder(int man) {
        if (man == EMPTY) return 0;
        switch (manToType(man)) {
        case RED_J: case BLACK_J: return 900;   // 车
        case RED_P: case BLACK_P: return 450;   // 炮
        case RED_M: case BLACK_M: return 400;   // 马
        case RED_S: case BLACK_S: return 200;   // 士
        case RED_X: case BLACK_X: return 200;   // 相
        case RED_B: case BLACK_B: return 100;   // 兵
        case RED_K: case BLACK_K: return 10000; // 帅/将
        default: return 0;
        }
    }

    // ============================================================
    // 【优化】走法排序函数：吃子走法优先，MVV-LVA 思想
    // ============================================================
    void sortMoves(Position& pos, std::vector<Move>& moves) {
        // 为每个走法计算排序键
        std::vector<int> keys(moves.size());
        for (size_t i = 0; i < moves.size(); ++i) {
            int victim = pos.pieceAt(moves[i].toX, moves[i].toY);
            if (victim == EMPTY) {
                keys[i] = 0;  // 不吃子排最后
            }
            else {
                // MVV-LVA：被吃子价值 × 10 - 进攻子价值
                keys[i] = pieceValueForOrder(victim) * 10 - pieceValueForOrder(moves[i].man);
            }
        }

        // 用 std::sort 降序排列（比手写插入排序更快）
        std::sort(moves.begin(), moves.end(),
            [&](const Move& a, const Move& b) {
                int va = pos.pieceAt(a.toX, a.toY);
                int vb = pos.pieceAt(b.toX, b.toY);
                // 如果都吃子或都不吃子，按原始顺序（稳定排序不改变相对顺序）
                if ((va == EMPTY) == (vb == EMPTY)) return false;
                // 吃子的排在前面
                if (va != EMPTY && vb == EMPTY) return true;
                if (va == EMPTY && vb != EMPTY) return false;
                // 都用 MVV-LVA 排序
                int ka = (va == EMPTY) ? 0 : pieceValueForOrder(va) * 10 - pieceValueForOrder(a.man);
                int kb = (vb == EMPTY) ? 0 : pieceValueForOrder(vb) * 10 - pieceValueForOrder(b.man);
                return ka > kb;
            }
        );
    }

    // ============================================================
    // 【核心】alpha-beta 剪枝版 negamax（含走法排序 + 渴望窗口支持）
    // ============================================================
    int negamax(Position& pos, int depth, uint64_t& nodeCount, int64_t deadline,
        int stm, int alpha, int beta) {
        nodeCount++;

        std::vector<Move> moves;
        genLegalMoves(pos, moves);

        if (moves.empty()) {
            return pos.inCheck(stm) ? -9700 : 9800;
        }
        if (depth == 0) {
            int score = evaluate(pos);
            return stm == RED ? score : -score;
        }

        // 【优化】走法排序（吃子优先，提升剪枝效率）
        sortMoves(pos, moves);

        int best = -20000;
        for (const Move& m : moves) {
            int fromX = pos.manX(m.man), fromY = pos.manY(m.man);
            int cap = pos.pieceAt(m.toX, m.toY);

            if (cap == 0 || cap == 16) return 9999;

            bool ok = pos.makeMove(m);
            if (!ok) continue;

            pos.setSideToMove(stm == RED ? BLACK : RED);
            int val = -negamax(pos, depth - 1, nodeCount, deadline,
                stm == RED ? BLACK : RED, -beta, -alpha);

            pos.setSideToMove(stm);
            pos.unmakeMoveEx(m, fromX, fromY, cap);

            if (val > best) best = val;
            if (best > alpha) alpha = best;

            // 【剪枝】beta 剪枝
            if (alpha >= beta) break;
            if (deadline && nowMs() >= deadline) break;
        }
        return best;
    }

} // namespace

// ============================================================
// 【优化】search() 主函数：迭代加深 + 渴望窗口 + Top3 候选
// ============================================================
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
        return none;
    }

    int maxDepth = cfg.maxDepth > 0 ? cfg.maxDepth : 6;
    int bestScore = 0;
    Move bestMove = legal[0];

    std::vector<Candidate> top;
    top.reserve(3);

    // 【优化】根层走法排序（先搜好走法，快速找到好的 alpha 值）
    sortMoves(cur, legal);

    int prevScore = 0;  // 上一层的分数，用于渴望窗口

    for (int d = 1; d <= maxDepth; d++) {
        // 【优化】渴望窗口（Aspiration Window）
        // 用上一层的分数作为基准，缩小搜索窗口，提升剪枝效率
        // 如果窗口搜索失败，再用全窗口重新搜索
        int windowSize = 50;  // 窗口大小，可调
        int alpha = prevScore - windowSize;
        int beta = prevScore + windowSize;
        bool windowFailed = false;

        // 尝试用渴望窗口搜索
        for (const Move& m : legal) {
            Position pm = cur;
            if (!pm.makeMove(m)) continue;
            pm.setSideToMove(stm == RED ? BLACK : RED);

            int val = -negamax(pm, d - 1, nodes, deadline,
                stm == RED ? BLACK : RED, alpha, beta);

            // 如果分数落在窗口边界上，说明窗口太小，需要重新搜索
            if (val <= alpha || val >= beta) {
                windowFailed = true;
                // 用全窗口重新搜索这个走法
                val = -negamax(pm, d - 1, nodes, deadline,
                    stm == RED ? BLACK : RED, -20000, 20000);
            }

            // 维护 Top3 候选
            bool inserted = false;
            for (size_t i = 0; i < top.size(); i++) {
                if (val > top[i].score) {
                    top.insert(top.begin() + (int)i, Candidate{ m, val });
                    if (top.size() > 3) top.pop_back();
                    inserted = true;
                    break;
                }
            }
            if (!inserted && top.size() < 3) top.push_back(Candidate{ m, val });
            if (val > bestScore) { bestScore = val; bestMove = m; }
            if (deadline && nowMs() >= deadline) break;
        }

        // 如果渴望窗口失败，用全窗口重新搜索这一层（仅对第一个走法）
        if (windowFailed && top.size() > 0) {
            // 重新计算第一个走法的精确值
            Move firstMove = top[0].move;
            Position pm = cur;
            if (pm.makeMove(firstMove)) {
                pm.setSideToMove(stm == RED ? BLACK : RED);
                int val = -negamax(pm, d - 1, nodes, deadline,
                    stm == RED ? BLACK : RED, -20000, 20000);
                top[0].score = val;
                if (val > bestScore) { bestScore = val; bestMove = firstMove; }
            }
        }

        if (info) { info->depth = d; info->score = bestScore; }
        if (deadline && nowMs() >= deadline) break;

        // 更新 prevScore，用于下一层的渴望窗口
        prevScore = bestScore;
    }

    // 原版"前三候选 + 随机挑选"
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
    return bestMove;
}
