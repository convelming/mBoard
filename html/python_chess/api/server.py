from __future__ import annotations

import asyncio
import json
import threading
from pathlib import Path
from typing import Dict, List, Optional

import torch
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from engine.alphazero_model import AlphaZeroNet
from engine.mcts import MCTS
from engine.rules import ChessRules
from engine.train import AlphaZeroTrainer, TrainConfig

"""FastAPI service for AlphaZero chess training and inference.

Primary endpoints:
- POST /api/train/start : start asynchronous self-play training loop
- GET  /api/train/status: poll training state and event history
- POST /api/play/move   : apply optional human move, then return AI move
- WS   /ws/train        : push training events in real time
"""

app = FastAPI(title="mTabula AlphaZero Chess API")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


class TrainStartReq(BaseModel):
    iters: int = 8
    games_per_iter: int = 3
    sims: int = 48
    epochs: int = 2
    batch_size: int = 64
    checkpoint_every_epochs: int = 10


class PlayReq(BaseModel):
    # Input position in FEN, plus optional human move in UCI.
    fen: str
    human_move_uci: Optional[str] = None
    sims: int = 64


class WSManager:
    def __init__(self):
        self.clients: List[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.clients.append(ws)

    def disconnect(self, ws: WebSocket):
        if ws in self.clients:
            self.clients.remove(ws)

    async def broadcast(self, payload: dict):
        dead = []
        msg = json.dumps(payload, ensure_ascii=False)
        for ws in self.clients:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.disconnect(ws)


ws_mgr = WSManager()
main_loop: Optional[asyncio.AbstractEventLoop] = None

train_state: Dict[str, object] = {
    "running": False,
    "last": None,
    "history": [],
}

MODEL = AlphaZeroNet()
MODEL.eval()


def maybe_load_latest(ckpt_dir: Path):
    cks = sorted(ckpt_dir.glob("az_iter_*.pt"))
    if not cks:
        return
    data = torch.load(cks[-1], map_location="cpu")
    if "model" in data:
        MODEL.load_state_dict(data["model"], strict=False)


def append_train_event(ev: dict):
    history = train_state["history"]
    history.append(ev)
    if len(history) > 300:
        del history[: len(history) - 300]
    train_state["last"] = ev


@app.on_event("startup")
async def on_startup():
    global main_loop
    main_loop = asyncio.get_running_loop()
    # Ensure API play endpoint uses the latest trained weights on service boot.
    maybe_load_latest(Path(__file__).resolve().parents[1] / "checkpoints")


@app.get("/api/health")
def health():
    return {
        "ok": True,
        "service": "python_chess_az",
        "running": train_state["running"],
        "history_len": len(train_state["history"]),
    }


@app.post("/api/train/start")
def start_train(req: TrainStartReq):
    if train_state["running"]:
        return {"ok": False, "error": "training already running"}

    train_state["running"] = True
    append_train_event({"event": "started"})

    def _emit(ev: dict):
        append_train_event(ev)
        if main_loop is not None:
            asyncio.run_coroutine_threadsafe(ws_mgr.broadcast(ev), main_loop)

    def _run():
        cfg = TrainConfig(
            device="cpu",
            iters=req.iters,
            games_per_iter=req.games_per_iter,
            sims=req.sims,
            epochs=req.epochs,
            batch_size=req.batch_size,
            out_dir=str(Path(__file__).resolve().parents[1] / "checkpoints"),
            checkpoint_every_epochs=max(1, req.checkpoint_every_epochs),
        )
        trainer = AlphaZeroTrainer(cfg)
        try:
            trainer.train_loop(on_log=_emit)
            maybe_load_latest(Path(cfg.out_dir))
            _emit({"event": "train_done"})
        except Exception as e:
            _emit({"event": "train_error", "error": str(e)})
        finally:
            train_state["running"] = False

    threading.Thread(target=_run, daemon=True).start()
    return {"ok": True}


@app.get("/api/train/status")
def train_status():
    return {
        "ok": True,
        "running": train_state["running"],
        "last": train_state["last"],
        "history": train_state["history"],
    }


@app.post("/api/play/move")
def play_move(req: PlayReq):
    rules = ChessRules(req.fen)

    if req.human_move_uci:
        if not rules.push_uci(req.human_move_uci):
            return {"ok": False, "error": "illegal human move"}

    if rules.is_terminal():
        return {"ok": True, "fen": rules.fen(), "ai_move": None, "result": rules.game_result()}

    mcts = MCTS(MODEL, simulations=req.sims)
    ai_move, _ = mcts.search(rules, temperature=1e-6)
    if ai_move is None:
        return {"ok": False, "error": "ai has no move"}

    uci = ai_move.uci()
    rules.board.push(ai_move)
    return {"ok": True, "ai_move": uci, "fen": rules.fen(), "result": rules.game_result()}


@app.websocket("/ws/train")
async def ws_train(ws: WebSocket):
    await ws_mgr.connect(ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        ws_mgr.disconnect(ws)


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("api.server:app", host="0.0.0.0", port=8899, reload=False)
