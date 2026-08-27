#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Ra2Overlay 启动器 (Starter)
===========================
功能：
  install   - 安装 Ra2Overlay.asi（及配置文件）到游戏目录
  uninstall - 从游戏目录卸载 Ra2Overlay（含日志）
  status    - 查看安装状态（是否已安装 / 版本哈希比对）
  launch    - 启动游戏
  menu      - 交互式菜单（默认）

用法：
  python starter.py            # 交互菜单
  python starter.py install
  python starter.py uninstall [-y]
  python starter.py status
  python starter.py launch

配置见同目录 starter_config.json，修改游戏目录 / 文件清单后无需改代码。
"""

import hashlib
import json
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# 配置加载
# ---------------------------------------------------------------------------

BASE_DIR = Path(__file__).resolve().parent

# Ra2Overlay.asi 已通过 asi_embedded.py 内嵌进程序（base64）。
# 源码运行时若 asi_embedded.py 存在则用之；否则回退读取编译产物文件。
try:
    import asi_embedded  # noqa: E402
    HAS_EMBED = True
except ImportError:
    HAS_EMBED = False


def get_asi_bytes() -> bytes:
    """获取 Ra2Overlay.asi 二进制内容。"""
    if HAS_EMBED:
        return asi_embedded.extract_asi()
    p = BASE_DIR / "GL-BaseHook/build/Win32/Release/Ra2Overlay.asi"
    if not p.exists():
        raise FileNotFoundError(f"未找到辅助文件: {p}（请先编译或确保 asi_embedded.py 存在）")
    return p.read_bytes()


def config_path() -> Path:
    """配置文件路径：打包后放在 exe 同目录（可编辑），源码运行在项目根。"""
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent / "starter_config.json"
    return BASE_DIR / "starter_config.json"

DEFAULT_CONFIG = {
    "game_dir": "D:/Ra2Game412",                    # 游戏安装目录
    "game_exe": "gamemd.exe",                       # launch 启动的程序
    "install_targets": [                            # 安装目标：相对游戏目录
        "scripts/Ra2Overlay.asi"
    ],
    "uninstall_files": [                            # 卸载清单：相对游戏目录
        "scripts/Ra2Overlay.asi",
        "scripts/Ra2Overlay.log"
    ]
}


def load_config():
    """读取配置文件，不存在则用默认值并生成。"""
    cfg_file = config_path()
    if cfg_file.exists():
        with open(cfg_file, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        # 合并默认值，保证新增字段兼容
        merged = dict(DEFAULT_CONFIG)
        merged.update(cfg)
        return merged
    with open(cfg_file, "w", encoding="utf-8") as f:
        json.dump(DEFAULT_CONFIG, f, ensure_ascii=False, indent=2)
    return dict(DEFAULT_CONFIG)


# ---------------------------------------------------------------------------
# 工具函数
# ---------------------------------------------------------------------------

def md5(path: Path) -> str:
    """计算文件 MD5，文件不存在返回空串。"""
    if not path.exists():
        return ""
    try:
        # PyInstaller 解压的资源带只读属性，先解除再读取
        os.chmod(path, os.stat(path).st_mode | stat.S_IREAD | stat.S_IWRITE)
    except OSError:
        pass
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def game_path(cfg, rel: str) -> Path:
    return Path(cfg["game_dir"]) / rel


# ---------------------------------------------------------------------------
# 子命令
# ---------------------------------------------------------------------------

def cmd_status(cfg):
    print("=" * 52)
    print("Ra2Overlay 安装状态")
    print("=" * 52)
    print(f"游戏目录 : {cfg['game_dir']}  ({'存在' if Path(cfg['game_dir']).exists() else '不存在!'})")
    print()

    # 内嵌辅助文件的二进制内容（作为版本基准）
    try:
        embed_bytes = get_asi_bytes()
        embed_md5 = hashlib.md5(embed_bytes).hexdigest()
    except (FileNotFoundError, ImportError) as e:
        print(f"[!] 无法获取内嵌辅助文件: {e}")
        return 1

    all_ok = True
    for dst_rel in cfg.get("install_targets", []):
        dst = game_path(cfg, dst_rel)
        if not dst.exists():
            print(f"[-] 未安装   : {dst_rel}")
            all_ok = False
            continue
        dst_md5 = md5(dst)
        if embed_md5 == dst_md5:
            print(f"[OK] 已安装   : {dst_rel}  (版本一致)")
        else:
            print(f"[!!] 版本不符: {dst_rel}  (游戏内与内嵌版本哈希不同，请重新安装)")
            all_ok = False

    print()
    if all_ok:
        print("结论: 全部安装且为最新版本。")
    else:
        print("结论: 尚未安装或版本不一致，请运行 install。")
    return 0 if all_ok else 1


def cmd_install(cfg):
    print("=" * 52)
    print("安装 Ra2Overlay")
    print("=" * 52)
    game_dir = Path(cfg["game_dir"])
    if not game_dir.exists():
        print(f"[错误] 游戏目录不存在: {game_dir}")
        return 1

    try:
        embed_bytes = get_asi_bytes()
    except (FileNotFoundError, ImportError) as e:
        print(f"[错误] 无法获取内嵌辅助文件: {e}")
        return 1

    for dst_rel in cfg.get("install_targets", []):
        dst = game_path(cfg, dst_rel)
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(embed_bytes)
        print(f"[安装] Ra2Overlay.asi (内嵌)")
        print(f"   -> {dst}  ({os.path.getsize(dst):,} 字节)")

    print()
    print("安装完成。启动游戏后按 Insert 打开菜单，按 End 卸载辅助。")
    return 0


def cmd_uninstall(cfg, assume_yes=False):
    print("=" * 52)
    print("卸载 Ra2Overlay")
    print("=" * 52)

    targets = [game_path(cfg, rel) for rel in cfg["uninstall_files"]]
    existing = [p for p in targets if p.exists()]

    if not existing:
        print("没有找到已安装的 Ra2Overlay 文件（无需卸载）。")
        return 0

    for p in existing:
        print(f"将删除: {p}")

    if not assume_yes:
        try:
            ans = input("确认卸载？这些文件将被删除 [y/N]: ").strip().lower()
        except EOFError:
            ans = ""
        if ans not in ("y", "yes"):
            print("已取消。")
            return 1

    for p in existing:
        try:
            if p.is_dir():
                shutil.rmtree(p)
            else:
                p.unlink()
            print(f"[卸载] 已删除: {p}")
        except OSError as e:
            print(f"[错误] 删除失败 {p}: {e}")
            return 1

    print()
    print("卸载完成。游戏恢复干净状态。")
    return 0


def cmd_launch(cfg):
    game_dir = Path(cfg["game_dir"])
    exe = game_dir / cfg["game_exe"]
    if not exe.exists():
        print(f"[错误] 找不到游戏程序: {exe}")
        return 1
    print(f"启动游戏: {exe}")
    subprocess.Popen([str(exe)], cwd=str(game_dir))
    return 0


# ---------------------------------------------------------------------------
# 交互菜单
# ---------------------------------------------------------------------------

def cmd_menu(cfg):
    while True:
        print()
        print("=" * 52)
        print("  Ra2Overlay 启动器")
        print("=" * 52)
        print("  1. 安装 (Install)")
        print("  2. 卸载 (Uninstall)")
        print("  3. 状态 (Status)")
        print("  4. 启动游戏 (Launch)")
        print("  0. 退出 (Exit)")
        print("=" * 52)
        choice = input("请选择: ").strip()

        if choice == "1":
            cmd_install(cfg)
        elif choice == "2":
            cmd_uninstall(cfg)
        elif choice == "3":
            cmd_status(cfg)
        elif choice == "4":
            cmd_launch(cfg)
        elif choice == "0":
            print("再见。")
            break
        else:
            print("无效选择，请重试。")
    return 0


# ---------------------------------------------------------------------------
# 入口
# ---------------------------------------------------------------------------

def main():
    cfg = load_config()
    args = sys.argv[1:]

    if not args:
        return cmd_menu(cfg)

    cmd = args[0].lower()
    if cmd in ("install", "i"):
        return cmd_install(cfg)
    if cmd in ("uninstall", "u"):
        return cmd_uninstall(cfg, assume_yes=("-y" in args))
    if cmd in ("status", "s"):
        return cmd_status(cfg)
    if cmd in ("launch", "l"):
        return cmd_launch(cfg)
    if cmd in ("menu", "m"):
        return cmd_menu(cfg)
    if cmd in ("help", "h", "--help", "-h"):
        print(__doc__)
        return 0

    print(f"未知命令: {cmd}")
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main())
