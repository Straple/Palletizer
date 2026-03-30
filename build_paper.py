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
"""

import subprocess
import sys
import os

PAPER_DIR = os.path.join(os.path.dirname(__file__), "papers_general")
SCRIPTS_DIR = os.path.join(os.path.dirname(__file__), "src", "scripts")
TEX_MAIN = "2025-3Dpallette-HSE"

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
    req = os.path.join(root, "requirements.txt")
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


def run(cmd, cwd=None):
    print(f"  → {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
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

    pdflatex_cmd = ["pdflatex", "-interaction=nonstopmode", "-halt-on-error", TEX_MAIN]
    bibtex_cmd = ["bibtex", TEX_MAIN]

    # pdflatex → bibtex → pdflatex → pdflatex
    steps = [
        ("pdflatex (1/3)", pdflatex_cmd),
        ("bibtex",         bibtex_cmd),
        ("pdflatex (2/3)", pdflatex_cmd),
        ("pdflatex (3/3)", pdflatex_cmd),
    ]

    for name, cmd in steps:
        print(f"\n[{name}]")
        if not run(cmd, cwd=PAPER_DIR):
            if "bibtex" in name:
                print("  (bibtex warning — продолжаем)")
                continue
            return False

    pdf_path = os.path.join(PAPER_DIR, f"{TEX_MAIN}.pdf")
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
