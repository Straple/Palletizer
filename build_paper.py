#!/usr/bin/env python3
"""
Сборка курсовой работы: генерация графиков + компиляция LaTeX → PDF.

Использование:
    python3 build_paper.py          # графики + LaTeX
    python3 build_paper.py --plots  # только графики
    python3 build_paper.py --latex  # только LaTeX

Зависимости для графиков (pandas, matplotlib, numpy):
    python3 -m venv .venv
    .venv/bin/pip install -r requirements.txt

Скрипты в src/scripts/ лучше запускать тем же интерпретатором:
    .venv/bin/python src/scripts/plot_time_comparison.py

LaTeX: после установки MacTeX выполните в терминале:
    eval "$(/usr/libexec/path_helper)"
или перезапустите терминал.

Артефакты сборки (.aux, .log, .pdf, …) пишутся в papers_general/texbuild/
(каталог в .gitignore).
"""

import glob
import os
import shutil
import subprocess
import sys

PAPER_DIR = os.path.join(os.path.dirname(__file__), "papers_general")
SCRIPTS_DIR = os.path.join(os.path.dirname(__file__), "src", "scripts")
TEX_MAIN = "2025-3Dpallette-HSE"
# Относительно PAPER_DIR; см. .gitignore
TEX_BUILD_SUBDIR = "texbuild"

PLOT_SCRIPTS = [
    "plot_time_comparison.py",
    "plot_weights_comparison.py",
    "plot_algorithm_comparison.py",
    "plot_lns_vs_genetic.py",
    "plot_height_balance_sweep.py",
]


def get_plot_python(root):
    """Prefer project .venv so plot scripts find pandas/matplotlib."""
    if sys.platform == "win32":
        venv_py = os.path.join(root, ".venv", "Scripts", "python.exe")
    else:
        venv_py = os.path.join(root, ".venv", "bin", "python")
    if os.path.isfile(venv_py):
        return venv_py
    return sys.executable


def ensure_plot_deps(py, root):
    r = subprocess.run(
        [py, "-c", "import pandas, matplotlib, numpy"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    if r.returncode == 0:
        return True
    print(
        "Нет pandas/matplotlib в текущем Python.\n"
        "Создайте окружение и установите зависимости:\n"
        f"  cd {root}\n"
        "  python3 -m venv .venv\n"
        f"  .venv/bin/pip install -r requirements.txt\n"
        "После этого снова запустите build_paper.py или:\n"
        "  .venv/bin/python src/scripts/plot_time_comparison.py"
    )
    return False


def _tex_bin_dirs():
    """Каталоги с бинарниками TeX на macOS (часто не в PATH у IDE)."""
    dirs = []
    if sys.platform != "darwin":
        return dirs
    dirs.append("/Library/TeX/texbin")
    for pattern in (
        "/usr/local/texlive/*/bin/*",
        "/Library/TeX/texlive/*/bin/*",
    ):
        for d in sorted(glob.glob(pattern)):
            if os.path.isdir(d):
                dirs.append(d)
    for p in ("/opt/homebrew/opt/texlive/bin", "/usr/local/opt/texlive/bin"):
        if os.path.isdir(p):
            dirs.append(p)
    for pattern in ("/opt/homebrew/texlive/*/bin/*",):
        for d in sorted(glob.glob(pattern)):
            if os.path.isdir(d):
                dirs.append(d)
    for app in glob.glob("/Applications/TeX*.app/Contents/Resources/texbin"):
        if os.path.isdir(app):
            dirs.append(app)
    seen = set()
    out = []
    for d in dirs:
        if d not in seen and os.path.isdir(d):
            seen.add(d)
            out.append(d)
    return out


def _tex_search_path():
    extra = _tex_bin_dirs()
    return os.pathsep.join(extra + [os.environ.get("PATH", "")])


def _tex_executable_path(p):
    """
    Абсолютный путь к бинарнику TeX для subprocess.
    Нельзя использовать os.path.realpath: pdflatex — symlink на pdftex;
    realpath даёт …/pdftex, argv0 становится pdftex и грузится plain TeX,
    а не LaTeX (падает на \\documentclass).
    """
    return os.path.normpath(os.path.abspath(p))


def _pdflatex_from_login_shell():
    """Подхват PATH из login-интерактивного shell (~/.zshrc)."""
    for argv in (
        ["/bin/zsh", "-ilc", "command -v pdflatex"],
        ["/bin/bash", "-lc", "command -v pdflatex"],
    ):
        try:
            r = subprocess.run(
                argv,
                capture_output=True,
                text=True,
                timeout=20,
                stdin=subprocess.DEVNULL,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        if r.returncode != 0:
            continue
        out = (r.stdout or "").strip()
        if not out:
            continue
        p = out.splitlines()[-1].strip()
        if p and os.path.isfile(p) and os.access(p, os.X_OK):
            return _tex_executable_path(p)
    return None


def _pdflatex_from_find():
    """Поиск pdflatex в /Applications и /opt/homebrew."""
    if sys.platform != "darwin":
        return None
    roots = [p for p in ("/Applications", "/opt/homebrew") if os.path.isdir(p)]
    if not roots:
        return None
    try:
        r = subprocess.run(
            ["/usr/bin/find"] + roots + [
                "-maxdepth", "12",
                "-name", "pdflatex",
                "-type", "f",
            ],
            capture_output=True,
            text=True,
            timeout=90,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if r.returncode != 0 and not r.stdout:
        return None
    candidates = []
    for line in r.stdout.splitlines():
        p = line.strip()
        if not p or not os.access(p, os.X_OK):
            continue
        low = p.lower()
        if any(x in low for x in ("texlive", "texbin", "mactex", "tug.org", "/tex/")):
            candidates.append(p)
    if not candidates:
        for line in r.stdout.splitlines():
            p = line.strip()
            if p and os.access(p, os.X_OK):
                candidates.append(p)
                break
    if not candidates:
        return None
    pref = [p for p in candidates if "texlive" in p.lower()]
    pick = sorted(pref or candidates, key=len)[0]
    return _tex_executable_path(pick)


def find_pdflatex():
    """Абсолютный путь к pdflatex или None."""
    search_path = _tex_search_path()
    p = shutil.which("pdflatex", path=search_path)
    if p and os.path.isfile(p):
        return _tex_executable_path(p)
    if sys.platform == "darwin":
        for pattern in (
            "/Library/TeX/texbin/pdflatex",
            "/usr/local/texlive/*/bin/*/pdflatex",
            "/opt/homebrew/texlive/*/bin/*/pdflatex",
            "/Library/TeX/texlive/*/bin/*/pdflatex",
        ):
            for path in sorted(glob.glob(pattern)):
                if os.path.isfile(path) and os.access(path, os.X_OK):
                    return _tex_executable_path(path)
    p = shutil.which("pdflatex")
    if p and os.path.isfile(p):
        return _tex_executable_path(p)
    p = _pdflatex_from_login_shell()
    if p:
        return p
    return _pdflatex_from_find()


def find_bibtex(pdflatex_path):
    """bibtex рядом с pdflatex или в PATH."""
    if pdflatex_path:
        d = os.path.dirname(pdflatex_path)
        b = os.path.join(d, "bibtex")
        if os.path.isfile(b):
            return _tex_executable_path(b)
    search_path = _tex_search_path()
    p = shutil.which("bibtex", path=search_path)
    if p and os.path.isfile(p):
        return _tex_executable_path(p)
    p = shutil.which("bibtex")
    return _tex_executable_path(p) if p and os.path.isfile(p) else None


def print_tex_install_help():
    print(
        "\npdflatex не найден. Установите LaTeX.\n\n"
        "macOS (лёгкий вариант, ~100 МБ):\n"
        "  brew install --cask basictex\n"
        "  echo 'export PATH=\"/Library/TeX/texbin:$PATH\"' >> ~/.zshrc\n"
        "  source ~/.zshrc\n\n"
        "macOS (полный MacTeX):\n"
        "  brew install --cask mactex\n"
        "  # если brew говорит «установлен», а файлов нет — доустановите .pkg:\n"
        "  open \"$(brew --prefix)/Caskroom/mactex\"\n\n"
        "После установки:\n"
        "  eval \"$(/usr/libexec/path_helper)\"\n"
        "  python3 build_paper.py --latex\n"
    )


def run(cmd, cwd=None):
    print(f"  → {' '.join(cmd)}")
    try:
        # pdflatex может печатать байты не в UTF-8 (локаль, шрифты в .log)
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except FileNotFoundError as e:
        print(f"ОШИБКА: исполняемый файл не найден: {e.filename or cmd[0]}")
        return False
    if result.returncode != 0:
        print(f"ОШИБКА (код {result.returncode}):")
        print(result.stdout[-2000:] if len(result.stdout) > 2000 else result.stdout)
        print(result.stderr[-2000:] if len(result.stderr) > 2000 else result.stderr)
        return False
    return True


def generate_plots():
    print("=== Генерация графиков ===")
    root = os.path.dirname(os.path.abspath(__file__))
    py = get_plot_python(root)
    if not ensure_plot_deps(py, root):
        return False
    for script in PLOT_SCRIPTS:
        path = os.path.join(SCRIPTS_DIR, script)
        print(f"\n[{script}]")
        if not run([py, path], cwd=root):
            return False
    print("\nВсе графики сгенерированы.")
    return True


def compile_latex():
    print("\n=== Компиляция LaTeX ===")

    pdflatex = find_pdflatex()
    if not pdflatex:
        print_tex_install_help()
        return False

    build_path = os.path.join(PAPER_DIR, TEX_BUILD_SUBDIR)
    os.makedirs(build_path, exist_ok=True)

    bibtex = find_bibtex(pdflatex)
    if not bibtex:
        print("Предупреждение: bibtex не найден, шаг bibtex пропускается.\n")

    # Все выходы pdflatex/bibtex — в texbuild/, исходники .tex остаются в papers_general/
    out_dir_arg = f"-output-directory={TEX_BUILD_SUBDIR}"
    pdflatex_cmd = [
        pdflatex,
        out_dir_arg,
        "-interaction=nonstopmode",
        "-halt-on-error",
        TEX_MAIN,
    ]
    # .aux в texbuild/; jobname с путём для bibtex (TeX Live)
    bib_job = f"{TEX_BUILD_SUBDIR}/{TEX_MAIN}".replace("\\", "/")
    bibtex_cmd = [bibtex, bib_job] if bibtex else None

    steps = [
        ("pdflatex (1/3)", pdflatex_cmd),
        ("bibtex", bibtex_cmd),
        ("pdflatex (2/3)", pdflatex_cmd),
        ("pdflatex (3/3)", pdflatex_cmd),
    ]

    for name, cmd in steps:
        if cmd is None:
            continue
        print(f"\n[{name}]")
        if not run(cmd, cwd=PAPER_DIR):
            if "bibtex" in name:
                print("  (bibtex warning — продолжаем)")
                continue
            return False

    pdf_path = os.path.join(build_path, f"{TEX_MAIN}.pdf")
    if os.path.exists(pdf_path):
        size_mb = os.path.getsize(pdf_path) / (1024 * 1024)
        print(f"\nГотово: {pdf_path} ({size_mb:.1f} МБ)")
    else:
        print(f"\nОШИБКА: {pdf_path} не создан")
        return False
    return True


def main():
    args = set(sys.argv[1:])
    do_plots = "--plots" in args or not args
    do_latex = "--latex" in args or not args

    ok = True
    if do_plots:
        ok = generate_plots()
    if ok and do_latex:
        ok = compile_latex()

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
