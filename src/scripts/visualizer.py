# example to use:
# python3 src/scripts/visualizer.py answers/261.csv --pallet-length 1200 --pallet-width 800 --out boxes.html --color-by sku --initial-count 100000000 --opacity 0.4 --edges single
import argparse
import json
import sys
import webbrowser
from pathlib import Path
from typing import Dict, Optional, List, Tuple, Any

import numpy as np
import pandas as pd
import plotly.graph_objects as go
import plotly.io as pio
from plotly.colors import qualitative as qcolors


def get_palette() -> List[str]:
    names = [
        "Plotly", "D3", "Dark24", "Light24",
        "Set3", "Bold", "Antique", "Pastel",
        "Prism", "Alphabet", "G10", "T10"
    ]
    pool: List[str] = []
    for n in names:
        if hasattr(qcolors, n):
            pool += getattr(qcolors, n)
    if not pool:
        pool = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"]
    return pool


def read_boxes(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path, dtype={"SKU": str})
    required = ["SKU", "x", "y", "z", "X", "Y", "Z"]
    missing = [c for c in required if c not in df.columns]
    if missing:
        raise ValueError(f"В CSV нет колонок: {missing}")
    for c in ["x", "y", "z", "X", "Y", "Z"]:
        df[c] = pd.to_numeric(df[c], errors="coerce")
    if df[["x", "y", "z", "X", "Y", "Z"]].isnull().any().any():
        bad_rows = df[df[["x", "y", "z", "X", "Y", "Z"]].isnull().any(axis=1)]
        raise ValueError(f"Есть некорректные числовые значения в координатах (строки: {bad_rows.index.tolist()[:5]} ...)")
    # Упорядочим min/max по осям
    for a, A in (("x", "X"), ("y", "Y"), ("z", "Z")):
        mn = df[[a, A]].min(axis=1)
        mx = df[[a, A]].max(axis=1)
        df[a], df[A] = mn, mx
    return df


def build_color_map(df: pd.DataFrame, color_by: str) -> Dict[str, str]:
    palette = get_palette()
    skus = pd.unique(df["SKU"])
    if color_by == "sku":
        return {sku: palette[i % len(palette)] for i, sku in enumerate(skus)}
    # один цвет для всех коробок
    return {sku: "#1f77b4" for sku in skus}


def box_vertices(row, offset: float = 0.0) -> List[Tuple[float, float, float]]:
    """
    Возвращает 8 вершин коробки.
    offset > 0 расширяет коробку наружу (для рёбер, чтобы избежать z-fighting).
    """
    x, y, z, X, Y, Z = row["x"], row["y"], row["z"], row["X"], row["Y"], row["Z"]
    v0 = (x - offset, y - offset, z - offset)
    v1 = (X + offset, y - offset, z - offset)
    v2 = (X + offset, Y + offset, z - offset)
    v3 = (x - offset, Y + offset, z - offset)
    v4 = (x - offset, y - offset, Z + offset)
    v5 = (X + offset, y - offset, Z + offset)
    v6 = (X + offset, Y + offset, Z + offset)
    v7 = (x - offset, Y + offset, Z + offset)
    return [v0, v1, v2, v3, v4, v5, v6, v7]


def box_edges(vertices) -> List[Tuple[int, int]]:
    return [
        (0, 1), (1, 2), (2, 3), (3, 0),
        (0, 4), (1, 5), (2, 6), (3, 7),
        (4, 5), (5, 6), (6, 7), (7, 4),
    ]


# Небольшой offset для рёбер, чтобы они рисовались "поверх" граней и не было z-fighting
EDGE_OFFSET = 0.5


def add_box_traces(fig: go.Figure, row, sku_color: str, sku: str,
                   first_of_sku: bool, mode: str,
                   mesh_opacity: float,
                   edge_mode: str, edge_color_single: str,
                   edge_width: float, edge_opacity: float) -> Tuple[Optional[int], int]:
    # Вершины для граней (без смещения)
    verts = np.array(box_vertices(row, offset=0.0))
    # Вершины для рёбер (с небольшим смещением наружу для избежания z-fighting)
    verts_edges = np.array(box_vertices(row, offset=EDGE_OFFSET))

    # Mesh (грани) — только если mode == "meshes"
    mesh_idx: Optional[int] = None
    if mode == "meshes":
        faces = [
            (0, 1, 2), (0, 2, 3),
            (4, 5, 6), (4, 6, 7),
            (0, 1, 5), (0, 5, 4),
            (1, 2, 6), (1, 6, 5),
            (2, 3, 7), (2, 7, 6),
            (3, 0, 4), (3, 4, 7),
        ]
        ifaces = np.array(faces).T
        fig.add_trace(go.Mesh3d(
            x=verts[:, 0], y=verts[:, 1], z=verts[:, 2],
            i=ifaces[0], j=ifaces[1], k=ifaces[2],
            color=sku_color,
            opacity=mesh_opacity,
            flatshading=True,
            lighting=dict(ambient=1, diffuse=1, specular=0, roughness=1, fresnel=0),
            lightposition=dict(x=1000, y=2000, z=3000),
            name="box",
            #hoverinfo="skip",
            showscale=False,
            showlegend=False,
            visible=False,
        ))
        mesh_idx = len(fig.data) - 1

    # Рёбра (один трейс на коробку) - используем verts_edges для избежания z-fighting
    x_all, y_all, z_all = [], [], []
    for a, b in box_edges(verts_edges):
        x_all += [verts_edges[a][0], verts_edges[b][0], None]
        y_all += [verts_edges[a][1], verts_edges[b][1], None]
        z_all += [verts_edges[a][2], verts_edges[b][2], None]

    # Цвет рёбер: по SKU или единый
    line_color = sku_color if edge_mode == "sku" else edge_color_single

    # Элемент легенды нужен только в режиме edges=sku и только для «первого» бокса данного SKU
    showlegend = (edge_mode == "sku") and first_of_sku
    # Чтобы элемент был виден в легенде, но линия изначально не рисовалась — legendonly
    initial_visible: Any = "legendonly" if showlegend else False

    fig.add_trace(go.Scatter3d(
        x=x_all, y=y_all, z=z_all,
        mode="lines",
        name=f"SKU {sku}",
        line=dict(color=line_color, width=edge_width),
        opacity=edge_opacity,
        hoverinfo="skip",
        showlegend=showlegend,
        legendgroup=sku if edge_mode == "sku" else None,
        visible=initial_visible,
    ))
    edge_idx = len(fig.data) - 1

    return mesh_idx, edge_idx


def add_centers_trace(fig: go.Figure, df: pd.DataFrame) -> Tuple[int, List[float], List[float], List[float], List[str]]:
    cx = (df["x"].values + df["X"].values) / 2.0
    cy = (df["y"].values + df["Y"].values) / 2.0
    cz = (df["z"].values + df["Z"].values) / 2.0
    dx = (df["X"] - df["x"]).values
    dy = (df["Y"] - df["y"]).values
    dz = (df["Z"] - df["z"]).values

    texts = [
        f"SKU: {sku}<br>"
        f"x:[{x:.0f}, {X:.0f}] y:[{y:.0f}, {Y:.0f}] z:[{z:.0f}, {Z:.0f}]<br>"
        f"size: {dx_:.0f}×{dy_:.0f}×{dz_:.0f}"
        for sku, x, y, z, X, Y, Z, dx_, dy_, dz_ in zip(
            df["SKU"], df["x"], df["y"], df["z"], df["X"], df["Y"], df["Z"], dx, dy, dz
        )
    ]

    fig.add_trace(go.Scatter3d(
        x=[], y=[], z=[],
        mode="markers",
        marker=dict(size=2, color="rgba(0,0,0,0)"),
        hovertext=[],
        hoverinfo="text",
        name="info",
        showlegend=False,
        visible=True,
    ))
    centers_idx = len(fig.data) - 1
    return centers_idx, cx.tolist(), cy.tolist(), cz.tolist(), texts


def add_pallet(fig: go.Figure, length: Optional[float], width: Optional[float], z0: float,
               face_color="#C0C0C0", face_opacity=0.15, edge_color="#666666",
               edge_width=2.0) -> List[int]:
    if length is None or width is None:
        return []
    x0, y0, z = 0.0, 0.0, z0
    x1, y1 = float(length), float(width)
    verts = np.array([
        [x0, y0, z],
        [x1, y0, z],
        [x1, y1, z],
        [x0, y1, z],
    ])
    faces = np.array([[0, 1, 2], [0, 2, 3]]).T

    fig.add_trace(go.Mesh3d(
        x=verts[:, 0], y=verts[:, 1], z=verts[:, 2],
        i=faces[0], j=faces[1], k=faces[2],
        color=face_color,
        opacity=face_opacity,
        flatshading=True,
        hoverinfo="skip",
        showlegend=False,
        visible=True,
    ))
    mesh_idx = len(fig.data) - 1

    fig.add_trace(go.Scatter3d(
        x=[x0, x1, x1, x0, x0],
        y=[y0, y0, y1, y1, y0],
        z=[z, z, z, z, z],
        mode="lines",
        line=dict(color=edge_color, width=edge_width),
        hoverinfo="skip",
        showlegend=False,
        visible=True,
        name="pallet"
    ))
    edge_idx = len(fig.data) - 1

    return [mesh_idx, edge_idx]


def build_figure(df: pd.DataFrame, mode="meshes", color_by="none",
                 mesh_opacity=0.35,
                 edges_mode="sku", edge_color_single="#000000",
                 edge_width=2.0, edge_opacity=0.7,
                 pallet_length: Optional[float] = None,
                 pallet_width: Optional[float] = None):
    fig = go.Figure()

    color_map = build_color_map(df, color_by)
    per_box_meta = []  # [{mesh:int|None, edge:int, sku:str}, ...]
    seen = set()

    # Диапазоны
    xmin, ymin, zmin = df[["x", "y", "z"]].min()
    xmax, ymax, zmax = df[["X", "Y", "Z"]].max()

    # Паллет
    always_visible = add_pallet(fig, pallet_length, pallet_width, float(zmin))

    # Коробки в порядке CSV
    for _, row in df.iterrows():
        sku = row["SKU"]
        sku_color = color_map[sku]
        first_of_sku = sku not in seen
        seen.add(sku)

        mesh_idx, edge_idx = add_box_traces(
            fig, row, sku_color, sku, first_of_sku, mode,
            mesh_opacity,
            edges_mode, edge_color_single,
            edge_width, edge_opacity
        )
        per_box_meta.append({"mesh": mesh_idx, "edge": edge_idx, "sku": sku})

    # Центры для ховера
    centers_idx, cx, cy, cz, texts = add_centers_trace(fig, df)

    # Пэддинги
    pad_x = max(1.0, 0.02 * (xmax - xmin))
    pad_y = max(1.0, 0.02 * (ymax - ymin))
    pad_z = max(1.0, 0.02 * (zmax - zmin))

    fig.update_layout(
        scene=dict(
            xaxis=dict(
                title="X", range=[float(xmin - pad_x), float(xmax + pad_x)],
                backgroundcolor="rgb(240,240,240)", showspikes=False
            ),
            yaxis=dict(
                title="Y", range=[float(ymin - pad_y), float(ymax + pad_y)],
                backgroundcolor="rgb(245,245,245)", showspikes=False
            ),
            zaxis=dict(
                title="Z", range=[float(zmin - pad_z), float(zmax + pad_z)],
                backgroundcolor="rgb(250,250,250)", showspikes=False
            ),
            aspectmode="data",
        ),
        margin=dict(l=0, r=0, t=30, b=0),
        title=f"Boxes visualization (step-by-step, {mode}, edges={edges_mode})",
        showlegend=(edges_mode == "sku"),
        legend=dict(groupclick="toggleitem"),
    )

    return fig, per_box_meta, centers_idx, cx, cy, cz, texts, always_visible


def html_with_interactivity(fig: go.Figure,
                            out_path: Path,
                            per_box_meta: List[Dict],
                            centers_idx: int,
                            cx: List[float], cy: List[float], cz: List[float], texts: List[str],
                            always_visible: List[int],
                            initial_count: int = 1,
                            edges_mode: str = "sku",
                            div_id: str = "boxes_fig",
                            plotlyjs: str = "inline") -> None:
    total = len(per_box_meta)
    meta = {
        "divId": div_id,
        "total": total,
        "initial": max(0, min(initial_count, total)),
        "perBox": per_box_meta,
        "centerTrace": centers_idx,
        "centers": {"cx": cx, "cy": cy, "cz": cz, "texts": texts},
        "alwaysVisible": always_visible,
        "edgesMode": edges_mode,
        "controls": {
            "countLabelId": f"{div_id}_count_label",
            "inputId": f"{div_id}_count_input",
        }
    }

    # JS: управление отображением, клавишами, кнопками, легендой (SKU)
    script = f"""
(function(){{
  function ready(fn) {{
    if (document.readyState !== 'loading') fn();
    else document.addEventListener('DOMContentLoaded', fn);
  }}
  ready(function(){{
    var gd = document.getElementById('{div_id}');
    if (!gd) return;
    var meta = {json.dumps(meta, separators=(',', ':'))};
    var currentN = meta.initial;
    var skuOn = new Set(); // Изначально ни один SKU не выбран (для edgesMode='sku')

    function applyVisibility(){{
      var vis = new Array(gd.data.length).fill(false);

      // показать meshes первых N коробок (если есть)
      for (var i=0; i<currentN; i++) {{
        var m = meta.perBox[i].mesh;
        if (m !== null && m !== undefined) vis[m] = true;
      }}

      // режим рёбер
      if (meta.edgesMode === 'single') {{
        // показываем рёбра для всех первых N коробок
        for (var i=0; i<currentN; i++) {{
          vis[ meta.perBox[i].edge ] = true;
        }}
      }} else if (meta.edgesMode === 'sku') {{
        // показываем рёбра только выбранных SKU среди первых N
        for (var i=0; i<currentN; i++) {{
          var sku = meta.perBox[i].sku;
          if (skuOn.has(sku)) vis[ meta.perBox[i].edge ] = true;
        }}
      }} // none — ничего не делаем

      // всегда видимые (паллет и т.п.)
      (meta.alwaysVisible || []).forEach(function(idx){{ vis[idx] = true; }});
      Plotly.restyle(gd, 'visible', vis);

      // обновить центры-ховеры
      if (meta.centerTrace !== null) {{
        Plotly.restyle(gd, {{
          x: [meta.centers.cx.slice(0, currentN)],
          y: [meta.centers.cy.slice(0, currentN)],
          z: [meta.centers.cz.slice(0, currentN)],
          hovertext: [meta.centers.texts.slice(0, currentN)]
        }}, [meta.centerTrace]);
      }}

      // ui
      var label = document.getElementById(meta.controls.countLabelId);
      if (label) label.textContent = currentN + ' / ' + meta.total;
      var input = document.getElementById(meta.controls.inputId);
      if (input) input.value = currentN;
    }}

    function setN(n){{
      n = parseInt(n||0, 10);
      if (isNaN(n)) n = 0;
      currentN = Math.max(0, Math.min(meta.total, n));
      applyVisibility();
    }}

    function step(d){{ setN(currentN + d); }}

    function buildControls(){{
      var container = document.createElement('div');
      container.style.margin = '8px 0';
      container.style.display = 'flex';
      container.style.gap = '8px';
      container.style.flexWrap = 'wrap';

      function btn(txt, onclick){{
        var b = document.createElement('button');
        b.textContent = txt;
        b.onclick = onclick;
        b.style.padding = '4px 8px';
        return b;
      }}

      var countLabel = document.createElement('span');
      countLabel.id = meta.controls.countLabelId;

      var input = document.createElement('input');
      input.type = 'number';
      input.min = 0;
      input.max = meta.total;
      input.value = meta.initial;
      input.id = meta.controls.inputId;
      input.style.width = '90px';

      var setBtn = btn('Set', function(){{ setN(input.value); }});

      container.appendChild(btn('Start (0)', function(){{ setN(0); }}));
      container.appendChild(btn('-1', function(){{ step(-1); }}));
      container.appendChild(btn('+1', function(){{ step(1); }}));
      container.appendChild(btn('End (All)', function(){{ setN(meta.total); }}));
      container.appendChild(document.createTextNode(' Count: '));
      container.appendChild(countLabel);
      container.appendChild(document.createTextNode(' | Go to: '));
      container.appendChild(input);
      container.appendChild(setBtn);

      gd.parentNode.insertBefore(container, gd.nextSibling);
    }}

    buildControls();

    // Клавиши: A/D и стрелки
    window.addEventListener('keydown', function(e){{
      if (e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') step(1);
      if (e.key === 'ArrowLeft'  || e.key === 'a' || e.key === 'A') step(-1);
    }});

    // Легенда: кликом выбираем/снимаем выделение SKU (только в режиме edges='sku')
    gd.on('plotly_legendclick', function(ev){{
      if (meta.edgesMode !== 'sku') return false;
      var t = gd.data[ev.curveNumber];
      if (!t || !t.legendgroup) return false; // не наши элементы
      var sku = t.legendgroup;
      if (skuOn.has(sku)) skuOn.delete(sku); else skuOn.add(sku);
      applyVisibility();
      return false; // отменяем стандартное поведение
    }});

    // инициализация
    applyVisibility();
  }});
}})();
"""

    config = dict(displaylogo=False, responsive=True)
    html = pio.to_html(
        fig,
        include_plotlyjs=plotlyjs,  # 'inline' офлайн; 'cdn' легче (нужен интернет)
        full_html=True,
        config=config,
        div_id=div_id,
        post_script=script,
    )
    out_path.write_text(html, encoding="utf-8")


def derive_output_path(csv_path: Path, explicit_out: Optional[Path]) -> Path:
    if explicit_out is not None:
        return explicit_out
    return csv_path.with_name(f"{csv_path.stem}_boxes.html")


def main():
    parser = argparse.ArgumentParser(
        description="Визуализация коробок из CSV: HTML с интерактивом (A/D, ←/→, кнопки, выбор SKU в легенде)."
    )
    parser.add_argument("csv", type=Path, help="Путь к CSV файлу с колонками SKU,x,y,z,X,Y,Z")
    parser.add_argument("--mode", choices=["meshes", "wireframe"], default="meshes",
                        help="meshes (грани + рёбра) или wireframe (только рёбра).")
    parser.add_argument("--opacity", type=float, default=0.35, help="Прозрачность граней (для режима meshes).")
    parser.add_argument("--color-by", choices=["none", "sku"], default="none",
                        help="Цвет граней: none (один цвет) или sku (по SKU).")
    parser.add_argument("--edges", choices=["sku", "single", "none"], default="sku",
                        help="Режим рёбер: sku (выбор по легенде), single (единый цвет), none (без рёбер).")
    parser.add_argument("--edge-color", default="#000000",
                        help="Цвет рёбер для режима edges=single (по умолчанию чёрный).")
    parser.add_argument("--edge-width", type=float, default=2.0, help="Толщина рёбер.")
    parser.add_argument("--edge-opacity", type=float, default=0.7, help="Прозрачность рёбер.")
    parser.add_argument("--pallet-length", type=float, default=None, help="Длина паллеты по оси X (начиная с 0).")
    parser.add_argument("--pallet-width", type=float, default=None, help="Ширина паллеты по оси Y (начиная с 0).")
    parser.add_argument("--initial-count", type=int, default=1,
                        help="Сколько коробок показать изначально (по порядку CSV).")
    parser.add_argument("--out", type=Path, default=None, help="Путь к HTML (по умолчанию <csv>_boxes.html).")
    parser.add_argument("--plotlyjs", choices=["inline", "cdn", "directory"], default="inline",
                        help="Как подключать plotly.js: inline (офлайн), cdn (легче файл, нужен интернет), directory.")
    args = parser.parse_args()

    try:
        df = read_boxes(args.csv)
    except Exception as e:
        print(f"Ошибка чтения: {e}", file=sys.stderr)
        sys.exit(1)

    fig, per_box_meta, centers_idx, cx, cy, cz, texts, always_visible = build_figure(
        df,
        mode=args.mode,
        color_by=args.color_by,
        mesh_opacity=args.opacity,
        edges_mode=args.edges,
        edge_color_single=args.edge_color,
        edge_width=args.edge_width,
        edge_opacity=args.edge_opacity,
        pallet_length=args.pallet_length,
        pallet_width=args.pallet_width,
    )

    out_path = derive_output_path(args.csv, args.out)
    try:
        html_with_interactivity(
            fig,
            out_path=out_path,
            per_box_meta=per_box_meta,
            centers_idx=centers_idx,
            cx=cx, cy=cy, cz=cz, texts=texts,
            always_visible=always_visible,
            initial_count=max(0, min(args.initial_count, len(per_box_meta))),
            edges_mode=args.edges,
            div_id="boxes_fig",
            plotlyjs=args.plotlyjs,
        )
        print(f"Сохранено: {out_path}")
        try:
            webbrowser.open(out_path.resolve().as_uri())
        except Exception:
            pass
    except Exception as e:
        print(f"Не удалось сохранить HTML: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
