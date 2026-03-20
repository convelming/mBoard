const observer = new IntersectionObserver(
  entries => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('show');
      }
    });
  },
  { threshold: 0.15 }
);

document.querySelectorAll('.reveal').forEach(el => observer.observe(el));

// --------- 3D-ish background animation (wizard-chess vibe) ----------
const canvas = document.getElementById('bg3d');
const ctx = canvas.getContext('2d');
const DPR = Math.min(window.devicePixelRatio || 1, 2);

const state = {
  w: 0,
  h: 0,
  t: 0,
  boardSize: 8,
  boardScale: 1,
  phase: 0
};

const pieces = [
  { color: '#ff6b3d', i: 0, j: 0, progress: 0, speed: 0.22 },
  { color: '#00a896', i: 7, j: 7, progress: 0, speed: 0.18 },
  { color: '#ffd166', i: 0, j: 7, progress: 0, speed: 0.2 }
];

function resize() {
  state.w = window.innerWidth;
  state.h = window.innerHeight;
  canvas.width = Math.floor(state.w * DPR);
  canvas.height = Math.floor(state.h * DPR);
  canvas.style.width = `${state.w}px`;
  canvas.style.height = `${state.h}px`;
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  state.boardScale = Math.min(state.w, state.h) * 0.42;
}

function project(x, y, z) {
  // Manual perspective + tilt
  const cx = state.w * 0.5;
  const cy = state.h * 0.56;
  const tilt = 0.72;
  const depth = 720;
  const yy = y * tilt - z;
  const k = depth / (depth + y * 0.9 + 420);
  return {
    x: cx + x * k,
    y: cy + yy * k
  };
}

function boardToWorld(i, j) {
  const s = state.boardScale / state.boardSize;
  const x = (i - state.boardSize / 2 + 0.5) * s;
  const y = (j - state.boardSize / 2 + 0.5) * s;
  return { x, y, s };
}

function drawBoard() {
  const n = state.boardSize;
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      const p = boardToWorld(i, j);
      const d = p.s * 0.48;

      const a = project(p.x - d, p.y - d, 0);
      const b = project(p.x + d, p.y - d, 0);
      const c = project(p.x + d, p.y + d, 0);
      const d0 = project(p.x - d, p.y + d, 0);

      const alt = (i + j) % 2 === 0;
      const glow = 0.08 + 0.06 * Math.sin(state.t * 0.001 + (i + j) * 0.4);
      ctx.fillStyle = alt ? `rgba(18,26,42,${0.62 + glow})` : `rgba(241,227,196,${0.55 + glow})`;
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(b.x, b.y);
      ctx.lineTo(c.x, c.y);
      ctx.lineTo(d0.x, d0.y);
      ctx.closePath();
      ctx.fill();
    }
  }

  // Border
  const e0 = boardToWorld(0, 0);
  const e1 = boardToWorld(n - 1, 0);
  const e2 = boardToWorld(n - 1, n - 1);
  const e3 = boardToWorld(0, n - 1);
  const c0 = project(e0.x - e0.s * 0.5, e0.y - e0.s * 0.5, 0);
  const c1 = project(e1.x + e1.s * 0.5, e1.y - e1.s * 0.5, 0);
  const c2 = project(e2.x + e2.s * 0.5, e2.y + e2.s * 0.5, 0);
  const c3 = project(e3.x - e3.s * 0.5, e3.y + e3.s * 0.5, 0);
  ctx.strokeStyle = 'rgba(255,95,46,0.4)';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(c0.x, c0.y);
  ctx.lineTo(c1.x, c1.y);
  ctx.lineTo(c2.x, c2.y);
  ctx.lineTo(c3.x, c3.y);
  ctx.closePath();
  ctx.stroke();
}

function serpentineNext(i, j) {
  const n = state.boardSize;
  if (j % 2 === 0) {
    if (i < n - 1) return { i: i + 1, j };
    if (j < n - 1) return { i, j: j + 1 };
  } else {
    if (i > 0) return { i: i - 1, j };
    if (j < n - 1) return { i, j: j + 1 };
  }
  return { i: 0, j: 0 };
}

function drawPiece(piece, x, y, z, s) {
  const base = project(x, y, z);
  const top = project(x, y, z + s * 0.42);
  const r = Math.max(4, (base.y - top.y) * 0.62);

  // Glow
  const g = ctx.createRadialGradient(base.x, base.y, 1, base.x, base.y, r * 2.2);
  g.addColorStop(0, 'rgba(255,255,255,0.45)');
  g.addColorStop(1, 'rgba(255,255,255,0)');
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(base.x, base.y + r * 0.5, r * 2.2, 0, Math.PI * 2);
  ctx.fill();

  // Body
  ctx.fillStyle = piece.color;
  ctx.beginPath();
  ctx.ellipse(base.x, base.y, r * 0.95, r * 0.6, 0, 0, Math.PI * 2);
  ctx.fill();

  ctx.fillStyle = 'rgba(20,20,30,0.35)';
  ctx.beginPath();
  ctx.ellipse(top.x, top.y, r * 0.55, r * 0.38, 0, 0, Math.PI * 2);
  ctx.fill();
}

function updatePieces(dt) {
  for (const p of pieces) {
    p.progress += dt * p.speed * 0.001;
    if (p.progress >= 1) {
      p.progress = 0;
      const nxt = serpentineNext(p.i, p.j);
      p.i = nxt.i;
      p.j = nxt.j;
    }
  }
}

function drawPieces() {
  const list = [];
  for (const p of pieces) {
    const next = serpentineNext(p.i, p.j);
    const a = boardToWorld(p.i, p.j);
    const b = boardToWorld(next.i, next.j);
    const x = a.x + (b.x - a.x) * p.progress;
    const y = a.y + (b.y - a.y) * p.progress;
    const bob = Math.sin(state.t * 0.004 + (p.i + p.j) * 0.6) * a.s * 0.08 + a.s * 0.18;
    list.push({ p, x, y, z: bob, s: a.s });
  }
  // Painter's sort by y
  list.sort((u, v) => u.y - v.y);
  list.forEach(it => drawPiece(it.p, it.x, it.y, it.z, it.s));
}

let last = performance.now();
function frame(now) {
  const dt = now - last;
  last = now;
  state.t += dt;

  ctx.clearRect(0, 0, state.w, state.h);
  updatePieces(dt);
  drawBoard();
  drawPieces();
  requestAnimationFrame(frame);
}

resize();
window.addEventListener('resize', resize);
requestAnimationFrame(frame);
