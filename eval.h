#pragma once
// eval.h — 局面评估（移植自 xiangqi1 ContactV + SubThink 评估段）

#include "position.h"

// 局面评估：红方视角为正（与原版符号约定一致）。纯函数，无副作用。
int evaluate(const Position& pos);
