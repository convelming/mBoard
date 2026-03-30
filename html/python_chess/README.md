# mTabula Python Chess (AlphaZero-style)

该项目提供一个可扩展的国际象棋训练/对弈框架：

- `engine/rules.py`: 规则层（基于 `python-chess`，与 chess.js 一致的合法走法语义）
- `engine/alphazero_model.py`: 策略-价值网络
- `engine/mcts.py`: PUCT MCTS 搜索
- `engine/selfplay.py`: 自博弈数据生成
- `engine/train.py`: 训练循环
- `engine/difficulty.py`: 不同训练阶段权重映射为难度
- `api/server.py`: FastAPI + WebSocket（训练进度推送、人机对弈接口）

## Quick Start

```bash
cd /Users/convel/Documents/mTabula/html/python_chess
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 -m api.server
```

默认服务：`http://127.0.0.1:8899`

## 与前端集成

- 训练可视化页面：`/Users/convel/Documents/mTabula/html/az/train.html`
- 人机对弈页面：`/Users/convel/Documents/mTabula/html/az/play.html`

前端使用 `chess.js` 做交互与棋盘展示，后端做 AI 搜索和训练。

推荐用本地静态服务打开页面（避免浏览器本地文件跨域限制）：

```bash
cd /Users/convel/Documents/mTabula/html
python3 -m http.server 8080
```

然后访问：

- `http://127.0.0.1:8080/az/train.html`
- `http://127.0.0.1:8080/az/play.html`

## ESP32后续部署建议

- 训练后导出小模型（ONNX/TFLite int8）
- 在 ESP32 上建议只做轻量推理或策略表查询
- 若要完整 MCTS，建议由后端执行搜索，ESP32做控制与通信
