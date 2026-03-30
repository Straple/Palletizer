#!/usr/bin/env python3
"""
Plot LNSSolver vs GeneticSolver performance over time.
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

    base_filter = (
        (df['pallets'] == 'one') &
        (df['score_percolation_mult'] == 1.0) &
        (df['score_min_support_ratio_mult'] == 2.0)
    )

    lns = df[base_filter & (df['algorithm'] == 'LNSSolver')].drop_duplicates(subset=['timelimit_ms']).sort_values('timelimit_ms')
    gen = df[base_filter & (df['algorithm'] == 'GeneticSolver')].drop_duplicates(subset=['timelimit_ms']).sort_values('timelimit_ms')

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(6.5, 8))

    c_lns = '#1f77b4'
    c_gen = '#d62728'

    for ax, metric, ylabel in [
        (ax1, 'percolation', 'Percolation (%)'),
        (ax2, 'min_support_ratio', 'Min support ratio (%)'),
    ]:
        for data, label, color, marker in [
            (lns, 'LNSSolver', c_lns, 'o'),
            (gen, 'GeneticSolver', c_gen, 's'),
        ]:
            avg = data[f'{metric}_avg'].values * 100
            ci_lo = data[f'{metric}_ci_low'].values * 100
            ci_hi = data[f'{metric}_ci_high'].values * 100
            t = data['timelimit_ms'].values

            ax.plot(t, avg, f'{marker}-', color=color, label=f'{label} (avg)', linewidth=2, markersize=5)
            ax.fill_between(t, ci_lo, ci_hi, color=color, alpha=0.15)

        ax.set_xlabel('Time limit (ms)')
        ax.set_ylabel(ylabel)
        ax.set_xscale('log')
        ax.set_xlim(400, 80000)
        ax.set_xticks([500, 1000, 2000, 5000, 10000, 30000, 60000])
        ax.set_xticklabels(['0.5s', '1s', '2s', '5s', '10s', '30s', '60s'])
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.set_axisbelow(True)
        ax.legend(loc='lower right', framealpha=0.9)

    plt.tight_layout()

    output = 'papers/figures/lns_vs_genetic_time.pdf'
    plt.savefig(output, format='pdf', bbox_inches='tight')
    plt.savefig(output.replace('.pdf', '.png'), format='png', bbox_inches='tight', dpi=150)
    print(f'Saved {output}')


if __name__ == '__main__':
    main()
