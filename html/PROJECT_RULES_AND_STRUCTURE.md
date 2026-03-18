# mTabula 项目规则与目录说明

更新时间：2026-03-18
路径：`/Users/convel/Documents/mTabula/html`

## 1. 目标与当前实现
- 本项目用于 mTabula 智能棋盘网页端 + Python 后端联调。
- 扫码行为在棋盘端执行，网页端不展示扫码二维码。
- 产品接入必须先通过 `productId` 白名单校验（CSV）。
- 数据库暂用 CSV，按产品 ID 分目录落盘。

## 2. Product ID 命名规则

### 2.1 规则格式
`MB + 型号 + 代际 + 产地 + 棋种 + 感应 + 芯片 + 软件 + 年(2位) + 月(2位) + 保留位(2位) + 流水号(2位)`

示例：`MBP1SCNEA26030001`

### 2.2 字段定义
- `MB`: 固定前缀
- `型号(1位)`: `B`/`P`/`L`（basic/pro/lite）
- `代际(1位)`: `0-9` 或 `A-Z` 单字符
- `产地(1位)`: 单字符编码
- `棋种(1位)`: `C`(国际象棋) / `V`(中国象棋)
- `感应(1位)`: `N`(NFC) / `H`(Hall)
- `芯片(1位)`: `E`(ESP32) / `A`(Arduino) / `S`(STM)
- `软件(1位)`: 单字符编码
- `年月(4位)`: 例如 `2603` 表示 2026-03
- `保留位(2位)`: 默认 `00`
- `流水号(2位)`: `00-99`

### 2.3 校验约束
- 正则：`^MB[A-Z0-9]{15}$`
- 全局基本约束：`^[A-Za-z0-9_-]{3,64}$`（通用 productId 输入检查）
- 录入接口会校验字段组合与 `productId` 一致性（若前端传了 `productId`）。

## 3. 白名单与扫码校验规则
- 白名单文件：`data/products.csv`
- 只有 `products.csv` 中存在且 `active` 非 `0/false/no/off` 的 `product_id` 才允许接入。
- 校验接口：`POST /api/scan/verify`
- 校验通过后自动创建目录：`data/products/<productId>/`

## 4. 数据落盘规则（CSV）
校验通过并开始交互后，数据写入：
- `data/products/<productId>/games.csv`（对弈走子）
- `data/products/<productId>/configs.csv`（棋盘配置）
- `data/products/<productId>/commands.csv`（控制命令，如 home）
- `data/products/<productId>/sensor_moves.csv`（棋盘端传感回传）
- `data/products/<productId>/meta.json`（目录元信息）

## 5. 后端接口约定（Python）
服务文件：`backend/server.py`

### 5.1 基础
- `GET /api/health`：健康检查

### 5.2 产品录入
- `GET /api/admin/products`：查看当前产品表
- `POST /api/admin/products`：按命名规则录入产品并写入 `products.csv`
  - 返回：`productId`、`workspace`、`accessPath`、`accessUrl`、`qrUrl`
  - 以上接口需要登录后调用

### 5.3 登录鉴权
- `GET /api/auth/me`：查看登录态
- `POST /api/auth/login`：账号密码登录
- `POST /api/auth/logout`：退出登录
- 管理账号文件：`data/admin_users.csv`（CSV 存储用户名 + 盐值 + 密码哈希）

### 5.4 扫码校验
- `POST /api/scan/verify`
  - 入参：`productId`
  - 通过后返回：`accessPath`、`accessUrl`、`qrUrl`

### 5.5 棋盘业务
- `GET /api/boards/<productId>/status`
- `POST /api/boards/<productId>/moves`
- `POST /api/boards/<productId>/config`
- `POST /api/boards/<productId>/command/home`
- `GET /api/boards/<productId>/sensor-moves?after=<id>`
- `POST /api/boards/<productId>/sensor-moves`
- `GET /api/boards/history?boardId=<productId>`

## 6. 前端页面规则

### 6.1 首页
- 文件：`index.html`
- 导航跳转：产品简介 / 玩法模式 / 核心技术 / 游戏空间 / 联系我们

### 6.2 游戏空间
- 文件：`gamespace.html` + `gamespace.js` + `gamespace.css`
- 入口参数：`?productId=...`
- 页面初始化先调用 `/api/scan/verify`
  - 未通过：禁用交互按钮
  - 通过：允许对弈、写配置、写历史
- 标签：棋盘游戏 / 游玩历史 / RPG游戏 / 历史故事 / 棋盘配置
- 国际象棋：基于 `chess.js + chessboard.js`，支持合法走子检查、Status/FEN/PGN

### 6.3 产品录入页
- 登录页：`pid-login.html`
- 管理页：`pid.html` + `pid.js` + `pid.css`
- 输入命名字段后自动生成 `productId`
- 提交到 `/api/admin/products`
- 成功后显示访问路径与二维码
- 未登录访问 `pid.html` 会自动重定向到 `pid-login.html`

### 6.4 联系我们
- 文件：`contact.html`
- 用作团队介绍与招聘模板

## 7. 目录结构与内容说明

### 7.1 根目录关键文件
- `index.html`：官网首页
- `gamespace.html/js/css`：游戏空间
- `pid-login.html/js/css`：PID 登录页面
- `pid.html/js/css`：PID 管理与产品录入页面（受保护）
- `contact.html`：联系我们模板页
- `styles.css`、`script.js`：首页样式与背景动画
- `PROJECT_RULES_AND_STRUCTURE.md`：本说明文档

### 7.2 `backend/`
- `server.py`：Python API + 静态文件服务
- `run.sh`：启动脚本
- `README.md`：后端启动与接口简述

### 7.3 `data/`
- `products.csv`：产品白名单与产品信息
- `admin_users.csv`：后台登录用户（盐值 + 哈希密码）
- `products/<productId>/`：每个产品独立数据目录（CSV + meta）

### 7.4 `js/`
- `chess.js`：国际象棋规则引擎
- `chessboard-1.0.0.js`：棋盘组件
- `jquery-3.7.1.min.js`：当前使用版本
- `jquery-1.7.1.min.js`：旧版保留（不建议新页面使用）

### 7.5 `css/`
- `chessboard-1.0.0.css`：棋盘组件样式

### 7.6 `img/`
- `img/chesspieces/wikipedia/`：国际象棋棋子图片资源

### 7.7 `scan/`（历史测试项目）
- 早期 Node.js 扫码测试样例，当前主流程已切换到 `backend/server.py`

### 7.8 其他
- `.idea/`：IDE 配置
- `testChess.html`：国际象棋库的测试页
- `README.md / LICENSE.md / CHANGELOG.md`：第三方库说明

## 8. 启动与联调

```bash
cd /Users/convel/Documents/mTabula/html
./backend/run.sh
```

访问：
- `http://127.0.0.1:8866/index.html`
- `http://127.0.0.1:8866/pid-login.html`
- `http://127.0.0.1:8866/gamespace.html?productId=MBP1SCNEA26030001`

## 9. 当前约束与后续建议
- 当前是 CSV 存储，适合单机原型联调，不适合高并发。
- 可后续迁移到 SQLite/MySQL，并保留同样 API。
- 已支持登录鉴权；建议继续补充失败次数限制、审计日志与 HTTPS 部署。
