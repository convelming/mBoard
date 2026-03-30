# AlphaZero Chess: Algorithm and I/O Details

本文说明 `html/python_chess` 的训练算法、张量维度、以及关键模块输入输出。

## 1. Action Space (8x8x73)

固定动作空间：
- `ACTION_SIZE = 8 * 8 * 73 = 4672`

含义：
- 64 个起点格（from-square）
- 每个起点 73 个动作模板（plane）

73 个模板拆分：
- `0..55`: 滑动动作（8个方向 x 1~7格）
- `56..63`: 马跳（8种）
- `64..72`: 欠升变（升马/象/车 x 左吃/前进/右吃）

规则约束：
- 网络始终输出 4672 维 logits
- 通过 `legal mask` 把非法动作概率置 0
- 对合法动作重新归一化

## 2. State Encoding

状态输入：`state ∈ R^{18x8x8}`

平面定义：
- `0..5`: 白方 `P,N,B,R,Q,K`
- `6..11`: 黑方 `P,N,B,R,Q,K`
- `12`: side-to-move plane
- `13`: white K-side castling right
- `14`: white Q-side castling right
- `15`: black K-side castling right
- `16`: black Q-side castling right
- `17`: normalized halfmove clock

数据类型：`float32`

## 3. Network

文件：`engine/alphazero_model.py`

输入：
- `x`: `[B, 18, 8, 8]`

输出：
- `policy_logits`: `[B, 4672]`
- `value`: `[B, 1]`，`tanh` 到 `[-1,1]`

结构：
- stem conv
- residual tower
- policy head
- value head

## 4. MCTS (PUCT)

文件：`engine/mcts.py`

每次搜索 `search(rules)`：
1. 根节点网络评估（policy + value）
2. 执行 `simulations` 次模拟：
   - Selection: 选最大 `Q + U`
   - Expansion: 在叶节点按合法动作展开
   - Evaluation: 网络评估叶节点
   - Backup: 值回传，按走子方交替翻转符号
3. 根节点访问次数归一化得到 `pi`

返回：
- `best_move`
- `pi: [4672]`

## 5. Self-Play Data Generation

文件：`engine/selfplay.py`

对局内每步保存：
- `state_t`
- `pi_t`（MCTS根访问分布）
- `turn_sign_t`（白方=+1，黑方=-1）

终局结果转 value：
- 白胜 `z_final=+1`
- 黑胜 `z_final=-1`
- 和棋 `z_final=0`

样本标签：
- `z_t = z_final * turn_sign_t`

得到训练样本 `(state_t, pi_t, z_t)`。

## 6. Training Loop

文件：`engine/train.py`

每个 iteration：
1. 用当前模型做若干局自博弈
2. 聚合样本数组：
   - `states`: `[N,18,8,8]`
   - `policies`: `[N,4672]`
   - `values`: `[N]`
3. 训练若干 epoch
4. 保存 checkpoint: `az_iter_XXX.pt`

损失函数：
- `policy_loss = -sum(pi * log_softmax(logits))`
- `value_loss = MSE(v, z)`
- `total_loss = policy_loss + value_loss`

## 7. API Contract

文件：`api/server.py`

- `GET /api/health`
- `POST /api/train/start`
  - 输入：`iters, games_per_iter, sims, epochs, batch_size`
- `GET /api/train/status`
  - 输出：running 状态和历史日志
- `POST /api/play/move`
  - 输入：`fen`, `human_move_uci`(optional), `sims`
  - 输出：`ai_move`, `fen`, `result`
- `WS /ws/train`
  - 推送训练过程事件

## 8. Frontend Coupling

- `html/az/train.html`
  - 控制训练参数、轮询状态、显示日志
- `html/az/play.html`
  - 用 `chess.js + chessboard.js` 展示棋盘
  - 用户走子后调用 `/api/play/move`

## 9. Practical Notes

- 该实现用于工程原型与教学，非比赛级强度。
- 若要提升棋力：
  - 增大自博弈规模
  - 增大网络容量
  - 加入 replay buffer、数据增广、模型评估替换策略
- 若面向 ESP32：建议仅端侧控制，搜索在服务器执行。
