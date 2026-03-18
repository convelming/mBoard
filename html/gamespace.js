(function () {
  var boardIdEl = document.getElementById('boardIdLabel');
  var connectStatusEl = document.getElementById('connectStatus');
  var sessionCard = document.getElementById('sessionCard');
  var tabsCard = document.getElementById('tabsCard');
  var authFailCard = document.getElementById('authFailCard');
  var moveLogEl = document.getElementById('moveLog');
  var chessMetaPanel = document.getElementById('chessMetaPanel');
  var chessStatusEl = document.getElementById('chessStatus');
  var chessFenEl = document.getElementById('chessFen');
  var chessPgnEl = document.getElementById('chessPgn');
  var chessStateCanvas = document.getElementById('chessStateCanvas');
  var historyBodyEl = document.getElementById('historyBody');
  var boardTypeSelect = document.getElementById('boardTypeSelect');
  var sideSelect = document.getElementById('sideSelect');
  var joinBtn = document.getElementById('joinBtn');
  var resetBtn = document.getElementById('resetBtn');
  var simulateSensorBtn = document.getElementById('simulateSensorBtn');
  var refreshHistoryBtn = document.getElementById('refreshHistoryBtn');
  var clearLocalHistoryBtn = document.getElementById('clearLocalHistoryBtn');
  var configForm = document.getElementById('configForm');
  var homeBtn = document.getElementById('homeBtn');
  var configStatus = document.getElementById('configStatus');

  var query = new URLSearchParams(window.location.search);
  var boardId = query.get('productId') || query.get('boardId') || query.get('id') || 'MTABULA-DEMO-0001';

  var LOCAL_HISTORY_KEY = 'mtabula_move_history_v1';
  var LOCAL_CONFIG_PREFIX = 'mtabula_board_config_';

  var state = {
    boardId: boardId,
    verified: false,
    boardType: 'chess',
    side: 'white',
    joined: false,
    chess: new Chess(),
    chessUi: null,
    sensorCursor: 0,
    pollTimer: null,
    xq: {
      pieces: {},
      turn: 'red'
    }
  };

  var XIANGQI_INIT = {
    '0,0': { text: '车', side: 'black' },
    '1,0': { text: '马', side: 'black' },
    '2,0': { text: '象', side: 'black' },
    '3,0': { text: '士', side: 'black' },
    '4,0': { text: '将', side: 'black' },
    '5,0': { text: '士', side: 'black' },
    '6,0': { text: '象', side: 'black' },
    '7,0': { text: '马', side: 'black' },
    '8,0': { text: '车', side: 'black' },
    '1,2': { text: '炮', side: 'black' },
    '7,2': { text: '炮', side: 'black' },
    '0,3': { text: '卒', side: 'black' },
    '2,3': { text: '卒', side: 'black' },
    '4,3': { text: '卒', side: 'black' },
    '6,3': { text: '卒', side: 'black' },
    '8,3': { text: '卒', side: 'black' },

    '0,9': { text: '车', side: 'red' },
    '1,9': { text: '马', side: 'red' },
    '2,9': { text: '相', side: 'red' },
    '3,9': { text: '仕', side: 'red' },
    '4,9': { text: '帅', side: 'red' },
    '5,9': { text: '仕', side: 'red' },
    '6,9': { text: '相', side: 'red' },
    '7,9': { text: '马', side: 'red' },
    '8,9': { text: '车', side: 'red' },
    '1,7': { text: '炮', side: 'red' },
    '7,7': { text: '炮', side: 'red' },
    '0,6': { text: '兵', side: 'red' },
    '2,6': { text: '兵', side: 'red' },
    '4,6': { text: '兵', side: 'red' },
    '6,6': { text: '兵', side: 'red' },
    '8,6': { text: '兵', side: 'red' }
  };

  function callChessMethod(names, defaultValue) {
    for (var i = 0; i < names.length; i += 1) {
      var fn = state.chess[names[i]];
      if (typeof fn === 'function') {
        return fn.call(state.chess);
      }
    }
    return defaultValue;
  }

  function isGameOver() {
    return !!callChessMethod(['game_over', 'isGameOver'], false);
  }

  function isInCheckmate() {
    return !!callChessMethod(['in_checkmate', 'isCheckmate'], false);
  }

  function isInDraw() {
    return !!callChessMethod(['in_draw', 'isDraw'], false);
  }

  function isInCheck() {
    return !!callChessMethod(['in_check', 'isCheck'], false);
  }

  function currentTurnColor() {
    return state.chess.turn() === 'b' ? 'Black' : 'White';
  }

  function drawChessStateCanvas(statusText, fenText) {
    if (!chessStateCanvas) return;
    var ctx = chessStateCanvas.getContext('2d');
    var w = chessStateCanvas.width;
    var h = chessStateCanvas.height;

    ctx.clearRect(0, 0, w, h);

    var g = ctx.createLinearGradient(0, 0, w, 0);
    g.addColorStop(0, '#173156');
    g.addColorStop(0.5, '#2a1d3b');
    g.addColorStop(1, '#4e2c1d');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);

    ctx.strokeStyle = 'rgba(255,255,255,0.25)';
    ctx.strokeRect(1, 1, w - 2, h - 2);

    ctx.fillStyle = '#f6f8ff';
    ctx.font = '600 17px Space Grotesk, Noto Sans SC, sans-serif';
    ctx.fillText('Chess Status', 16, 28);

    ctx.font = '500 14px Space Grotesk, Noto Sans SC, sans-serif';
    ctx.fillStyle = '#cfe0ff';
    ctx.fillText(statusText, 16, 52);

    ctx.font = '500 11px Space Grotesk, Noto Sans SC, sans-serif';
    ctx.fillStyle = '#9dc0ef';
    var fenShort = String(fenText || '').slice(0, 105);
    ctx.fillText('FEN: ' + fenShort + (fenShort.length >= 105 ? '...' : ''), 16, 72);
  }

  function updateChessStatus() {
    var status = '';
    var moveColor = currentTurnColor();

    if (isInCheckmate()) {
      status = 'Game over, ' + moveColor + ' is in checkmate.';
    } else if (isInDraw()) {
      status = 'Game over, drawn position.';
    } else {
      status = moveColor + ' to move';
      if (isInCheck()) status += ', ' + moveColor + ' is in check.';
    }

    var fen = state.chess.fen();
    var pgn = state.chess.pgn();

    if (chessStatusEl) chessStatusEl.textContent = status;
    if (chessFenEl) chessFenEl.textContent = fen || '-';
    if (chessPgnEl) chessPgnEl.textContent = pgn || '(暂无)';
    drawChessStateCanvas(status, fen);
  }

  function nowIso() {
    return new Date().toISOString();
  }

  function setConnectionStatus(text, cls) {
    connectStatusEl.textContent = text;
    connectStatusEl.classList.remove('ok', 'warn');
    if (cls) connectStatusEl.classList.add(cls);
  }

  function setUnauthorizedView(enabled) {
    if (enabled) {
      if (sessionCard) sessionCard.classList.add('hidden');
      if (tabsCard) tabsCard.classList.add('hidden');
      if (authFailCard) authFailCard.classList.remove('hidden');
      return;
    }
    if (sessionCard) sessionCard.classList.remove('hidden');
    if (tabsCard) tabsCard.classList.remove('hidden');
    if (authFailCard) authFailCard.classList.add('hidden');
  }

  function setInteractionEnabled(enabled, silent) {
    joinBtn.disabled = !enabled;
    resetBtn.disabled = !enabled;
    simulateSensorBtn.disabled = !enabled;
    boardTypeSelect.disabled = !enabled;
    sideSelect.disabled = !enabled;
    refreshHistoryBtn.disabled = !enabled;
    clearLocalHistoryBtn.disabled = !enabled;
    homeBtn.disabled = !enabled;
    configForm.querySelectorAll('input,button').forEach(function (node) {
      node.disabled = !enabled;
    });

    if (!enabled) {
      state.joined = false;
    }

    if (!enabled && !silent) {
      addLog({
        ts: nowIso(),
        source: 'web',
        type: state.boardType,
        move: 'blocked',
        state: 'productId not verified'
      });
    }
  }

  function formatTime(iso) {
    try {
      return new Date(iso).toLocaleString();
    } catch (_e) {
      return iso;
    }
  }

  function getLocalHistory() {
    try {
      var text = localStorage.getItem(LOCAL_HISTORY_KEY);
      if (!text) return [];
      var rows = JSON.parse(text);
      return Array.isArray(rows) ? rows : [];
    } catch (_e) {
      return [];
    }
  }

  function saveLocalHistory(rows) {
    localStorage.setItem(LOCAL_HISTORY_KEY, JSON.stringify(rows));
  }

  function appendHistory(record) {
    var rows = getLocalHistory();
    rows.unshift(record);
    if (rows.length > 1200) rows = rows.slice(0, 1200);
    saveLocalHistory(rows);
  }

  function addLog(record) {
    var sourceText = record.source === 'sensor' ? '棋盘端' : '前端';
    var item = document.createElement('div');
    item.className = 'log-item';
    item.innerHTML =
      '<div><strong>' + sourceText + '</strong> · ' + (record.type || state.boardType) + ' · ' + (record.move || '-') + '</div>' +
      '<div class="meta">' + formatTime(record.ts || nowIso()) + '</div>' +
      '<div class="meta">' + (record.fen || record.state || 'no-state') + '</div>';
    moveLogEl.prepend(item);
  }

  function renderHistoryTable(list) {
    historyBodyEl.innerHTML = '';
    if (!list.length) {
      var tr = document.createElement('tr');
      tr.innerHTML = '<td colspan="6">暂无历史记录</td>';
      historyBodyEl.appendChild(tr);
      return;
    }

    list.forEach(function (row) {
      var tr = document.createElement('tr');
      tr.innerHTML =
        '<td>' + formatTime(row.ts) + '</td>' +
        '<td>' + (row.boardId || '-') + '</td>' +
        '<td>' + (row.type || '-') + '</td>' +
        '<td>' + (row.source || '-') + '</td>' +
        '<td>' + (row.move || '-') + '</td>' +
        '<td>' + (row.fen || row.state || '-') + '</td>';
      historyBodyEl.appendChild(tr);
    });
  }

  async function request(path, options) {
    try {
      var res = await fetch(path, options || {});
      var body = await res.json();
      if (!res.ok) {
        return { ok: false, error: body && body.error ? body.error : ('http-' + res.status) };
      }
      return body;
    } catch (err) {
      return { ok: false, error: err && err.message ? err.message : 'network-error' };
    }
  }

  var API = {
    verifyProduct: function () {
      return request('/api/scan/verify', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ productId: state.boardId })
      });
    },
    ping: function () {
      return request('/api/boards/' + encodeURIComponent(state.boardId) + '/status');
    },
    sendMove: function (payload) {
      return request('/api/boards/' + encodeURIComponent(state.boardId) + '/moves', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
    },
    history: function () {
      return request('/api/boards/history?boardId=' + encodeURIComponent(state.boardId));
    },
    sensorMoves: function () {
      return request('/api/boards/' + encodeURIComponent(state.boardId) + '/sensor-moves?after=' + encodeURIComponent(String(state.sensorCursor)));
    },
    saveConfig: function (payload) {
      return request('/api/boards/' + encodeURIComponent(state.boardId) + '/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
    },
    home: function () {
      return request('/api/boards/' + encodeURIComponent(state.boardId) + '/command/home', {
        method: 'POST'
      });
    }
  };

  function initSessionInfo() {
    boardIdEl.textContent = state.boardId;
  }

  function initTabs() {
    var tabs = Array.prototype.slice.call(document.querySelectorAll('.tab'));
    var panels = Array.prototype.slice.call(document.querySelectorAll('.tab-panel'));
    tabs.forEach(function (btn) {
      btn.addEventListener('click', function () {
        tabs.forEach(function (b) { b.classList.remove('active'); });
        panels.forEach(function (p) { p.classList.remove('active'); });
        btn.classList.add('active');
        var key = btn.getAttribute('data-tab');
        var panel = document.querySelector('[data-panel="' + key + '"]');
        if (panel) panel.classList.add('active');
      });
    });
  }

  function canMoveChessPiece(piece) {
    if (!state.verified) return false;
    if (!state.joined) return false;
    if (isGameOver()) return false;

    var color = piece.charAt(0) === 'w' ? 'white' : 'black';
    if (color !== state.side) return false;

    var turn = state.chess.turn() === 'w' ? 'white' : 'black';
    return turn === state.side;
  }

  function initChessUi() {
    state.chessUi = Chessboard('chessBoard', {
      draggable: true,
      position: 'start',
      orientation: state.side,
      pieceTheme: './img/chesspieces/wikipedia/{piece}.png',
      onDragStart: function (_source, piece, _position, orientation) {
        if (state.boardType !== 'chess') return false;
        if (!state.joined) return false;
        if (isGameOver()) return false;

        if ((orientation === 'white' && piece.search(/^w/) === -1) ||
          (orientation === 'black' && piece.search(/^b/) === -1)) {
          return false;
        }

        if ((state.chess.turn() === 'w' && piece.search(/^b/) !== -1) ||
          (state.chess.turn() === 'b' && piece.search(/^w/) !== -1)) {
          return false;
        }

        return canMoveChessPiece(piece);
      },
      onDrop: function (source, target) {
        var move = state.chess.move({
          from: source,
          to: target,
          promotion: 'q'
        });
        if (!move) return 'snapback';

        var record = {
          ts: nowIso(),
          boardId: state.boardId,
          type: 'chess',
          source: 'web',
          move: move.san,
          fen: state.chess.fen(),
          detail: { from: source, to: target }
        };
        appendHistory(record);
        addLog(record);
        renderHistory();
        API.sendMove(record);
        updateChessStatus();
      },
      onSnapEnd: function () {
        state.chessUi.position(state.chess.fen());
        updateChessStatus();
      }
    });
  }

  function resetChessBoard() {
    state.chess.reset();
    if (state.chessUi) {
      state.chessUi.orientation(state.side);
      state.chessUi.position('start');
    }
    updateChessStatus();
  }

  function createXqPieceEl(key, piece) {
    var div = document.createElement('div');
    div.className = 'xq-piece ' + piece.side;
    div.draggable = true;
    div.textContent = piece.text;
    div.setAttribute('data-key', key);

    div.addEventListener('dragstart', function (ev) {
      if (!state.joined || state.boardType !== 'xiangqi') {
        ev.preventDefault();
        return;
      }

      var myXqSide = state.side === 'white' ? 'red' : 'black';
      if (piece.side !== myXqSide || state.xq.turn !== myXqSide) {
        ev.preventDefault();
        return;
      }

      ev.dataTransfer.setData('text/plain', key);
      ev.dataTransfer.effectAllowed = 'move';
    });

    return div;
  }

  function renderXqBoard() {
    var el = document.getElementById('xiangqiBoard');
    el.innerHTML = '';
    for (var y = 0; y < 10; y += 1) {
      for (var x = 0; x < 9; x += 1) {
        var key = x + ',' + y;
        var cell = document.createElement('div');
        cell.className = 'xq-cell' + (y === 4 || y === 5 ? ' river' : '');
        cell.setAttribute('data-key', key);
        cell.addEventListener('dragover', function (ev) {
          ev.preventDefault();
          ev.currentTarget.classList.add('over');
        });
        cell.addEventListener('dragleave', function (ev) {
          ev.currentTarget.classList.remove('over');
        });
        cell.addEventListener('drop', function (ev) {
          ev.preventDefault();
          ev.currentTarget.classList.remove('over');
          var fromKey = ev.dataTransfer.getData('text/plain');
          var toKey = ev.currentTarget.getAttribute('data-key');
          if (!fromKey || !toKey || fromKey === toKey) return;
          moveXqPiece(fromKey, toKey, 'web');
        });

        var piece = state.xq.pieces[key];
        if (piece) {
          cell.appendChild(createXqPieceEl(key, piece));
        }
        el.appendChild(cell);
      }
    }
  }

  function moveXqPiece(fromKey, toKey, source) {
    if (!state.xq.pieces[fromKey]) return;
    var piece = state.xq.pieces[fromKey];

    if (source === 'web') {
      if (!state.verified) return;
      var myXqSide = state.side === 'white' ? 'red' : 'black';
      if (!state.joined || piece.side !== myXqSide || state.xq.turn !== myXqSide) return;
    }

    state.xq.pieces[toKey] = piece;
    delete state.xq.pieces[fromKey];
    state.xq.turn = state.xq.turn === 'red' ? 'black' : 'red';
    renderXqBoard();

    var record = {
      ts: nowIso(),
      boardId: state.boardId,
      type: 'xiangqi',
      source: source,
      move: fromKey + '->' + toKey,
      state: 'turn=' + state.xq.turn
    };
    appendHistory(record);
    addLog(record);
    renderHistory();

    if (source === 'web') {
      API.sendMove(record);
    }
  }

  function resetXiangqiBoard() {
    state.xq = {
      pieces: JSON.parse(JSON.stringify(XIANGQI_INIT)),
      turn: 'red'
    };
    renderXqBoard();
  }

  function switchBoardType(type) {
    state.boardType = type;
    var chessEl = document.getElementById('chessBoard');
    var xqEl = document.getElementById('xiangqiBoard');

    if (type === 'chess') {
      chessEl.classList.remove('hidden');
      xqEl.classList.add('hidden');
      if (chessMetaPanel) chessMetaPanel.classList.remove('hidden');
      if (state.chessUi) state.chessUi.orientation(state.side);
      updateChessStatus();
      return;
    }

    chessEl.classList.add('hidden');
    xqEl.classList.remove('hidden');
    if (chessMetaPanel) chessMetaPanel.classList.add('hidden');
  }

  async function initBackendStatus() {
    var verify = await API.verifyProduct();
    if (!verify.ok) {
      state.verified = false;
      setInteractionEnabled(false);
      setUnauthorizedView(true);
      setConnectionStatus('产品ID校验失败：' + (verify.error || '未在白名单CSV中'), 'warn');
      return;
    }

    state.verified = true;
    setUnauthorizedView(false);
    setInteractionEnabled(true);
    startSensorPolling();

    var ping = await API.ping();
    if (ping.ok) {
      setConnectionStatus('后端连接状态：已连接，产品ID已通过校验', 'ok');
    } else {
      setConnectionStatus('产品ID已校验，但后端状态接口不可用', 'warn');
    }
  }

  async function pollSensorMoves() {
    var data = await API.sensorMoves();
    if (!data.ok || !Array.isArray(data.moves)) return;

    data.moves.forEach(function (item) {
      state.sensorCursor = Math.max(state.sensorCursor, Number(item.id || 0));

      if (item.type === 'xiangqi' && item.from && item.to) {
        moveXqPiece(item.from, item.to, 'sensor');
        return;
      }

      if (item.type === 'chess' && item.from && item.to) {
        var move = state.chess.move({ from: item.from, to: item.to, promotion: 'q' });
        if (move) {
          if (state.chessUi) state.chessUi.position(state.chess.fen());
          var record = {
            ts: nowIso(),
            boardId: state.boardId,
            type: 'chess',
            source: 'sensor',
            move: move.san,
            fen: state.chess.fen()
          };
          appendHistory(record);
          addLog(record);
          renderHistory();
          updateChessStatus();
        }
      }
    });
  }

  function startSensorPolling() {
    if (state.pollTimer) clearInterval(state.pollTimer);
    state.pollTimer = setInterval(pollSensorMoves, 3500);
  }

  function simulateSensorMove() {
    if (!state.verified) return;
    if (state.boardType === 'xiangqi') {
      var keys = Object.keys(state.xq.pieces);
      if (!keys.length) return;
      var from = keys.find(function (k) { return state.xq.pieces[k].side === state.xq.turn; });
      if (!from) return;
      var x = Number(from.split(',')[0]);
      var y = Number(from.split(',')[1]);
      var candidates = [
        (x + 1) + ',' + y,
        (x - 1) + ',' + y,
        x + ',' + (y + (state.xq.turn === 'red' ? -1 : 1))
      ].filter(function (k) {
        var parts = k.split(',');
        var cx = Number(parts[0]);
        var cy = Number(parts[1]);
        return cx >= 0 && cx < 9 && cy >= 0 && cy < 10;
      });
      if (!candidates.length) return;
      moveXqPiece(from, candidates[0], 'sensor');
      return;
    }

    var moves = state.chess.moves({ verbose: true });
    if (!moves.length) return;

    var m = moves[Math.floor(Math.random() * moves.length)];
    var applied = state.chess.move({ from: m.from, to: m.to, promotion: 'q' });
    if (!applied) return;

    if (state.chessUi) state.chessUi.position(state.chess.fen());
    var record = {
      ts: nowIso(),
      boardId: state.boardId,
      type: 'chess',
      source: 'sensor',
      move: applied.san,
      fen: state.chess.fen()
    };
    appendHistory(record);
    addLog(record);
    renderHistory();
    updateChessStatus();
  }

  async function renderHistory() {
    var local = getLocalHistory();
    var rows = local.slice(0, 300);

    var remote = await API.history();
    if (remote.ok && Array.isArray(remote.rows) && remote.rows.length) {
      var merged = remote.rows.concat(rows);
      merged.sort(function (a, b) {
        return String(b.ts).localeCompare(String(a.ts));
      });
      rows = merged.slice(0, 300);
    }

    renderHistoryTable(rows);
  }

  function getLocalConfig() {
    try {
      var text = localStorage.getItem(LOCAL_CONFIG_PREFIX + state.boardId);
      if (!text) return null;
      return JSON.parse(text);
    } catch (_e) {
      return null;
    }
  }

  function saveLocalConfig(cfg) {
    localStorage.setItem(LOCAL_CONFIG_PREFIX + state.boardId, JSON.stringify(cfg));
  }

  function fillConfigForm() {
    var config = getLocalConfig() || {
      scaleX: 0.060,
      scaleY: 0.060,
      offsetX: 0,
      offsetY: 0,
      magnetLevel: 6
    };
    configForm.scaleX.value = config.scaleX;
    configForm.scaleY.value = config.scaleY;
    configForm.offsetX.value = config.offsetX;
    configForm.offsetY.value = config.offsetY;
    configForm.magnetLevel.value = config.magnetLevel;
  }

  function bindEvents() {
    boardTypeSelect.addEventListener('change', function () {
      switchBoardType(boardTypeSelect.value);
    });

    joinBtn.addEventListener('click', function () {
      if (!state.verified) return;
      state.joined = true;
      state.side = sideSelect.value;
      if (state.chessUi) state.chessUi.orientation(state.side);
      updateChessStatus();
      addLog({
        ts: nowIso(),
        source: 'web',
        type: state.boardType,
        move: 'join-' + state.side,
        state: 'joined'
      });
    });

    resetBtn.addEventListener('click', function () {
      if (state.boardType === 'chess') {
        resetChessBoard();
        return;
      }
      resetXiangqiBoard();
    });

    simulateSensorBtn.addEventListener('click', simulateSensorMove);

    refreshHistoryBtn.addEventListener('click', renderHistory);

    clearLocalHistoryBtn.addEventListener('click', function () {
      localStorage.removeItem(LOCAL_HISTORY_KEY);
      renderHistory();
    });

    configForm.addEventListener('submit', async function (ev) {
      ev.preventDefault();
      var payload = {
        scaleX: Number(configForm.scaleX.value),
        scaleY: Number(configForm.scaleY.value),
        offsetX: Number(configForm.offsetX.value),
        offsetY: Number(configForm.offsetY.value),
        magnetLevel: Number(configForm.magnetLevel.value),
        ts: nowIso()
      };

      saveLocalConfig(payload);
      var resp = await API.saveConfig(payload);
      if (resp.ok) {
        configStatus.textContent = '配置已保存到后端并写入本地缓存。';
        configStatus.classList.remove('warn');
        configStatus.classList.add('ok');
      } else {
        configStatus.textContent = '后端未连接，已保存到本地缓存。';
        configStatus.classList.remove('ok');
        configStatus.classList.add('warn');
      }
    });

    homeBtn.addEventListener('click', async function () {
      var resp = await API.home();
      if (resp.ok) {
        configStatus.textContent = '已发送回零指令到设备。';
        configStatus.classList.remove('warn');
        configStatus.classList.add('ok');
      } else {
        configStatus.textContent = '回零指令未发送成功（后端未连接）。';
        configStatus.classList.remove('ok');
        configStatus.classList.add('warn');
      }
    });
  }

  function boot() {
    initSessionInfo();
    setInteractionEnabled(false, true);
    initTabs();
    initChessUi();
    resetXiangqiBoard();
    switchBoardType('chess');
    bindEvents();
    fillConfigForm();
    renderHistory();
    initBackendStatus();
  }

  boot();
})();
