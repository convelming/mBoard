(function () {
  var productIdEl = document.getElementById('productId');
  var rowsEl = document.getElementById('rows');
  var colsEl = document.getElementById('cols');
  var apiPathEl = document.getElementById('apiPath');
  var offColorEl = document.getElementById('offColor');
  var onColorEl = document.getElementById('onColor');
  var intervalEl = document.getElementById('intervalMs');
  var espHallIntervalEl = document.getElementById('espHallIntervalMs');
  var statusLine = document.getElementById('statusLine');
  var gridEl = document.getElementById('hallGrid');
  var metaMode = document.getElementById('metaMode');
  var metaActive = document.getElementById('metaActive');
  var metaMax = document.getElementById('metaMax');
  var payloadTextEl = document.getElementById('payloadText');

  var timer = null;
  var cellEls = [];
  var gridGapPx = 2;
  var lastPayload = { values: [], mode: 'binary', rows: 0, cols: 0, activeCount: null, maxValue: null, ts: '' };
  var DEFAULT_ROWS = 10;
  var DEFAULT_COLS = 11;
  var HALL_PUSH_MIN_MS = 100;
  var HALL_PUSH_MAX_MS = 60000;
  var HALL_PUSH_DEFAULT_MS = 5000;

  function toInt(value, fallback) {
    var n = parseInt(value, 10);
    return isNaN(n) ? fallback : n;
  }

  function clamp(n, min, max) {
    return Math.max(min, Math.min(max, n));
  }

  function setStatus(text, isError) {
    statusLine.textContent = text;
    statusLine.style.color = isError ? '#ff9f9f' : '#9db0cf';
  }

  function parseNumeric(v) {
    if (typeof v === 'boolean') return v ? 1 : 0;
    var n = Number(v);
    return isNaN(n) ? 0 : n;
  }

  function parseMaybeNumber(v) {
    if (v === null || v === undefined || v === '') return null;
    var n = Number(v);
    return isNaN(n) ? null : n;
  }

  function dataAgeSeconds(ts) {
    if (!ts) return null;
    var t = Date.parse(ts);
    if (isNaN(t)) return null;
    var age = Math.floor((Date.now() - t) / 1000);
    if (!isFinite(age)) return null;
    return age < 0 ? 0 : age;
  }

  function backendOrigin() {
    if (window.location && /^https?:\/\//i.test(window.location.origin || '')) {
      return window.location.origin.replace(/\/+$/, '');
    }
    return 'http://localhost:8866';
  }

  function normalizeHallPushMs(raw) {
    return clamp(toInt(raw, HALL_PUSH_DEFAULT_MS), HALL_PUSH_MIN_MS, HALL_PUSH_MAX_MS);
  }

  function getHallPushMsInput() {
    var ms = normalizeHallPushMs(espHallIntervalEl && espHallIntervalEl.value);
    if (espHallIntervalEl) espHallIntervalEl.value = String(ms);
    return ms;
  }

  function saveConfig() {
    var hallMs = getHallPushMsInput();
    var cfg = {
      productId: productIdEl.value.trim(),
      apiPath: apiPathEl.value.trim(),
      offColor: offColorEl.value,
      onColor: onColorEl.value,
      interval: intervalEl.value,
      hallIntervalMs: String(hallMs)
    };
    localStorage.setItem('hall_test_cfg', JSON.stringify(cfg));
  }

  function loadConfig() {
    try {
      var cfg = JSON.parse(localStorage.getItem('hall_test_cfg') || '{}');
      if (cfg.productId) productIdEl.value = cfg.productId;
      if (cfg.apiPath) apiPathEl.value = cfg.apiPath;
      if (cfg.offColor) offColorEl.value = cfg.offColor;
      if (cfg.onColor) onColorEl.value = cfg.onColor;
      if (cfg.interval) intervalEl.value = cfg.interval;
      if (espHallIntervalEl && cfg.hallIntervalMs) {
        espHallIntervalEl.value = String(normalizeHallPushMs(cfg.hallIntervalMs));
      }
    } catch (_) {}
    if (espHallIntervalEl && !espHallIntervalEl.value) {
      espHallIntervalEl.value = String(HALL_PUSH_DEFAULT_MS);
    }
  }

  function toFlat(values, rows, cols) {
    var total = rows * cols;
    var out = new Array(total).fill(0);
    var r;
    var c;
    var idx;

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
        var x = toInt(item.x, -1);
        var y = toInt(item.y, -1);
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

  function formatCellText(v) {
    var n = parseNumeric(v);
    var rounded = Math.round(n * 10) / 10;
    if (Math.abs(rounded - Math.round(rounded)) < 0.0001) return String(Math.round(rounded));
    return String(rounded);
  }

  function formatValuesMatrix(values, rows, cols) {
    if (rows <= 0 || cols <= 0) return '(empty)';
    var flat = toFlat(values, rows, cols);
    var width = 1;
    for (var i0 = 0; i0 < flat.length; i0 += 1) {
      var t = formatCellText(flat[i0]);
      if (t.length > width) width = t.length;
    }

    var lines = [];
    for (var r = 0; r < rows; r += 1) {
      var row = [];
      for (var c = 0; c < cols; c += 1) {
        var s = formatCellText(flat[r * cols + c]);
        while (s.length < width) s = ' ' + s;
        row.push(s);
      }
      lines.push(row.join(' '));
    }
    return lines.join('\n');
  }

  function inferDimsFromValues(values) {
    if (!Array.isArray(values) || values.length === 0) return { rows: 0, cols: 0 };
    if (Array.isArray(values[0])) {
      var maxCols = 0;
      for (var r = 0; r < values.length; r += 1) {
        var row = values[r];
        if (Array.isArray(row) && row.length > maxCols) maxCols = row.length;
      }
      return { rows: values.length, cols: maxCols };
    }
    return { rows: 0, cols: 0 };
  }

  function formatPayloadText(payload) {
    if (!payload || typeof payload !== 'object') return String(payload || '');

    var body = {};
    for (var key in payload) {
      if (!Object.prototype.hasOwnProperty.call(payload, key)) continue;
      if (key === 'values') continue;
      body[key] = payload[key];
    }

    var rows = toInt(payload.rows, 0);
    var cols = toInt(payload.cols, 0);
    if (rows <= 0 || cols <= 0) {
      var inferred = inferDimsFromValues(payload.values || []);
      rows = inferred.rows;
      cols = inferred.cols;
    }
    if (rows <= 0) rows = clamp(toInt(rowsEl.value, DEFAULT_ROWS), 1, 128);
    if (cols <= 0) cols = clamp(toInt(colsEl.value, DEFAULT_COLS), 1, 128);

    var text = JSON.stringify(body, null, 2);
    if (Array.isArray(payload.values)) {
      var rawFlat = toFlat(payload.values, rows, cols);
      var binary = new Array(rawFlat.length);
      for (var bi = 0; bi < rawFlat.length; bi += 1) {
        binary[bi] = rawFlat[bi] > 0 ? 1 : 0;
      }
      text += '\nvalues (' + rows + 'x' + cols + '):\n' + formatValuesMatrix(payload.values, rows, cols);
      text += '\n\nvalues_binary (1=active):\n' + formatValuesMatrix(binary, rows, cols);
    } else if (payload.values !== undefined) {
      text += '\nvalues:\n' + JSON.stringify(payload.values, null, 2);
    }
    return text;
  }

  function updatePayloadView(payload) {
    if (!payloadTextEl) return;
    try {
      payloadTextEl.textContent = formatPayloadText(payload || {});
    } catch (_) {
      payloadTextEl.textContent = String(payload || '');
    }
  }

  function buildGrid() {
    var rows = clamp(toInt(rowsEl.value, DEFAULT_ROWS), 1, 128);
    var cols = clamp(toInt(colsEl.value, DEFAULT_COLS), 1, 128);
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
    var rows = clamp(toInt(rowsEl.value, DEFAULT_ROWS), 1, 128);
    var cols = clamp(toInt(colsEl.value, DEFAULT_COLS), 1, 128);
    var rect = gridEl.getBoundingClientRect();
    if (!rect.width || !rect.height) return;

    var maxCellW = (rect.width - (cols - 1) * gridGapPx) / cols;
    var maxCellH = (rect.height - (rows - 1) * gridGapPx) / rows;
    var cell = Math.floor(Math.max(10, Math.min(maxCellW, maxCellH)));
    gridEl.style.gridTemplateColumns = 'repeat(' + cols + ', ' + cell + 'px)';
    gridEl.style.gridTemplateRows = 'repeat(' + rows + ', ' + cell + 'px)';
  }

  function choosePayload(data) {
    function p(v) {
      var n = parseInt(v, 10);
      return isNaN(n) || n <= 0 ? 0 : n;
    }
    function base(values, mode) {
      return {
        values: values || [],
        mode: mode || 'unknown',
        rows: p(data.rows),
        cols: p(data.cols),
        activeCount: parseMaybeNumber(data.activeCount),
        maxValue: parseMaybeNumber(data.maxValue),
        ts: String(data.ts || ''),
        productId: String(data.productId || '')
      };
    }

    if (!data || typeof data !== 'object') {
      return { values: [], mode: 'unknown', rows: 0, cols: 0, activeCount: null, maxValue: null, ts: '', productId: '' };
    }
    if (data.matrix) return base(data.matrix, data.mode || 'intensity');
    if (data.values !== undefined) return base(data.values, data.mode || 'intensity');
    if (data.binary) return base(data.binary, 'binary');
    if (data.cells) return base(data.cells, data.mode || 'intensity');
    if (data.data) {
      var nested = choosePayload(data.data);
      if (!nested.rows) nested.rows = p(data.rows);
      if (!nested.cols) nested.cols = p(data.cols);
      if (nested.mode === 'unknown' && data.mode) nested.mode = data.mode;
      if (nested.activeCount === null) nested.activeCount = parseMaybeNumber(data.activeCount);
      if (nested.maxValue === null) nested.maxValue = parseMaybeNumber(data.maxValue);
      if (!nested.ts) nested.ts = String(data.ts || '');
      if (!nested.productId) nested.productId = String(data.productId || '');
      return nested;
    }
    return base([], data.mode || 'unknown');
  }

  function syncGridSizeFromPayload(picked) {
    var rows = toInt(picked && picked.rows, 0);
    var cols = toInt(picked && picked.cols, 0);
    if (rows <= 0 || cols <= 0) {
      var inferred = inferDimsFromValues((picked && picked.values) || []);
      rows = inferred.rows;
      cols = inferred.cols;
    }
    if (rows <= 0 || cols <= 0) return;

    rows = clamp(rows, 1, 128);
    cols = clamp(cols, 1, 128);

    var prevRows = clamp(toInt(rowsEl.value, DEFAULT_ROWS), 1, 128);
    var prevCols = clamp(toInt(colsEl.value, DEFAULT_COLS), 1, 128);
    rowsEl.value = String(rows);
    colsEl.value = String(cols);
    if (!cellEls.length || rows !== prevRows || cols !== prevCols) buildGrid();
  }

  function render(picked) {
    picked = picked || {};
    var values = picked.values || [];
    var sourceMode = picked.mode || 'binary';
    lastPayload = {
      values: values,
      mode: sourceMode,
      rows: picked.rows || 0,
      cols: picked.cols || 0,
      activeCount: picked.activeCount,
      maxValue: picked.maxValue,
      ts: picked.ts || ''
    };

    var rows = clamp(toInt(rowsEl.value, DEFAULT_ROWS), 1, 128);
    var cols = clamp(toInt(colsEl.value, DEFAULT_COLS), 1, 128);
    var total = rows * cols;
    var threshold = 0;
    var offColor = offColorEl.value || '#1f2a40';
    var onColor = onColorEl.value || '#2ad4a7';

    var flat = toFlat(values, rows, cols);
    var maxValCalc = 0;
    var activeCalc = 0;
    for (var i1 = 0; i1 < total; i1 += 1) {
      if (flat[i1] > maxValCalc) maxValCalc = flat[i1];
      if (flat[i1] > threshold) activeCalc += 1;
    }
    if (maxValCalc <= 0) maxValCalc = 1;

    for (var idx = 0; idx < total; idx += 1) {
      var cell = cellEls[idx];
      if (!cell) continue;
      var v = flat[idx] || 0;
      var isOn = v > threshold;
      cell.style.background = isOn ? onColor : offColor;
      cell.innerHTML = '<div>' + (v.toFixed ? v.toFixed(1) : v) + '</div>';
    }

    var activeShown = parseMaybeNumber(picked.activeCount);
    if (activeShown === null) activeShown = activeCalc;
    var maxShown = parseMaybeNumber(picked.maxValue);
    if (maxShown === null) maxShown = maxValCalc;

    metaMode.textContent = sourceMode || 'binary';
    metaActive.textContent = String(activeShown);
    metaMax.textContent = String(Math.round(maxShown * 100) / 100);
    fitGridToViewport();
    return { activeCalc: activeCalc, maxCalc: maxValCalc };
  }

  function isAllZero(values, rows, cols) {
    var flat = toFlat(values, rows, cols);
    if (!flat.length) return true;
    for (var i2 = 0; i2 < flat.length; i2 += 1) {
      if (flat[i2] !== 0) return false;
    }
    return true;
  }

  async function fetchHallConfig() {
    var productId = productIdEl.value.trim();
    if (!productId) return;
    var url = backendOrigin() + '/api/boards/' + encodeURIComponent(productId) + '/hall-config?_t=' + Date.now();
    try {
      var res = await fetch(url, { method: 'GET', cache: 'no-store' });
      var data = await res.json();
      if (!res.ok || !data || data.ok === false) return;
      var ms = normalizeHallPushMs(data.hallIntervalMsText || data.hallIntervalMs);
      if (espHallIntervalEl) espHallIntervalEl.value = String(ms);
      saveConfig();
    } catch (_) {}
  }

  async function applyHallConfig() {
    var productId = productIdEl.value.trim();
    if (!productId) {
      setStatus('下发失败：请先填写 Product ID', true);
      return;
    }
    var ms = getHallPushMsInput();
    var url = backendOrigin() + '/api/boards/' + encodeURIComponent(productId) + '/hall-config';
    try {
      var res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hallIntervalMs: ms })
      });
      var data = await res.json();
      if (!res.ok || !data || data.ok === false) {
        throw new Error((data && data.error) || ('http-' + res.status));
      }
      if (espHallIntervalEl) {
        espHallIntervalEl.value = String(normalizeHallPushMs(data.hallIntervalMsText || data.hallIntervalMs || ms));
      }
      saveConfig();
      setStatus('已下发 ESP 上报间隔：' + espHallIntervalEl.value + ' ms（设备在线后自动拉取）', false);
    } catch (err) {
      setStatus('下发失败：' + (err && err.message ? err.message : err), true);
    }
  }

  async function scanOnce() {
    saveConfig();
    if (!cellEls.length) buildGrid();

    var productId = productIdEl.value.trim();
    var path = (apiPathEl.value || '/api/boards/{productId}/hall-latest').trim();
    path = path.replace('{productId}', encodeURIComponent(productId));
    var url = backendOrigin() + path;

    try {
      var reqUrl = url + (url.indexOf('?') >= 0 ? '&' : '?') + '_t=' + Date.now();
      var res = await fetch(reqUrl, { method: 'GET', cache: 'no-store' });
      var data = await res.json();
      updatePayloadView(data);
      if (!res.ok || data.ok === false) throw new Error(data.error || ('http-' + res.status));

      var picked = choosePayload(data);
      syncGridSizeFromPayload(picked);
      var stat = render(picked);
      var warn = '';
      var rr = clamp(toInt(rowsEl.value, DEFAULT_ROWS), 1, 128);
      var cc = clamp(toInt(colsEl.value, DEFAULT_COLS), 1, 128);

      if (Array.isArray(picked.values) && isAllZero(picked.values, rr, cc)) {
        warn += ' | 警告：values 全为 0（请检查 MCP 地址/霍尔极性/连线）';
      }
      var activeReported = parseMaybeNumber(picked.activeCount);
      if (activeReported !== null && activeReported !== stat.activeCalc) {
        warn += ' | 警告：activeCount 不一致（上报=' + activeReported + '，前端计算=' + stat.activeCalc + '）';
      }
      var age = dataAgeSeconds(picked.ts);
      if (age !== null) {
        warn += ' | 数据时效=' + age + 's';
        if (age > 15) warn += '（可能陈旧）';
      }

      try {
        var statusUrl = backendOrigin() + '/api/boards/' + encodeURIComponent(productId) + '/status?_t=' + Date.now();
        var sres = await fetch(statusUrl, { method: 'GET', cache: 'no-store' });
        if (sres.ok) {
          var sd = await sres.json();
          if (sd && sd.ok) {
            warn += ' | 板在线=' + (sd.online ? '1' : '0');
            if (sd.lastSeen) warn += ' lastSeen=' + sd.lastSeen;
          }
        }
      } catch (_) {}

      setStatus('扫描成功：' + rowsEl.value + 'x' + colsEl.value + ' | ' + new Date().toLocaleTimeString() + ' | ' + url + warn, false);
    } catch (err) {
      updatePayloadView({ ok: false, error: String(err && err.message ? err.message : err), url: url });
      setStatus('扫描失败：' + (err && err.message ? err.message : err) + ' | ' + url, true);
    }
  }

  function startPolling() {
    stopPolling();
    var ms = clamp(toInt(intervalEl.value, 500), 100, 10000);
    intervalEl.value = String(ms);
    scanOnce();
    timer = setInterval(scanOnce, ms);
    setStatus('轮询中：每 ' + ms + ' ms 扫描一次', false);
  }

  function stopPolling() {
    if (timer) {
      clearInterval(timer);
      timer = null;
      setStatus('轮询已停止', false);
    }
  }

  document.getElementById('buildBtn').addEventListener('click', function () {
    saveConfig();
    buildGrid();
    setStatus('网格已重建', false);
  });
  document.getElementById('scanOnceBtn').addEventListener('click', scanOnce);
  document.getElementById('startBtn').addEventListener('click', startPolling);
  document.getElementById('stopBtn').addEventListener('click', stopPolling);
  document.getElementById('applyEspIntervalBtn').addEventListener('click', applyHallConfig);

  [productIdEl, apiPathEl, intervalEl, espHallIntervalEl].forEach(function (el) {
    if (!el) return;
    el.addEventListener('change', saveConfig);
  });
  productIdEl.addEventListener('change', fetchHallConfig);

  [offColorEl, onColorEl].forEach(function (el) {
    el.addEventListener('change', function () {
      saveConfig();
      render(lastPayload);
    });
    el.addEventListener('input', function () {
      saveConfig();
      render(lastPayload);
    });
  });

  loadConfig();
  buildGrid();
  updatePayloadView({});
  fetchHallConfig();
  window.addEventListener('resize', fitGridToViewport);
})();
