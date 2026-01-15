from __future__ import annotations

import os
import json
import shutil
import zipfile
import threading
import queue
import urllib.request
import urllib.error
from dataclasses import dataclass
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox


# ========================= DEFAULT CONFIG =========================

DEFAULT_REPO_ZIP_URL = "https://github.com/therealpixeles/SDLite/archive/refs/heads/main.zip"

DEFAULT_SDL2_ZIP_URL = (
    "https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/"
    "SDL2-devel-2.32.10-mingw.zip"
)

DEFAULT_SDL2_IMAGE_ZIP_URL = (
    "https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.8/"
    "SDL2_image-devel-2.8.8-mingw.zip"
)

DEFAULT_INSTALL_SUBFOLDER = "SDLite"
DEFAULT_ROOT_MARKERS = ["include", "src", "res"]

DEFAULT_STRUCTURE = {
    "create_dirs": [
        "include",
        "src",
        "res",
        "external/SDL2",
        "external/SDL2_image",
        "bin/debug",
        "bin/release",
    ],
    "markers": {
        "SDL2": "external/SDL2/include/SDL2/SDL.h",
        "SDL2_image": "external/SDL2_image/include/SDL2/SDL_image.h",
    },
    "repo_root_markers": ["include", "src", "res"],
}

DEFAULT_CUSTOM_STRUCTURE_JSON = json.dumps(DEFAULT_STRUCTURE, indent=2)


# ========================= UTIL =========================

class InstallError(RuntimeError):
    pass


def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)


def delete_tree(p: Path) -> None:
    if not p.exists():
        return
    if p.is_file() or p.is_symlink():
        try:
            p.chmod(0o666)
        except Exception:
            pass
        try:
            p.unlink()
        except Exception:
            try:
                os.remove(str(p))
            except Exception:
                pass
        return
    shutil.rmtree(p, ignore_errors=True)


def move_tree(src: Path, dst: Path) -> None:
    """Move file/dir src into dst path, overwriting dst if needed."""
    if not src.exists():
        return

    if src.is_file():
        ensure_dir(dst.parent)
        if dst.exists():
            delete_tree(dst)
        try:
            src.replace(dst)
        except Exception:
            shutil.copy2(src, dst)
            delete_tree(src)
        return

    ensure_dir(dst)
    for item in src.iterdir():
        move_tree(item, dst / item.name)
    try:
        src.rmdir()
    except Exception:
        pass


def copy_tree(src: Path, dst: Path) -> None:
    """Copy file/dir src into dst path, overwriting dst if needed."""
    if not src.exists():
        return
    if src.is_file():
        ensure_dir(dst.parent)
        if dst.exists():
            delete_tree(dst)
        shutil.copy2(src, dst)
        return
    ensure_dir(dst)
    for item in src.iterdir():
        copy_tree(item, dst / item.name)


def count_children_one_level(dir_path: Path) -> tuple[int, int, str | None]:
    if not dir_path.exists() or not dir_path.is_dir():
        return (0, 0, None)
    d = 0
    f = 0
    only_dir = None
    for item in dir_path.iterdir():
        if item.is_dir():
            d += 1
            only_dir = item.name
        else:
            f += 1
    if d == 1 and f == 0:
        return (d, f, only_dir)
    return (d, f, None)


def flatten_single_dir_wrapper(dir_path: Path) -> bool:
    d, f, only = count_children_one_level(dir_path)
    if not (d == 1 and f == 0 and only):
        return False
    inner = dir_path / only
    if not inner.is_dir():
        return False
    for item in inner.iterdir():
        move_tree(item, dir_path / item.name)
    try:
        inner.rmdir()
    except Exception:
        pass
    return True


def looks_like_project_root(dir_path: Path, markers: list[str]) -> bool:
    hits = 0
    for m in markers:
        if (dir_path / m).is_dir():
            hits += 1
    return hits >= 2


def find_project_root_near(start_dir: Path, repo_markers: list[str]) -> Path:
    cur = start_dir

    # unwrap chain
    for _ in range(10):
        if looks_like_project_root(cur, repo_markers):
            return cur
        d, f, only = count_children_one_level(cur)
        if d == 1 and f == 0 and only:
            cur = cur / only
            continue
        break

    if looks_like_project_root(cur, repo_markers):
        return cur

    # one-level
    candidates: list[Path] = []
    for item in cur.iterdir():
        if item.is_dir():
            if looks_like_project_root(item, repo_markers):
                return item
            candidates.append(item)

    # two-level
    for c in candidates[:32]:
        try:
            for item in c.iterdir():
                if item.is_dir() and looks_like_project_root(item, repo_markers):
                    return item
        except Exception:
            pass

    # fallback unwrap a bit and return
    for _ in range(6):
        if not flatten_single_dir_wrapper(cur):
            break
    return cur


def extract_zip(zip_path: Path, dest_dir: Path) -> None:
    ensure_dir(dest_dir)
    try:
        with zipfile.ZipFile(zip_path, "r") as z:
            z.extractall(dest_dir)
    except zipfile.BadZipFile as e:
        raise InstallError(f"Bad ZIP file: {zip_path}\n{e}") from e


def ensure_structure(install_dir: Path, structure: dict) -> None:
    for d in structure.get("create_dirs", []):
        ensure_dir(install_dir / Path(d))


def parse_structure_json(text: str) -> dict:
    try:
        obj = json.loads(text)
    except Exception as e:
        raise InstallError(f"Custom structure JSON is invalid:\n{e}") from e

    if not isinstance(obj, dict):
        raise InstallError("Custom structure JSON must be an object/dict.")
    if "create_dirs" not in obj or "markers" not in obj:
        raise InstallError("Custom structure JSON must include 'create_dirs' and 'markers'.")
    if not isinstance(obj["create_dirs"], list) or not isinstance(obj["markers"], dict):
        raise InstallError("'create_dirs' must be a list and 'markers' must be an object/dict.")
    if "repo_root_markers" in obj and not isinstance(obj["repo_root_markers"], list):
        raise InstallError("'repo_root_markers' must be a list if present.")
    return obj


# ========================= SDL ZIP HANDLING (NEW) =========================

TOOLCHAIN_NAME = "x86_64-w64-mingw32"
TOOLCHAIN_PAYLOAD_DIRS = ("include", "lib", "bin")


def find_sdl_payload_root(extracted_root: Path, log) -> Path:
    """
    Given an extracted dir (already unwrapped), find the folder that contains:
      - either TOOLCHAIN_NAME/ (preferred), or
      - include/lib/bin directly
    Returns the directory that contains those.
    """
    toolchain = extracted_root / TOOLCHAIN_NAME
    if toolchain.is_dir():
        return extracted_root

    if all((extracted_root / d).exists() for d in TOOLCHAIN_PAYLOAD_DIRS):
        return extracted_root

    # try a small breadth search: one-level and two-level
    one = [p for p in extracted_root.iterdir() if p.is_dir()]
    for p in one:
        if (p / TOOLCHAIN_NAME).is_dir():
            return p
        if all((p / d).exists() for d in TOOLCHAIN_PAYLOAD_DIRS):
            return p

    for p in one[:32]:
        try:
            two = [q for q in p.iterdir() if q.is_dir()]
        except Exception:
            continue
        for q in two[:64]:
            if (q / TOOLCHAIN_NAME).is_dir():
                return q
            if all((q / d).exists() for d in TOOLCHAIN_PAYLOAD_DIRS):
                return q

    log("WARNING: Could not confidently detect SDL payload root; using extracted root as fallback.")
    return extracted_root


def stage_install_sdl_zip(
    tmp_extract_dir: Path,
    install_dir: Path,
    external_name: str,          # "SDL2" or "SDL2_image"
    log,
    prefer_copy: bool = False,   # usually False (move), but you can flip if you want
) -> None:
    """
    Install strategy:
      - Build external/<name>.__staging__/x86_64-w64-mingw32/(include,lib,bin)
      - Delete external/<name>
      - Rename staging -> external/<name>
    This prevents merging and preserves toolchain folder naming.
    """

    external_dir = install_dir / "external"
    ensure_dir(external_dir)

    dest_final = external_dir / external_name
    dest_stage = external_dir / f"{external_name}.__staging__"

    # Clean staging if leftover
    delete_tree(dest_stage)
    ensure_dir(dest_stage)

    # Unwrap wrappers at tmp root to get closer to real payload
    for _ in range(12):
        # If we already see either toolchain folder or include/lib/bin at root, stop unwrapping
        if (tmp_extract_dir / TOOLCHAIN_NAME).is_dir():
            break
        if all((tmp_extract_dir / d).exists() for d in TOOLCHAIN_PAYLOAD_DIRS):
            break
        if not flatten_single_dir_wrapper(tmp_extract_dir):
            break

    payload_root = find_sdl_payload_root(tmp_extract_dir, log=log)

    # Determine where include/lib/bin live
    toolchain_dir = payload_root / TOOLCHAIN_NAME
    if toolchain_dir.is_dir():
        src_payload = toolchain_dir
    else:
        src_payload = payload_root

    # Create toolchain folder under staging
    stage_toolchain = dest_stage / TOOLCHAIN_NAME
    ensure_dir(stage_toolchain)

    # Copy/move ONLY include/lib/bin into staging toolchain folder
    copied_any = False
    for dname in TOOLCHAIN_PAYLOAD_DIRS:
        src = src_payload / dname
        if src.exists():
            dst = stage_toolchain / dname
            log(f"Staging {external_name}: {dname}/ -> {dst}")
            if prefer_copy:
                copy_tree(src, dst)
            else:
                move_tree(src, dst)
            copied_any = True
        else:
            log(f"WARNING: {external_name} payload missing '{dname}/' at {src_payload}")

    if not copied_any:
        raise InstallError(
            f"{external_name} install failed: could not find any of include/lib/bin in extracted ZIP.\n"
            f"Looked in: {src_payload}"
        )

    # Now swap in staging atomically-ish (delete then rename)
    log(f"Replacing {dest_final} using staging folder...")
    delete_tree(dest_final)

    try:
        dest_stage.replace(dest_final)
    except Exception:
        # fallback: move contents then delete staging
        ensure_dir(dest_final)
        for item in dest_stage.iterdir():
            move_tree(item, dest_final / item.name)
        delete_tree(dest_stage)

    # sanity: ensure toolchain folder exists in final
    if not (dest_final / TOOLCHAIN_NAME).is_dir():
        log(f"WARNING: {external_name} final toolchain folder missing: {dest_final / TOOLCHAIN_NAME}")


# ========================= DOWNLOAD =========================

@dataclass
class DownloadProgress:
    label: str
    got: int
    total: int | None


class Downloader:
    def __init__(self, user_agent: str = "SDLiteSetup/2.1"):
        self.user_agent = user_agent

    def download(self, url: str, dst: Path, label: str, progress_cb) -> None:
        ensure_dir(dst.parent)

        req = urllib.request.Request(
            url,
            headers={"User-Agent": self.user_agent, "Accept": "*/*"},
            method="GET",
        )

        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                status = getattr(resp, "status", None)
                if status is not None and status != 200:
                    raise InstallError(f"HTTP {status} while downloading:\n{url}")

                total = resp.headers.get("Content-Length")
                total_int = int(total) if total and total.isdigit() else None

                tmp = dst.with_suffix(dst.suffix + ".part")
                if tmp.exists():
                    delete_tree(tmp)

                got = 0
                chunk = 64 * 1024

                with open(tmp, "wb") as f:
                    while True:
                        data = resp.read(chunk)
                        if not data:
                            break
                        f.write(data)
                        got += len(data)
                        progress_cb(DownloadProgress(label=label, got=got, total=total_int))

                if dst.exists():
                    delete_tree(dst)
                tmp.replace(dst)

        except urllib.error.HTTPError as e:
            raise InstallError(f"HTTP error while downloading:\n{url}\n{e}") from e
        except urllib.error.URLError as e:
            raise InstallError(f"Network error while downloading:\n{url}\n{e}") from e


# ========================= UI =========================

class OptionsDialog(tk.Toplevel):
    def __init__(self, master: "App"):
        super().__init__(master)
        self.title("Options")
        self.geometry("760x540")
        self.minsize(720, 500)
        self.master_app = master

        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True, padx=10, pady=10)

        self.tab_basic = ttk.Frame(nb)
        self.tab_structure = ttk.Frame(nb)

        nb.add(self.tab_basic, text="Basic")
        nb.add(self.tab_structure, text="Structure")

        self._build_basic()
        self._build_structure()

        btns = ttk.Frame(self)
        btns.pack(fill="x", padx=10, pady=(0, 10))

        ttk.Button(btns, text="Save", command=self.on_save).pack(side="right")
        ttk.Button(btns, text="Cancel", command=self.destroy).pack(side="right", padx=(0, 8))

        self.transient(master)
        self.grab_set()

    def _build_basic(self):
        f = self.tab_basic

        row = ttk.Frame(f)
        row.pack(fill="x", padx=10, pady=10)

        ttk.Label(row, text="Install subfolder name:").pack(side="left")
        ttk.Entry(row, textvariable=self.master_app.var_install_subfolder, width=24).pack(side="left", padx=10)

        row2 = ttk.Frame(f)
        row2.pack(fill="x", padx=10, pady=(0, 10))

        ttk.Checkbutton(row2, text="Keep downloads (.downloads)", variable=self.master_app.var_keep_downloads).pack(anchor="w")
        ttk.Checkbutton(row2, text="Keep temp folders (.tmp_*) for debugging", variable=self.master_app.var_keep_temp).pack(anchor="w")

        ttk.Separator(f).pack(fill="x", padx=10, pady=10)

        ttk.Label(f, text="Download URLs:").pack(anchor="w", padx=10)

        def url_row(label, var):
            r = ttk.Frame(f)
            r.pack(fill="x", padx=10, pady=6)
            ttk.Label(r, text=label, width=18).pack(side="left")
            e = ttk.Entry(r, textvariable=var)
            e.pack(side="left", fill="x", expand=True)

        url_row("Repo ZIP:", self.master_app.var_repo_url)
        url_row("SDL2 ZIP:", self.master_app.var_sdl2_url)
        url_row("SDL2_image ZIP:", self.master_app.var_img_url)

        ttk.Separator(f).pack(fill="x", padx=10, pady=10)
        ttk.Label(f, text="SDL install behavior:", font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=10, pady=(0, 6))
        ttk.Checkbutton(
            f,
            text="Prefer COPY instead of MOVE for SDL payload (slower, leaves temp intact)",
            variable=self.master_app.var_prefer_copy,
        ).pack(anchor="w", padx=10, pady=(0, 6))

    def _build_structure(self):
        f = self.tab_structure

        ttk.Label(
            f,
            text="Structure mode:",
            font=("Segoe UI", 10, "bold"),
        ).pack(anchor="w", padx=10, pady=(10, 6))

        mode_row = ttk.Frame(f)
        mode_row.pack(fill="x", padx=10, pady=(0, 8))

        ttk.Radiobutton(mode_row, text="Default structure", value="default", variable=self.master_app.var_structure_mode).pack(side="left")
        ttk.Radiobutton(mode_row, text="Custom JSON structure", value="custom", variable=self.master_app.var_structure_mode).pack(side="left", padx=16)

        ttk.Label(f, text="Custom JSON (only used if Custom is selected):").pack(anchor="w", padx=10)
        self.txt = tk.Text(f, height=18, wrap="none")
        self.txt.pack(fill="both", expand=True, padx=10, pady=(6, 10))
        self.txt.insert("1.0", self.master_app.var_custom_structure.get())

        tips = (
            "Tips:\n"
            "- create_dirs: list of folders to ensure exist\n"
            "- markers: files that must exist (used for validation warnings)\n"
            "- repo_root_markers: used to detect repo root (defaults to include/src/res)\n"
        )
        ttk.Label(f, text=tips, justify="left").pack(anchor="w", padx=10, pady=(0, 10))

    def on_save(self):
        self.master_app.var_custom_structure.set(self.txt.get("1.0", "end").strip())
        if self.master_app.var_structure_mode.get() == "custom":
            parse_structure_json(self.master_app.var_custom_structure.get())
        self.destroy()


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("SDLite Setup")
        self.geometry("900x560")
        self.minsize(780, 500)

        self.installing = False
        self.msg_q: queue.Queue[tuple[str, object]] = queue.Queue()

        # options vars
        self.var_install_subfolder = tk.StringVar(value=DEFAULT_INSTALL_SUBFOLDER)
        self.var_keep_downloads = tk.BooleanVar(value=False)
        self.var_keep_temp = tk.BooleanVar(value=False)
        self.var_prefer_copy = tk.BooleanVar(value=False)

        self.var_repo_url = tk.StringVar(value=DEFAULT_REPO_ZIP_URL)
        self.var_sdl2_url = tk.StringVar(value=DEFAULT_SDL2_ZIP_URL)
        self.var_img_url = tk.StringVar(value=DEFAULT_SDL2_IMAGE_ZIP_URL)

        self.var_structure_mode = tk.StringVar(value="default")
        self.var_custom_structure = tk.StringVar(value=DEFAULT_CUSTOM_STRUCTURE_JSON)

        self._build_menu()
        self._build_ui()
        self.after(50, self._pump_msgs)

    def _build_menu(self):
        m = tk.Menu(self)

        filem = tk.Menu(m, tearoff=False)
        filem.add_command(label="Install...", command=self.on_install)
        filem.add_separator()
        filem.add_command(label="Exit", command=self.on_exit)
        m.add_cascade(label="File", menu=filem)

        optm = tk.Menu(m, tearoff=False)
        optm.add_command(label="Options...", command=self.open_options)
        m.add_cascade(label="Options", menu=optm)

        helpm = tk.Menu(m, tearoff=False)
        helpm.add_command(label="About", command=self.about)
        m.add_cascade(label="Help", menu=helpm)

        self.config(menu=m)

    def _build_ui(self):
        pad = 12

        ttk.Label(self, text="SDLite Setup", font=("Segoe UI", 18, "bold")).pack(anchor="w", padx=pad, pady=(pad, 0))
        ttk.Label(
            self,
            text="Downloads SDLite + SDL2 + SDL2_image and lays out a ready-to-build folder tree.",
            font=("Segoe UI", 10),
        ).pack(anchor="w", padx=pad, pady=(4, 8))

        self.status_var = tk.StringVar(value="Ready.")
        ttk.Label(self, textvariable=self.status_var).pack(anchor="w", padx=pad)

        self.pb = ttk.Progressbar(self, mode="determinate", maximum=100)
        self.pb.pack(fill="x", padx=pad, pady=(8, 8))

        log_frame = ttk.Frame(self)
        log_frame.pack(fill="both", expand=True, padx=pad, pady=(0, pad))

        self.log = tk.Text(log_frame, height=18, wrap="word", state="disabled")
        self.log.pack(side="left", fill="both", expand=True)

        scroll = ttk.Scrollbar(log_frame, command=self.log.yview)
        scroll.pack(side="right", fill="y")
        self.log.configure(yscrollcommand=scroll.set)

        btns = ttk.Frame(self)
        btns.pack(fill="x", padx=pad, pady=(0, pad))

        self.btn_install = ttk.Button(btns, text="Install...", command=self.on_install)
        self.btn_install.pack(side="left")

        self.btn_options = ttk.Button(btns, text="Options...", command=self.open_options)
        self.btn_options.pack(side="left", padx=8)

        self.btn_exit = ttk.Button(btns, text="Exit", command=self.on_exit)
        self.btn_exit.pack(side="right")

        self.ui_log("SDLite Setup starting...")
        self.ui_log("This version installs SDL2 then SDL2_image using a staging rename to avoid merging.")

    def about(self):
        messagebox.showinfo(
            "About SDLite Setup",
            "SDLite Setup (Python)\n"
            "- Staging installs for SDL2/SDL2_image to avoid merge\n"
            "- Preserves x86_64-w64-mingw32 inside external deps\n"
            "- Tkinter UI with options + custom structure JSON\n",
        )

    def open_options(self):
        if self.installing:
            self.bell()
            return
        OptionsDialog(self)

    def ui_log(self, text: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def ui_status(self, text: str) -> None:
        self.status_var.set(text)

    def ui_progress_determinate(self, pct: int) -> None:
        self.pb.configure(mode="determinate")
        self.pb["value"] = max(0, min(100, pct))

    def ui_progress_marquee(self, on: bool) -> None:
        if on:
            self.pb.configure(mode="indeterminate")
            self.pb.start(12)
        else:
            self.pb.stop()
            self.pb.configure(mode="determinate")

    def _pump_msgs(self):
        try:
            while True:
                kind, payload = self.msg_q.get_nowait()
                if kind == "log":
                    self.ui_log(str(payload))
                elif kind == "status":
                    self.ui_status(str(payload))
                elif kind == "pct":
                    self.ui_progress_determinate(int(payload))
                elif kind == "marquee":
                    self.ui_progress_marquee(bool(payload))
                elif kind == "done":
                    self._finish_install(success=True, details=str(payload))
                elif kind == "fail":
                    self._finish_install(success=False, details=str(payload))
        except queue.Empty:
            pass
        self.after(50, self._pump_msgs)

    def on_exit(self):
        if self.installing:
            self.bell()
            return
        self.destroy()

    def on_install(self):
        if self.installing:
            return

        chosen = filedialog.askdirectory(title="Choose install location for SDLite")
        if not chosen:
            self.ui_log("Cancelled by user.")
            return

        sub = self.var_install_subfolder.get().strip() or DEFAULT_INSTALL_SUBFOLDER
        install_dir = Path(chosen) / sub

        self.installing = True
        self.btn_install.configure(state="disabled")
        self.btn_options.configure(state="disabled")
        self.btn_exit.configure(state="disabled")

        t = threading.Thread(target=self._install_thread, args=(install_dir,), daemon=True)
        t.start()

    def _finish_install(self, success: bool, details: str):
        self.installing = False
        self.btn_install.configure(state="normal")
        self.btn_options.configure(state="normal")
        self.btn_exit.configure(state="normal")
        self.ui_progress_marquee(False)
        self.ui_progress_determinate(100 if success else 0)

        if success:
            messagebox.showinfo("SDLite Setup", f"SDLite setup completed.\n\n{details}")
            self.ui_status("Done! You can close this window.")
        else:
            messagebox.showerror("SDLite Setup", details)
            self.ui_status("Failed.")

    def _post(self, kind: str, payload):
        self.msg_q.put((kind, payload))

    def _install_thread(self, install_dir: Path):
        def log(s: str): self._post("log", s)
        def status(s: str): self._post("status", s)
        def pct(n: int): self._post("pct", n)
        def marquee(on: bool): self._post("marquee", on)

        try:
            # structure selection
            if self.var_structure_mode.get() == "custom":
                structure = parse_structure_json(self.var_custom_structure.get())
            else:
                structure = DEFAULT_STRUCTURE

            repo_markers = structure.get("repo_root_markers", DEFAULT_ROOT_MARKERS)

            repo_url = self.var_repo_url.get().strip()
            sdl2_url = self.var_sdl2_url.get().strip()
            img_url = self.var_img_url.get().strip()

            if not repo_url or not sdl2_url or not img_url:
                raise InstallError("Missing one or more download URLs (check Options...).")

            status("Preparing...")
            log(f"Install directory: {install_dir}")
            ensure_dir(install_dir)

            # temp dirs
            dl_dir = install_dir / ".downloads"
            tmp_repo = install_dir / ".tmp_repo"
            tmp_sdl = install_dir / ".tmp_sdl2"
            tmp_img = install_dir / ".tmp_sdl2_image"

            for d in (dl_dir, tmp_repo, tmp_sdl, tmp_img):
                delete_tree(d)
                ensure_dir(d)

            repo_zip = dl_dir / "repo.zip"
            sdl_zip = dl_dir / "sdl2.zip"
            img_zip = dl_dir / "sdl2_image.zip"

            status("Downloading files...")
            pct(0)

            dl = Downloader()

            def progress_cb(dp: DownloadProgress):
                if dp.total and dp.total > 0:
                    marquee(False)
                    percent = int((dp.got * 100) / dp.total)
                    status(f"{dp.label} ({percent}%)")
                    pct(percent)
                else:
                    marquee(True)
                    status(f"{dp.label} (downloading...)")

            log(f"Downloading: {repo_url}")
            dl.download(repo_url, repo_zip, "Downloading SDLite (repo)...", progress_cb)
            marquee(False)
            pct(10)

            log(f"Downloading: {sdl2_url}")
            dl.download(sdl2_url, sdl_zip, "Downloading SDL2...", progress_cb)
            marquee(False)
            pct(20)

            log(f"Downloading: {img_url}")
            dl.download(img_url, img_zip, "Downloading SDL2_image...", progress_cb)
            marquee(False)
            pct(30)

            # extract repo
            status("Extracting SDLite repo...")
            pct(35)
            log(f"Extracting repo ZIP -> {tmp_repo}")
            extract_zip(repo_zip, tmp_repo)

            repo_root = find_project_root_near(tmp_repo, repo_markers)
            log(f"Repo root selected: {repo_root}")
            pct(42)

            status("Applying project layout...")
            pct(45)

            for item in repo_root.iterdir():
                if item.name in {".downloads", ".tmp_repo", ".tmp_sdl2", ".tmp_sdl2_image"}:
                    continue
                move_tree(item, install_dir / item.name)

            log("Repo files copied into install directory.")
            ensure_structure(install_dir, structure)
            pct(55)

            # ---------------- SDL2 FIRST ----------------
            status("Extracting SDL2...")
            pct(60)
            log(f"Extracting SDL2 ZIP -> {tmp_sdl}")
            extract_zip(sdl_zip, tmp_sdl)

            status("Installing SDL2 (staging)...")
            pct(68)
            stage_install_sdl_zip(
                tmp_extract_dir=tmp_sdl,
                install_dir=install_dir,
                external_name="SDL2",
                log=log,
                prefer_copy=self.var_prefer_copy.get(),
            )
            pct(78)

            # ---------------- SDL2_image SECOND ----------------
            status("Extracting SDL2_image...")
            pct(82)
            log(f"Extracting SDL2_image ZIP -> {tmp_img}")
            extract_zip(img_zip, tmp_img)

            status("Installing SDL2_image (staging)...")
            pct(88)
            stage_install_sdl_zip(
                tmp_extract_dir=tmp_img,
                install_dir=install_dir,
                external_name="SDL2_image",
                log=log,
                prefer_copy=self.var_prefer_copy.get(),
            )
            pct(94)

            ensure_structure(install_dir, structure)

            # cleanup
            status("Cleaning up...")
            pct(96)

            if not self.var_keep_temp.get():
                delete_tree(tmp_repo)
                delete_tree(tmp_sdl)
                delete_tree(tmp_img)
            else:
                log("Keeping temp folders (.tmp_*) for debugging (Options enabled).")

            if not self.var_keep_downloads.get():
                delete_tree(dl_dir)
            else:
                log("Keeping downloads (.downloads) (Options enabled).")

            # validate
            status("Validating install...")
            pct(100)

            markers = structure.get("markers", {})
            for k, rel in markers.items():
                p = install_dir / Path(rel)
                log(("OK: " if p.is_file() else "WARNING: ") + f"{k} marker -> {rel}")

            # also log the toolchain folder presence (what you care about)
            for dep in ("SDL2", "SDL2_image"):
                tc = install_dir / "external" / dep / TOOLCHAIN_NAME
                log(("OK: " if tc.is_dir() else "WARNING: ") + f"{dep} toolchain folder -> {tc}")

            self._post("done", f"Install path:\n{install_dir}")

        except Exception as e:
            self._post("fail", str(e))


def main():
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
