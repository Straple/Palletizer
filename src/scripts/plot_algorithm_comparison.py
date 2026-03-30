#!/usr/bin/env python3
"""
Bar chart comparing all 5 algorithms on single-pallet metrics.
Outputs PDF for LaTeX inclusion.
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
import numpy as np

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

ALGORITHMS = ['GreedySolver', 'DBLFSolver', 'FFDSolver', 'GeneticSolver', 'LNSSolver']
SHORT_NAMES = ['Greedy', 'DBLF', 'FFD', 'Genetic', 'LNS']
COLORS = ['#2ca02c', '#9467bd', '#8c564b', '#d62728', '#1f77b4']


def main():
    df = pd.read_csv('logs/results.csv')

    rows = []
    for alg in ALGORITHMS:
        r = df[
            (df['pallets'] == 'one') &
            (df['algorithm'] == alg) &
            (df['timelimit_ms'] == 5000) &
            (df['score_percolation_mult'] == 1.0) &
            (df['score_min_support_ratio_mult'] == 2.0)
        ]
        if len(r) == 0:
            continue
        r = r.iloc[0]
        rows.append({
            'algorithm': alg,
            'perc': r['percolation_avg'] * 100,
            'perc_lo': r['percolation_ci_low'] * 100,
            'perc_hi': r['percolation_ci_high'] * 100,
            'supp': r['min_support_ratio_avg'] * 100,
            'supp_lo': r['min_support_ratio_ci_low'] * 100,
            'supp_hi': r['min_support_ratio_ci_high'] * 100,
        })

    data = pd.DataFrame(rows)
    x = np.arange(len(data))
    width = 0.35

    fig, ax = plt.subplots()

    perc_err = [data['perc'] - data['perc_lo'], data['perc_hi'] - data['perc']]
    supp_err = [data['supp'] - data['supp_lo'], data['supp_hi'] - data['supp']]

    bars1 = ax.bar(x - width / 2, data['perc'], width, yerr=perc_err,
                   label='Percolation', color='#1f77b4', alpha=0.85, capsize=4)
    bars2 = ax.bar(x + width / 2, data['supp'], width, yerr=supp_err,
                   label='Min support ratio', color='#ff7f0e', alpha=0.85, capsize=4)

    ax.set_ylabel('Value (%)')
    ax.set_xticks(x)
    ax.set_xticklabels(SHORT_NAMES)
    ax.set_ylim(0, 100)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    ax.set_axisbelow(True)

    

    plt.tight_layout()

    output = 'papers/figures/algorithm_comparison.pdf'
    plt.savefig(output, format='pdf', bbox_inches='tight')
    plt.savefig(output.replace('.pdf', '.png'), format='png', bbox_inches='tight', dpi=150)
    print(f'Saved {output}')


if __name__ == '__main__':
    main()
