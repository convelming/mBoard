(function () {
  var form = document.getElementById('loginForm');
  var statusLine = document.getElementById('statusLine');

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

  async function checkMe() {
    var resp = await request('/api/auth/me');
    if (resp.ok && resp.data && resp.data.ok && resp.data.authenticated) {
      window.location.href = '/pid.html';
    }
  }

  async function onSubmit(ev) {
    ev.preventDefault();
    var fd = new FormData(form);
    var username = String(fd.get('username') || '').trim();
    var password = String(fd.get('password') || '');

    if (!username || !password) {
      setStatus('请输入用户名和密码。', true);
      return;
    }

    setStatus('登录中...', false);
    var resp = await request('/api/auth/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: username, password: password })
    });

    if (!resp.ok || !resp.data || !resp.data.ok) {
      setStatus('登录失败：' + ((resp.data && resp.data.error) || ('http-' + resp.status)), true);
      return;
    }

    setStatus('登录成功，正在跳转...', false);
    setTimeout(function () {
      window.location.href = '/pid.html';
    }, 250);
  }

  form.addEventListener('submit', onSubmit);
  checkMe();
})();
