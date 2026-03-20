import express from 'express';
import path from 'path';
import { fileURLToPath } from 'url';
import fs from 'fs/promises';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const PORT = process.env.PORT || 8866;
const PRODUCTS_CSV = path.resolve(__dirname, '..', 'data', 'products.csv');

// In-memory demo data
const devices = {
  MBP1SCNEA26030001: {
    id: 'MBP1SCNEA26030001',
    model: 'MB-P1-S-C-N-E-A',
    gameType: 'chess',
    online: false,
    fw: '1.2.3',
    lastSeen: null,
    history: [
      '系统启动',
      '扫码会话创建',
      '等待设备联网'
    ]
  },
  MBL2CVHEA26080042: {
    id: 'MBL2CVHEA26080042',
    model: 'MB-L2-C-V-H-E-A',
    gameType: 'xiangqi',
    online: true,
    fw: '0.9.8',
    lastSeen: Date.now(),
    history: [
      '设备在线',
      '用户A进入房间',
      '完成一局对弈'
    ]
  }
};

const wifiList = [
  { ssid: 'MBOARD_LAB_5G', rssi: -42 },
  { ssid: 'Office-2.4G', rssi: -58 },
  { ssid: 'HomeWiFi', rssi: -66 }
];

function getDevice(id) {
  if (!devices[id]) {
    devices[id] = {
      id,
      model: 'MB-UNKNOWN',
      gameType: 'chess',
      online: false,
      fw: '0.0.1',
      lastSeen: null,
      history: ['新设备注册']
    };
  }
  return devices[id];
}

function parseCsvLine(line) {
  const out = [];
  let cur = '';
  let inQuotes = false;
  for (let i = 0; i < line.length; i += 1) {
    const ch = line[i];
    if (ch === '"') {
      if (inQuotes && line[i + 1] === '"') {
        cur += '"';
        i += 1;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (ch === ',' && !inQuotes) {
      out.push(cur);
      cur = '';
    } else {
      cur += ch;
    }
  }
  out.push(cur);
  return out;
}

async function readAuthorizedProducts() {
  try {
    const text = await fs.readFile(PRODUCTS_CSV, 'utf-8');
    const lines = text.split(/\r?\n/).filter(Boolean);
    if (lines.length < 2) return new Map();

    const headers = parseCsvLine(lines[0]).map(h => h.trim());
    const idxId = headers.indexOf('product_id');
    const idxActive = headers.indexOf('active');
    const idxModel = headers.indexOf('model');
    const idxGame = headers.indexOf('game_code');
    if (idxId < 0) return new Map();

    const map = new Map();
    for (let i = 1; i < lines.length; i += 1) {
      const cols = parseCsvLine(lines[i]);
      const productId = String(cols[idxId] || '').trim();
      if (!productId) continue;
      const active = String(cols[idxActive] || '1').trim() !== '0';
      if (!active) continue;
      map.set(productId, {
        model: String(cols[idxModel] || '').trim() || 'mBoard',
        gameCode: String(cols[idxGame] || '').trim() || 'C'
      });
    }
    return map;
  } catch (err) {
    return new Map();
  }
}

async function ensureAuthorized(id) {
  const products = await readAuthorizedProducts();
  const p = products.get(id);
  if (!p) return { ok: false };
  return { ok: true, product: p };
}

app.get('/api/product/verify', async (req, res) => {
  const id = String(req.query.id || '').trim();
  if (!id) return res.status(400).json({ ok: false, error: 'missing id' });
  const auth = await ensureAuthorized(id);
  if (!auth.ok) {
    return res.status(403).json({ ok: false, error: 'productId not authorized' });
  }
  res.json({ ok: true, id, model: auth.product.model, gameCode: auth.product.gameCode });
});

app.get('/api/device/status', async (req, res) => {
  const id = String(req.query.id || '').trim();
  if (!id) return res.status(400).json({ ok: false, error: 'missing id' });
  const auth = await ensureAuthorized(id);
  if (!auth.ok) return res.status(403).json({ ok: false, error: 'productId not authorized' });
  const d = getDevice(id);
  res.json({ ok: true, id: d.id, online: d.online, gameType: d.gameType, lastSeen: d.lastSeen });
});

app.get('/api/device/wifi-list', (_req, res) => {
  res.json({ ok: true, list: wifiList });
});

app.post('/api/device/provision', async (req, res) => {
  const { id, ssid, password } = req.body || {};
  if (!id || !ssid || !password) {
    return res.status(400).json({ ok: false, error: 'id/ssid/password required' });
  }
  const auth = await ensureAuthorized(String(id));
  if (!auth.ok) return res.status(403).json({ ok: false, error: 'productId not authorized' });
  const d = getDevice(String(id));
  // Demo behavior: provision success => mark online
  d.online = true;
  d.lastSeen = Date.now();
  d.history.unshift(`已配置WiFi: ${ssid}`);
  d.history.unshift('设备上线');
  res.json({ ok: true, id: d.id, online: true });
});

app.get('/api/device/info', (req, res) => {
  const id = String(req.query.id || '').trim();
  if (!id) return res.status(400).json({ ok: false, error: 'missing id' });
  const d = getDevice(id);
  res.json({
    ok: true,
    id: d.id,
    model: d.model,
    gameType: d.gameType,
    fw: d.fw,
    online: d.online,
    lastSeen: d.lastSeen
  });
});

app.get('/api/device/history', (req, res) => {
  const id = String(req.query.id || '').trim();
  if (!id) return res.status(400).json({ ok: false, error: 'missing id' });
  const d = getDevice(id);
  res.json({ ok: true, id: d.id, history: d.history });
});

// Test helper: quickly toggle online state
app.post('/api/test/set-online', (req, res) => {
  const { id, online } = req.body || {};
  if (!id) return res.status(400).json({ ok: false, error: 'missing id' });
  const d = getDevice(String(id));
  d.online = !!online;
  d.lastSeen = d.online ? Date.now() : d.lastSeen;
  d.history.unshift(d.online ? '测试:设备置为在线' : '测试:设备置为离线');
  res.json({ ok: true, id: d.id, online: d.online });
});

app.get('/scan', (_req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'scan.html'));
});

app.get('/board', (_req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'board.html'));
});

app.listen(PORT, () => {
  console.log(`mBoard scan test server: http://localhost:${PORT}/scan?id=MBP1SCNEA26030001`);
});
