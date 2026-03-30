# mTabula Python Chess (AlphaZero-style)

该项目基于 AlphaZero 思路实现国际象棋训练与对弈，动作空间采用固定维度：

- `8 x 8 x 73 = 4672`

其中：
- `8x8` 表示起点格（from-square）
- `73` 表示每个起点格上的动作模板（56滑动 + 8马跳 + 9欠升变）

## 目录

- `engine/action_space.py`: `8x8x73` 动作编码
- `engine/rules.py`: 规则层 + 输入平面编码 + legal mask
- `engine/alphazero_model.py`: 策略-价值网络
- `engine/mcts.py`: PUCT MCTS（策略改进器）
- `engine/selfplay.py`: 自博弈样本生成
- `engine/train.py`: 迭代训练循环
- `engine/difficulty.py`: checkpoint 到难度映射
- `api/server.py`: FastAPI + WebSocket（训练日志/对弈接口）

## 输入输出维度（核心）

### 1) 网络输入（状态编码）

- `state`: `float32[18, 8, 8]`

平面语义：
- `0..5`: 白方 `P,N,B,R,Q,K`
- `6..11`: 黑方 `P,N,B,R,Q,K`
- `12`: 轮到白方则全1，否则全0
- `13..16`: 四个易位权
- `17`: halfmove clock 归一化

### 2) 网络输出

- `policy_logits`: `float32[4672]`
- `value`: `float32[1]`，范围 `[-1, 1]`

### 3) 监督目标（训练）

- `pi`: `float32[4672]`，来自 MCTS 根节点访问次数分布
- `z`: `float32[1]`，终局胜负映射到当前状态执子视角

## 算法步骤（简版）

1. 用当前网络进行自博弈（每步通过 MCTS 搜索）
2. 记录样本 `(state, pi, z)`
3. 用样本训练网络，损失为：
   - `policy_loss = -sum(pi * log_softmax(policy_logits))`
   - `value_loss = mse(value, z)`
   - `loss = policy_loss + value_loss`
4. 保存 checkpoint
5. 重复迭代

更完整步骤见：[ALGORITHM_AND_IO.md](/Users/convel/Documents/mTabula/html/python_chess/ALGORITHM_AND_IO.md)

## Quick Start

```bash
cd /Users/convel/Documents/mTabula/html/python_chess
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 -m api.server
```

默认服务：`http://127.0.0.1:8899`

## 前端联调

- 训练可视化页面：`/Users/convel/Documents/mTabula/html/az/train.html`
- 人机对弈页面：`/Users/convel/Documents/mTabula/html/az/play.html`

推荐启动静态服务：

```bash
cd /Users/convel/Documents/mTabula/html
python3 -m http.server 8080
```

然后访问：
- `http://127.0.0.1:8080/az/train.html`
- `http://127.0.0.1:8080/az/play.html`

## 最小训练验证（Smoke Test）

```bash
cd /Users/convel/Documents/mTabula/html/python_chess
source .venv/bin/activate
python3 - <<'PY'
from engine.train import AlphaZeroTrainer, TrainConfig
cfg = TrainConfig(iters=1, games_per_iter=1, sims=8, epochs=1, batch_size=16)
AlphaZeroTrainer(cfg).train_loop(print)
PY
```

## 命令行训练脚本（PyCharm 可直接运行）

新增脚本：[run_train.py](/Users/convel/Documents/mTabula/html/python_chess/run_train.py)

示例（每累计 10 个 epoch 保存一次权重）：

```bash
cd /Users/convel/Documents/mTabula/html/python_chess
source .venv/bin/activate
python3 run_train.py \
  --device cpu \
  --iters 30 \
  --games-per-iter 4 \
  --sims 64 \
  --epochs 2 \
  --batch-size 64 \
  --checkpoint-every-epochs 10
```

说明：
- `checkpoint-every-epochs` 按累计训练 epoch 计数（`iters * epochs`）。
- 例如 `epochs=2`、`checkpoint-every-epochs=10`，会在第 `5,10,15...` 个 iteration 保存。

## ESP32部署建议

- 该网络原始规模较大，不建议在 ESP32 上跑完整 MCTS。
- 推荐：
  1. ESP32 负责运动控制 + 传感
  2. 后端服务器执行搜索与推理
  3. 如需端侧离线，仅部署量化后小模型+低算力策略
