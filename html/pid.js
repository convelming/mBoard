(function () {
  var form = document.getElementById('enrollForm');
  var statusLine = document.getElementById('statusLine');
  var productIdEl = document.getElementById('productId');
  var resultCard = document.getElementById('resultCard');
  var resultProductId = document.getElementById('resultProductId');
  var resultPath = document.getElementById('resultPath');
  var resultUrl = document.getElementById('resultUrl');
  var resultWorkspace = document.getElementById('resultWorkspace');
  var qrImage = document.getElementById('qrImage');
  var copyBtn = document.getElementById('copyBtn');
  var logoutLink = document.getElementById('logoutLink');
  var readEspBtn = document.getElementById('readEspBtn');
  var useEspSerialBtn = document.getElementById('useEspSerialBtn');

  var tierEl = document.getElementById('tierCode');
  var generationEl = document.getElementById('generation');
  var originEl = document.getElementById('originCode');
  var gameEl = document.getElementById('gameCode');
  var sensorEl = document.getElementById('sensorCode');
  var chipEl = document.getElementById('chipCode');
  var swEl = document.getElementById('swCode');
  var monthEl = document.getElementById('productionMonth');
  var reservedEl = document.getElementById('reserved');
  var serialEl = document.getElementById('serial');
  var modelEl = document.getElementById('model');
  var activeEl = document.getElementById('active');
  var espDeviceIdEl = document.getElementById('espDeviceId');
  var espMacEl = document.getElementById('espMac');
  var espChipModelEl = document.getElementById('espChipModel');
  var espFwVersionEl = document.getElementById('espFwVersion');

  var TIER_MODEL = {
    B: 'mBoard-basic',
    P: 'mBoard-pro',
    L: 'mBoard-lite'
  };

  function twoDigits(value) {
    var text = String(value || '').replace(/\D/g, '');
    if (!text) return '00';
    if (text.length === 1) return '0' + text;
    return text.slice(-2);
  }

  function oneCode(value) {
    var text = String(value || '').trim().toUpperCase().replace(/[^A-Z0-9]/g, '');
    return text ? text.charAt(0) : '';
  }

  function normalizeMac(value) {
    var raw = String(value || '').trim().toUpperCase().replace(/[^A-F0-9]/g, '');
    if (raw.length !== 12) return String(value || '').trim().toUpperCase();
    return raw.match(/.{1,2}/g).join(':');
  }

  function serialFromDeviceId(deviceId) {
    var text = String(deviceId || '').trim().toUpperCase().replace(/[^A-Z0-9]/g, '');
    if (!text) return '';
    var tail = text.slice(-4);
    var sum = 0;
    for (var i = 0; i < tail.length; i += 1) {
      sum += tail.charCodeAt(i);
    }
    return String(sum % 100).padStart(2, '0');
  }

  function updateModelByTier() {
    modelEl.value = TIER_MODEL[tierEl.value] || 'mBoard-custom';
  }

  function updateProductId() {
    var monthVal = monthEl.value || '';
    var yy = '00';
    var mm = '00';
    if (/^\d{4}-\d{2}$/.test(monthVal)) {
      yy = monthVal.slice(2, 4);
      mm = monthVal.slice(5, 7);
    }

    var pid = 'MB' +
      oneCode(tierEl.value) +
      oneCode(generationEl.value) +
      oneCode(originEl.value) +
      oneCode(gameEl.value) +
      oneCode(sensorEl.value) +
      oneCode(chipEl.value) +
      oneCode(swEl.value) +
      yy + mm +
      twoDigits(reservedEl.value) +
      twoDigits(serialEl.value);

    productIdEl.value = pid;
  }

  async function request(url, options) {
    try {
      var res = await fetch(url, options || {});
      var data = await res.json();
      return { ok: res.ok, status: res.status, data: data };
    } catch (err) {
      return { ok: false, status: 0, data: { ok: false, error: err.message || 'network-error' } };
    }
  }

  function setStatus(text, isError) {
    statusLine.textContent = text;
    statusLine.style.color = isError ? '#fca5a5' : '#9db0cf';
  }

  async function ensureAuth() {
    var resp = await request('/api/auth/me');
    if (!resp.ok || !resp.data || !resp.data.ok || !resp.data.authenticated) {
      window.location.href = '/pid-login.html';
      return false;
    }
    return true;
  }

  async function submitForm(ev) {
    ev.preventDefault();
    updateProductId();

    var payload = {
      tierCode: oneCode(tierEl.value),
      generation: oneCode(generationEl.value),
      originCode: oneCode(originEl.value),
      gameCode: oneCode(gameEl.value),
      sensorCode: oneCode(sensorEl.value),
      chipCode: oneCode(chipEl.value),
      swCode: oneCode(swEl.value),
      productionMonth: monthEl.value,
      reserved: twoDigits(reservedEl.value),
      serial: twoDigits(serialEl.value),
      model: modelEl.value.trim(),
      active: !!activeEl.checked,
      productId: productIdEl.value,
      efuseDeviceId: String(espDeviceIdEl.value || '').trim(),
      espDeviceId: String(espDeviceIdEl.value || '').trim(),
      espMac: String(espMacEl.value || '').trim(),
      chipModel: String(espChipModelEl.value || '').trim(),
      fwVersion: String(espFwVersionEl.value || '').trim()
    };

    setStatus('正在写入 products.csv 并生成二维码...', false);

    var resp = await request('/api/admin/products', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    if (!resp.ok || !resp.data.ok) {
      setStatus('录入失败：' + ((resp.data && resp.data.error) || ('http-' + resp.status)), true);
      return;
    }

    var data = resp.data;
    resultProductId.textContent = data.productId || '-';
    resultPath.textContent = data.accessPath || '-';
    resultUrl.textContent = data.accessUrl || '-';
    resultUrl.href = data.accessUrl || '#';
    resultWorkspace.textContent = data.workspace || '-';
    qrImage.src = data.qrUrl || '';
    resultCard.classList.add('show');

    setStatus('录入成功，已写入白名单并生成访问二维码。', false);
  }

  function copyProductId() {
    if (!productIdEl.value) return;
    navigator.clipboard.writeText(productIdEl.value).then(function () {
      setStatus('Product ID 已复制。', false);
    }).catch(function () {
      setStatus('复制失败，请手动复制。', true);
    });
  }

  async function logout(ev) {
    ev.preventDefault();
    await request('/api/auth/logout', { method: 'POST' });
    window.location.href = '/pid-login.html';
  }

  function initMonthDefault() {
    var now = new Date();
    var mm = String(now.getMonth() + 1).padStart(2, '0');
    monthEl.value = now.getFullYear() + '-' + mm;
  }

  function applyEspData(payload) {
    if (!payload || typeof payload !== 'object') return false;
    var changed = false;

    if (payload.deviceId) {
      espDeviceIdEl.value = String(payload.deviceId).trim().toUpperCase();
      changed = true;
    }
    if (payload.mac) {
      espMacEl.value = normalizeMac(payload.mac);
      changed = true;
    }
    if (payload.chipModel) {
      espChipModelEl.value = String(payload.chipModel).trim();
      changed = true;
    }
    if (payload.fwVersion) {
      espFwVersionEl.value = String(payload.fwVersion).trim();
      changed = true;
    }

    if (payload.deviceId) {
      serialEl.value = serialFromDeviceId(payload.deviceId) || serialEl.value;
      changed = true;
    }

    chipEl.value = 'E';
    updateProductId();
    return changed;
  }

  function parseEspLine(line) {
    var text = String(line || '').trim();
    if (!text) return null;

    try {
      var obj = JSON.parse(text);
      return {
        deviceId: obj.deviceId || obj.device_id || '',
        mac: obj.mac || obj.macAddress || '',
        chipModel: obj.chipModel || obj.chip || obj.chip_model || '',
        fwVersion: obj.fwVersion || obj.fw || obj.version || ''
      };
    } catch (_) {}

    var m;
    m = text.match(/device[\s_-]?id\s*[:=]\s*([A-Za-z0-9_-]+)/i);
    if (m) return { deviceId: m[1] };
    m = text.match(/\bmac\s*[:=]\s*([A-Fa-f0-9:.-]{12,20})/i);
    if (m) return { mac: m[1] };
    m = text.match(/\b(chip|model)\s*[:=]\s*([A-Za-z0-9_.-]+)/i);
    if (m) return { chipModel: m[2] };
    m = text.match(/\b(fw|firmware|version)\s*[:=]\s*([A-Za-z0-9_.-]+)/i);
    if (m) return { fwVersion: m[2] };
    return null;
  }

  async function readEspFromSerial() {
    if (!('serial' in navigator)) {
      setStatus('当前浏览器不支持 Web Serial，请用 Chrome/Edge 并在 HTTPS 或 localhost 下打开。', true);
      return;
    }

    var port = null;
    var reader = null;
    setStatus('请在弹窗中选择 ESP32 串口设备...', false);

    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });

      if (port.writable) {
        var writer = port.writable.getWriter();
        await writer.write(new TextEncoder().encode('INFO\n'));
        writer.releaseLock();
      }

      setStatus('已连接串口，正在读取设备信息（可按一次 ESP32 的 RST 键）...', false);
      reader = port.readable.getReader();

      var decoder = new TextDecoder();
      var buffer = '';
      var deadline = Date.now() + 12000;
      var foundAny = false;

      while (Date.now() < deadline) {
        var result = await reader.read();
        if (result.done) break;
        buffer += decoder.decode(result.value, { stream: true });
        var lines = buffer.split(/\r?\n/);
        buffer = lines.pop() || '';
        for (var i = 0; i < lines.length; i += 1) {
          var parsed = parseEspLine(lines[i]);
          if (!parsed) continue;
          if (applyEspData(parsed)) foundAny = true;
        }
      }

      if (foundAny) {
        setStatus('已读取 ESP32 信息并填充到表单。', false);
      } else {
        setStatus('未读到有效信息。请让固件串口输出 deviceId/mac/chipModel/fwVersion 后重试。', true);
      }
    } catch (err) {
      setStatus('读取 ESP32 失败：' + (err && err.message ? err.message : err), true);
    } finally {
      try {
        if (reader) reader.releaseLock();
      } catch (_) {}
      try {
        if (port) await port.close();
      } catch (_) {}
    }
  }

  function fillSerialFromEsp() {
    var serial = serialFromDeviceId(espDeviceIdEl.value);
    if (!serial) {
      setStatus('ESP 设备ID为空，无法推导流水号。', true);
      return;
    }
    serialEl.value = serial;
    updateProductId();
    setStatus('已用 ESP 设备ID 推导流水号：' + serial, false);
  }

  [tierEl, generationEl, originEl, gameEl, sensorEl, chipEl, swEl, monthEl, reservedEl, serialEl]
    .forEach(function (el) {
      el.addEventListener('input', updateProductId);
      el.addEventListener('change', updateProductId);
    });

  tierEl.addEventListener('change', function () {
    updateModelByTier();
    updateProductId();
  });

  copyBtn.addEventListener('click', copyProductId);
  if (readEspBtn) readEspBtn.addEventListener('click', readEspFromSerial);
  if (useEspSerialBtn) useEspSerialBtn.addEventListener('click', fillSerialFromEsp);
  if (logoutLink) logoutLink.addEventListener('click', logout);
  form.addEventListener('submit', submitForm);

  ensureAuth().then(function (ok) {
    if (!ok) return;
    initMonthDefault();
    updateModelByTier();
    updateProductId();
  });
})();
