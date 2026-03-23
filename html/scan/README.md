# mBoard 扫码 + 蓝牙配网原型

## 功能
- 手机扫码打开 `/scan?id=设备ID`
- 校验产品 ID（读取 `../data/products.csv`）
- 自动检查 ESP32 是否在线
- 若离线，弹窗通过蓝牙连接 ESP32 并下发 Wi-Fi 名称/密码
- ESP32 联网成功后回传状态并跳转 `/board?id=设备ID`
- 棋盘页显示：
  - 国际象棋 / 中国象棋棋盘
  - mBoard 基本信息
  - 历史操作记录

## 蓝牙配网协议
- Service UUID: `2f57c5fe-910f-4de9-8d0f-a7efb7b89a01`
- Characteristic:
  - `...02` 设备 ID（READ）
  - `...03` 配网写入（WRITE）
  - `...04` 状态通知（READ/NOTIFY）
  - `...05` 命令写入（WRITE，`SCAN_WIFI`）
  - `...06` Wi-Fi 列表通知（READ/NOTIFY）
- 配网写入 JSON：
```json
{"deviceId":"MTXXXXXXXXXXXX","ssid":"HomeWiFi","password":"12345678"}
```
- 状态通知 JSON：
```json
{"stage":"connecting|connected|failed","msg":"...","ip":"..."}
```
- Wi-Fi 列表通知 JSON：
```json
{"list":[{"ssid":"HomeWiFi","rssi":-53},{"ssid":"Office-2.4G","rssi":-67}]}
```

## 运行
```bash
cd /Users/convel/Documents/mTabula/html/scan
npm install
npm start
```

打开：
- `http://localhost:8866/scan?id=MBP1SCNEA26030001`（默认离线，触发配网弹窗）
- `http://localhost:8866/scan?id=MBL2CVHEA26080042`（默认在线，直接进棋盘）

## ESP32 固件
- 示例文件：`scan/esp32_ble_provision/esp32_ble_provision.ino`
- 仅用于 USB 读取芯片信息（给 PID 页面）：`scan/esp32_device_info/esp32_device_info.ino`
- 一体化联调（配网 + 心跳 + 霍尔上报）：`scan/esp32_hall_uplink/esp32_hall_uplink.ino`
- Arduino IDE 选择 ESP32 开发板后直接编译下载。
- 固件特性：
  - `device_id` 基于 eFuse MAC 生成
  - 通过 BLE 接收 Wi-Fi 配置
  - Wi-Fi 凭据写入 NVS（`Preferences`），重启后自动重连

## 浏览器要求
- Web Bluetooth 要求 HTTPS 或 localhost
- 推荐 Chrome(Android) / Chromium 桌面版
- iOS Safari 对 Web Bluetooth 支持有限，不建议用于首版配网

## 测试接口
- `POST /api/test/set-online` 可快速切设备在线状态

## 后端霍尔数据接口（Python backend）
- `POST /api/boards/<productId>/hall-frame`
- `GET /api/boards/<productId>/hall-latest`
