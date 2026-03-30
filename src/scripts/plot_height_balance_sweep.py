#!/usr/bin/env python3
"""
Plot effect of height_balance_mult on multi-pallet metrics.
Outputs PDF for LaTeX inclusion.
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib

matplotlib.use('Agg')

plt.rcParams.update({
    'font.family': 'serif',
    'font.size': 14,
    'axes.labelsize': 14,
    'axes.titlesize': 14,
    'legend.fontsize': 12,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'figure.figsize': (6.5, 4.5),
    'figure.dpi': 300,
})


def main():
    df = pd.read_csv('logs/results.csv')

    data = df[
        (df['pallets'] == 'multi') &
        (df['algorithm'] == 'LNSSolver') &
        (df['timelimit_ms'] == 10000) &
        (df['score_percolation_mult'] == 1.0) &
        (df['score_min_support_ratio_mult'] == 2.0)
    ].sort_values('score_height_balance_mult')

    data = data.drop_duplicates(subset=['score_height_balance_mult'])

    print(f'Found {len(data)} rows for height_balance_mult sweep')
    print(data[['score_height_balance_mult', 'percolation_avg', 'min_support_ratio_avg', 'height_balance_avg']])

    w4 = data['score_height_balance_mult'].values

    fig, ax = plt.subplots()

    metrics = [
        ('percolation', 'Percolation', '#1f77b4', 'o'),
        ('min_support_ratio', 'Min support ratio', '#ff7f0e', 's'),
        ('height_balance', 'Height balance', '#2ca02c', '^'),
    ]

    for key, label, color, marker in metrics:
        avg = data[f'{key}_avg'].values * 100
        ci_lo = data[f'{key}_ci_low'].values * 100
        ci_hi = data[f'{key}_ci_high'].values * 100
        ax.plot(w4, avg, f'{marker}-', color=color, label=f'{label} (avg)', linewidth=2, markersize=5)
        ax.fill_between(w4, ci_lo, ci_hi, color=color, alpha=0.15)

    ax.set_xlabel('Weight $w_4$ (height balance)')
    ax.set_ylabel('Value (%)')
    ax.set_xlim(-0.2, 10.5)
    ax.set_ylim(0, 105)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_axisbelow(True)
    ax.legend(loc='lower left', framealpha=0.9)
    ax.set_title('Multi-pallet metrics vs height balance weight $w_4$ (LNSSolver, 10s)')

    plt.tight_layout()

    output = 'papers_general/figures/height_balance_sweep.pdf'
    plt.savefig(output, format='pdf', bbox_inches='tight')
    plt.savefig(output.replace('.pdf', '.png'), format='png', bbox_inches='tight', dpi=150)
    print(f'Saved {output}')


if __name__ == '__main__':
    main()
