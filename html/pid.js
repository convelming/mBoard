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
      productId: productIdEl.value
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
  if (logoutLink) logoutLink.addEventListener('click', logout);
  form.addEventListener('submit', submitForm);

  ensureAuth().then(function (ok) {
    if (!ok) return;
    initMonthDefault();
    updateModelByTier();
    updateProductId();
  });
})();
