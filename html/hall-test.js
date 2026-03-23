(function () {
  var hostEl = document.getElementById('host');
  var espIdEl = document.getElementById('espId');
  var productIdEl = document.getElementById('productId');
  var rowsEl = document.getElementById('rows');
  var colsEl = document.getElementById('cols');
  var apiPathEl = document.getElementById('apiPath');
  var displayModeEl = document.getElementById('displayMode');
  var offColorEl = document.getElementById('offColor');
  var onColorEl = document.getElementById('onColor');
  var intervalEl = document.getElementById('intervalMs');
  var statusLine = document.getElementById('statusLine');
  var gridEl = document.getElementById('hallGrid');
  var metaMode = document.getElementById('metaMode');
  var metaActive = document.getElementById('metaActive');
  var metaMax = document.getElementById('metaMax');

  var timer = null;
  var cellEls = [];
  var gridGapPx = 2;
  var lastPayload = { values: [], mode: 'intensity' };

  function i(value, fallback) {
    var n = parseInt(value, 10);
    if (isNaN(n)) return fallback;
    return n;
  }

  function clamp(n, min, max) {
    return Math.max(min, Math.min(max, n));
  }

  function setStatus(text, isError) {
    statusLine.textContent = text;
    statusLine.style.color = isError ? '#ff9f9f' : '#9db0cf';
  }

  function saveConfig() {
    var cfg = {
      host: hostEl.value.trim(),
      espId: espIdEl.value.trim(),
      productId: productIdEl.value.trim(),
      rows: rowsEl.value,
      cols: colsEl.value,
      apiPath: apiPathEl.value.trim(),
      displayMode: displayModeEl.value,
      offColor: offColorEl.value,
      onColor: onColorEl.value,
      interval: intervalEl.value
    };
    localStorage.setItem('hall_test_cfg', JSON.stringify(cfg));
  }

  function loadConfig() {
    try {
      var cfg = JSON.parse(localStorage.getItem('hall_test_cfg') || '{}');
      if (cfg.host) hostEl.value = cfg.host;
      if (cfg.espId) espIdEl.value = cfg.espId;
      if (cfg.productId) productIdEl.value = cfg.productId;
      if (cfg.rows) rowsEl.value = cfg.rows;
      if (cfg.cols) colsEl.value = cfg.cols;
      if (cfg.apiPath) apiPathEl.value = cfg.apiPath;
      if (cfg.displayMode) displayModeEl.value = cfg.displayMode;
      if (cfg.offColor) offColorEl.value = cfg.offColor;
      if (cfg.onColor) onColorEl.value = cfg.onColor;
      if (cfg.interval) intervalEl.value = cfg.interval;
    } catch (_) {}
  }

  function buildGrid() {
    var rows = clamp(i(rowsEl.value, 10), 1, 64);
    var cols = clamp(i(colsEl.value, 11), 1, 64);
    rowsEl.value = String(rows);
    colsEl.value = String(cols);

    gridEl.innerHTML = '';
    gridEl.style.gridTemplateColumns = 'repeat(' + cols + ', 1fr)';
    cellEls = [];

    for (var r = 0; r < rows; r += 1) {
      for (var c = 0; c < cols; c += 1) {
        var cell = document.createElement('div');
        cell.className = 'cell';
        cell.textContent = r + ',' + c;
        gridEl.appendChild(cell);
        cellEls.push(cell);
      }
    }
    fitGridToViewport();
  }

  function fitGridToViewport() {
    if (!cellEls.length) return;
    var rows = clamp(i(rowsEl.value, 10), 1, 64);
    var cols = clamp(i(colsEl.value, 11), 1, 64);

    var rect = gridEl.getBoundingClientRect();
    if (!rect.width || !rect.height) return;

    var gap = gridGapPx;
    var maxCellW = (rect.width - (cols - 1) * gap) / cols;
    var maxCellH = (rect.height - (rows - 1) * gap) / rows;
    var cell = Math.floor(Math.max(8, Math.min(maxCellW, maxCellH)));

    // Apply fixed tracks so the full matrix always fits in available area.
    gridEl.style.gridTemplateColumns = 'repeat(' + cols + ', ' + cell + 'px)';
    gridEl.style.gridTemplateRows = 'repeat(' + rows + ', ' + cell + 'px)';
  }

  function parseNumeric(v) {
    if (typeof v === 'boolean') return v ? 1 : 0;
    var n = Number(v);
    return isNaN(n) ? 0 : n;
  }

  function toFlat(values, rows, cols) {
    var total = rows * cols;
    var out = new Array(total).fill(0);
    var r, c, idx;

    if (Array.isArray(values) && values.length > 0 && Array.isArray(values[0])) {
      for (r = 0; r < rows; r += 1) {
        for (c = 0; c < cols; c += 1) {
          idx = r * cols + c;
          if (values[r] && values[r][c] !== undefined) out[idx] = parseNumeric(values[r][c]);
        }
      }
      return out;
    }

    if (Array.isArray(values) && values.length > 0 && typeof values[0] === 'object') {
      for (var k = 0; k < values.length; k += 1) {
        var item = values[k] || {};
        var x = i(item.x, -1);
        var y = i(item.y, -1);
        if (x < 0 || y < 0 || x >= cols || y >= rows) continue;
        idx = y * cols + x;
        var vv = item.value;
        if (vv === undefined) vv = item.strength;
        if (vv === undefined) vv = item.binary;
        if (vv === undefined) vv = item.active;
        out[idx] = parseNumeric(vv);
      }
      return out;
    }

    if (Array.isArray(values)) {
      for (idx = 0; idx < total; idx += 1) {
        if (values[idx] !== undefined) out[idx] = parseNumeric(values[idx]);
      }
      return out;
    }
    return out;
  }

  function choosePayload(data) {
    if (!data || typeof data !== 'object') return { values: [], mode: 'unknown' };
    if (data.matrix) return { values: data.matrix, mode: data.mode || 'intensity' };
    if (data.values) return { values: data.values, mode: data.mode || 'intensity' };
    if (data.binary) return { values: data.binary, mode: 'binary' };
    if (data.cells) return { values: data.cells, mode: data.mode || 'intensity' };
    if (data.data) return choosePayload(data.data);
    return { values: [], mode: 'unknown' };
  }

  function render(values, sourceMode) {
    lastPayload = { values: values || [], mode: sourceMode || 'intensity' };
    var rows = clamp(i(rowsEl.value, 10), 1, 64);
    var cols = clamp(i(colsEl.value, 11), 1, 64);
    var total = rows * cols;
    var threshold = 0;
    var mode = displayModeEl.value;
    if (mode === 'auto') mode = sourceMode === 'binary' ? 'binary' : 'intensity';
    var offColor = offColorEl.value || '#1f2a40';
    var onColor = onColorEl.value || '#2ad4a7';

    var flat = toFlat(values, rows, cols);
    var maxVal = 0;
    var active = 0;
    for (var i2 = 0; i2 < total; i2 += 1) {
      if (flat[i2] > maxVal) maxVal = flat[i2];
      if (flat[i2] > threshold) active += 1;
    }
    if (maxVal <= 0) maxVal = 1;

    for (var idx = 0; idx < total; idx += 1) {
      var cell = cellEls[idx];
      if (!cell) continue;
      var v = flat[idx] || 0;
      var isOn = v > threshold;

      if (mode === 'binary') {
        cell.style.background = isOn ? onColor : offColor;
      } else {
        var t = clamp(v / maxVal, 0, 1);
        var c = blendHex(offColor, onColor, t);
        cell.style.background = c;
      }
      cell.innerHTML = '<div>' + (v.toFixed ? v.toFixed(1) : v) + '</div>';
    }

    metaMode.textContent = mode;
    metaActive.textContent = String(active);
    metaMax.textContent = String((Math.round(maxVal * 100) / 100));
    fitGridToViewport();
  }

  function hexToRgb(hex) {
    var s = String(hex || '').replace('#', '').trim();
    if (s.length === 3) s = s[0] + s[0] + s[1] + s[1] + s[2] + s[2];
    if (s.length !== 6) return { r: 31, g: 42, b: 64 };
    var n = parseInt(s, 16);
    if (isNaN(n)) return { r: 31, g: 42, b: 64 };
    return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
  }

  function rgbToHex(r, g, b) {
    function x(v) {
      var n = clamp(Math.round(v), 0, 255);
      var s = n.toString(16);
      return s.length === 1 ? '0' + s : s;
    }
    return '#' + x(r) + x(g) + x(b);
  }

  function blendHex(a, b, t) {
    var c1 = hexToRgb(a);
    var c2 = hexToRgb(b);
    var k = clamp(t, 0, 1);
    return rgbToHex(
      c1.r + (c2.r - c1.r) * k,
      c1.g + (c2.g - c1.g) * k,
      c1.b + (c2.b - c1.b) * k
    );
  }

  function normalizeHost(raw) {
    var text = String(raw || '').trim();
    if (!text) return '';
    if (/^https?:\/\//i.test(text)) return text.replace(/\/+$/, '');
    return 'http://' + text.replace(/\/+$/, '');
  }

  async function scanOnce() {
    saveConfig();
    if (!cellEls.length) buildGrid();

    var host = normalizeHost(hostEl.value);
    if (!host) {
      setStatus('请先输入 ESP32 IP/Host', true);
      return;
    }

    var rows = clamp(i(rowsEl.value, 10), 1, 64);
    var cols = clamp(i(colsEl.value, 11), 1, 64);
    var productId = productIdEl.value.trim();
    var path = (apiPathEl.value || '/api/boards/{productId}/hall-latest').trim();
    path = path.replace('{productId}', encodeURIComponent(productId));
    var url = host + path;

    try {
      var res = await fetch(url, { method: 'GET' });
      var data = await res.json();
      if (!res.ok || data.ok === false) {
        throw new Error(data.error || ('http-' + res.status));
      }
      var picked = choosePayload(data);
      render(picked.values, picked.mode);
      setStatus('后端拉取成功：' + new Date().toLocaleTimeString(), false);
    } catch (err) {
      setStatus('扫描失败：' + (err && err.message ? err.message : err), true);
    }
  }

  function startPolling() {
    stopPolling();
    var ms = clamp(i(intervalEl.value, 500), 100, 10000);
    intervalEl.value = String(ms);
    scanOnce();
    timer = setInterval(scanOnce, ms);
    setStatus('轮询中，每 ' + ms + ' ms 扫描一次。', false);
  }

  function stopPolling() {
    if (timer) {
      clearInterval(timer);
      timer = null;
      setStatus('已停止轮询。', false);
    }
  }

  document.getElementById('buildBtn').addEventListener('click', function () {
    saveConfig();
    buildGrid();
    setStatus('网格已生成。', false);
  });
  document.getElementById('scanOnceBtn').addEventListener('click', scanOnce);
  document.getElementById('startBtn').addEventListener('click', startPolling);
  document.getElementById('stopBtn').addEventListener('click', stopPolling);

  [
    hostEl, espIdEl, productIdEl, rowsEl, colsEl, apiPathEl,
    intervalEl
  ].forEach(function (el) {
    el.addEventListener('change', saveConfig);
  });

  [displayModeEl, offColorEl, onColorEl].forEach(function (el) {
    el.addEventListener('change', function () {
      saveConfig();
      render(lastPayload.values, lastPayload.mode);
    });
    el.addEventListener('input', function () {
      saveConfig();
      render(lastPayload.values, lastPayload.mode);
    });
  });

  loadConfig();
  buildGrid();
  window.addEventListener('resize', fitGridToViewport);
})();
