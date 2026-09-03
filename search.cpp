#include "search.h"
#include "movegen.h"
#include "eval.h"
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <algorithm>

namespace {

    // 历史表启发
    static int history[32][11][12] = { 0 };
    const double HISTORY_DECAY = 0.9;

    // 搜索计数器，用于定期衰减历史表（不是清零，而是大幅衰减）
    static int searchCount = 0;
    const int DECAY_INTERVAL = 50000;  // 每50000次搜索衰减一次

    struct Candidate { Move move; int score; };

    int64_t nowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    inline int pieceValueForOrder(int man) {
        if (man == EMPTY) return 0;
        switch (manToType(man)) {
        case RED_J: case BLACK_J: return 900;
        case RED_P: case BLACK_P: return 450;
        case RED_M: case BLACK_M: return 400;
        case RED_S: case BLACK_S: return 200;
        case RED_X: case BLACK_X: return 200;
        case RED_B: case BLACK_B: return 100;
        case RED_K: case BLACK_K: return 10000;
        default: return 0;
        }
    }

    // 【优化】走法排序：不用 keys 数组，直接算键
    void sortMoves(Position& pos, std::vector<Move>& moves) {
        std::sort(moves.begin(), moves.end(),
            [&](const Move& a, const Move& b) {
                int va = pos.pieceAt(a.toX, a.toY);
                int vb = pos.pieceAt(b.toX, b.toY);

                int ka = (va == EMPTY) ?
                    history[a.man][a.toX][a.toY] :
                    pieceValueForOrder(va) * 10 - pieceValueForOrder(a.man) +
                    history[a.man][a.toX][a.toY];

                int kb = (vb == EMPTY) ?
                    history[b.man][b.toX][b.toY] :
                    pieceValueForOrder(vb) * 10 - pieceValueForOrder(b.man) +
                    history[b.man][b.toX][b.toY];

                return ka > kb;
            }
        );
    }

    // 【优化】衰减单个走法的历史值（用到的才衰减，不用扫全表）
    inline void decayHistoryForMove(int man, int x, int y) {
        history[man][x][y] = (int)(history[man][x][y] * HISTORY_DECAY);
    }

    // 【优化】定期大幅衰减历史表（保留学习成果，防止溢出）
    inline void decayHistoryTable() {
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 11; j++) {
                for (int k = 0; k < 12; k++) {
                    history[i][j][k] = (int)(history[i][j][k] * 0.5);  // 大幅衰减，不全清零
                }
            }
        }
        searchCount = 0;
    }

    // alpha-beta 剪枝版 negamax
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

            if (alpha >= beta) {
                // 历史表加分
                history[m.man][m.toX][m.toY] += depth * depth;
                // 【优化】同时衰减这个位置（避免无限增长）
                decayHistoryForMove(m.man, m.toX, m.toY);
                break;
            }
            if (deadline && nowMs() >= deadline) break;
        }
        return best;
    }

} // namespace

// search() 主函数
Move search(const Position& pos, const SearchConfig& cfg, SearchInfo* info) {
    int64_t t0 = nowMs();
    int64_t deadline = cfg.timeMs > 0 ? t0 + cfg.timeMs : 0;
    uint64_t nodes = 0;

    Position cur = pos;
    int stm = cur.sideToMove();

    // 【优化】定期大幅衰减历史表（保留学习成果，防止溢出）
    if (++searchCount >= DECAY_INTERVAL) {
        decayHistoryTable();
    }

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

    sortMoves(cur, legal);

    int prevScore = 0;

    for (int d = 1; d <= maxDepth; d++) {
        int windowSize = 50;
        int alpha = prevScore - windowSize;
        int beta = prevScore + windowSize;
        bool windowFailed = false;

        for (const Move& m : legal) {
            Position pm = cur;
            if (!pm.makeMove(m)) continue;
            pm.setSideToMove(stm == RED ? BLACK : RED);

            int val = -negamax(pm, d - 1, nodes, deadline,
                stm == RED ? BLACK : RED, alpha, beta);

            if (val <= alpha || val >= beta) {
                windowFailed = true;
                val = -negamax(pm, d - 1, nodes, deadline,
                    stm == RED ? BLACK : RED, -20000, 20000);
            }

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

        if (windowFailed && top.size() > 0) {
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
