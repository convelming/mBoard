(function () {
  var hostEl = document.getElementById('host');
  var espIdEl = document.getElementById('espId');
  var productIdEl = document.getElementById('productId');
  var rowsEl = document.getElementById('rows');
  var colsEl = document.getElementById('cols');
  var apiPathEl = document.getElementById('apiPath');
  var displayModeEl = document.getElementById('displayMode');
  var thresholdEl = document.getElementById('threshold');
  var intervalEl = document.getElementById('intervalMs');
  var statusLine = document.getElementById('statusLine');
  var gridEl = document.getElementById('hallGrid');
  var metaMode = document.getElementById('metaMode');
  var metaActive = document.getElementById('metaActive');
  var metaMax = document.getElementById('metaMax');

  var timer = null;
  var cellEls = [];
  var gridGapPx = 2;

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
      threshold: thresholdEl.value,
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
      if (cfg.threshold) thresholdEl.value = cfg.threshold;
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
    var rows = clamp(i(rowsEl.value, 10), 1, 64);
    var cols = clamp(i(colsEl.value, 11), 1, 64);
    var total = rows * cols;
    var threshold = parseNumeric(thresholdEl.value);
    var mode = displayModeEl.value;
    if (mode === 'auto') mode = sourceMode === 'binary' ? 'binary' : 'intensity';

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
        cell.style.background = isOn ? 'rgba(42, 212, 167, 0.85)' : 'rgba(20, 28, 44, 0.7)';
      } else {
        var t = clamp(v / maxVal, 0, 1);
        var hue = 220 - Math.round(220 * t);
        var alpha = 0.25 + t * 0.75;
        cell.style.background = 'hsla(' + hue + ', 90%, 55%, ' + alpha + ')';
      }
      cell.innerHTML = '<div>' + (v.toFixed ? v.toFixed(1) : v) + '</div>';
    }

    metaMode.textContent = mode;
    metaActive.textContent = String(active);
    metaMax.textContent = String((Math.round(maxVal * 100) / 100));
    fitGridToViewport();
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
    displayModeEl, thresholdEl, intervalEl
  ].forEach(function (el) {
    el.addEventListener('change', saveConfig);
  });

  loadConfig();
  buildGrid();
  window.addEventListener('resize', fitGridToViewport);
})();
