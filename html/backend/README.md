# mTabula Python Backend

## Start

```bash
cd /Users/convel/Documents/mTabula/html
python3 ./backend/server.py --host 0.0.0.0 --port 8866
```

Then open:

- `http://127.0.0.1:8866/index.html`
- `http://127.0.0.1:8866/gamespace.html?productId=MBP1SCNEA26030001`
- `http://127.0.0.1:8866/pid-login.html`

## HTTPS start (for Web Serial / Web Bluetooth secure context)

Generate local cert/key (example with OpenSSL):

```bash
openssl req -x509 -nodes -newkey rsa:2048 -days 825 \
  -keyout mboard-key.pem -out mboard-cert.pem
```

Start HTTPS:

```bash
cd /Users/convel/Documents/mTabula/html
python3 ./backend/server.py --host 0.0.0.0 --port 8866 \
  --https --cert-file /path/to/mboard-cert.pem --key-file /path/to/mboard-key.pem
```

Then open with LAN IP:

- `https://<your-lan-ip>:8866/pid.html`
- `https://<your-lan-ip>:8866/scan?id=<productId>`

## Product ID whitelist

- CSV path: `data/products.csv`
- Only `product_id` in this CSV can pass `/api/scan/verify`

Example:

```csv
product_id,model,active
MBP1SCNEA26030001,mBoard-pro-demo,1
```

## Data output

After verification passes, backend auto-creates:

- `data/products/<productId>/games.csv`
- `data/products/<productId>/configs.csv`
- `data/products/<productId>/commands.csv`
- `data/products/<productId>/sensor_moves.csv`
- `data/products/<productId>/meta.json`

## Product Enroll API

- `POST /api/admin/products`
  - Input naming fields (tierCode/generation/originCode/gameCode/sensorCode/chipCode/swCode/productionMonth/reserved/serial)
  - Backend generates `productId`, writes to `data/products.csv`, returns:
    - `accessPath` (for gamespace path)
    - `accessUrl`
    - `qrUrl`

## PID 登录鉴权

- 登录页：`/pid-login.html`
- 受保护页面：`/pid.html`（未登录会自动跳转到 `/pid-login.html`）
- 账号文件：`data/admin_users.csv`
- 鉴权接口：
  - `GET /api/auth/me`
  - `POST /api/auth/login`
  - `POST /api/auth/logout`
- `GET/POST /api/admin/products` 需要先登录

默认管理员（首次）：
- username: `admin`
- password: `admin`

建议立即修改为自定义强密码，并配置环境变量 `MTABULA_AUTH_PEPPER` 作为服务端额外密钥。

可用脚本更新管理员账号（推荐）：

```bash
cd /Users/convel/Documents/mTabula/html
python3 ./backend/set_admin_user.py --username your_admin_name
```
