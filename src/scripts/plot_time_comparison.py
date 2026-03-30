#!/usr/bin/env python3
"""
Скрипт для построения графика зависимости качества укладки от времени выполнения.
Создаёт PDF файл для включения в LaTeX статью.
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
import numpy as np

# Используем backend для PDF без GUI
matplotlib.use('Agg')

# Настройка для LaTeX-совместимых шрифтов
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
    # Читаем данные
    df = pd.read_csv('logs/results.csv')
    
    # Фильтруем только LNSSolver с разными временными лимитами
    # Строки с w1=1.0, w2=2.0 и разными timelimit_ms
    time_data = df[
        (df['algorithm'] == 'LNSSolver') & 
        (df['pallets'] == 'one') &
        (df['score_percolation_mult'] == 1.0) & 
        (df['score_min_support_ratio_mult'] == 2.0)
    ].sort_values('timelimit_ms')
    
    time_data = time_data.drop_duplicates(subset=['timelimit_ms'])
    
    print(f"Найдено {len(time_data)} строк с данными по времени")
    print(time_data[['timelimit_ms', 'percolation_avg', 'min_support_ratio_avg']])
    
    # Извлекаем данные
    timelimits = time_data['timelimit_ms'].values
    
    # Percolation
    perc_avg = time_data['percolation_avg'].values * 100  # в проценты
    perc_ci_low = time_data['percolation_ci_low'].values * 100
    perc_ci_high = time_data['percolation_ci_high'].values * 100
    
    # Min support ratio
    supp_avg = time_data['min_support_ratio_avg'].values * 100
    supp_ci_low = time_data['min_support_ratio_ci_low'].values * 100
    supp_ci_high = time_data['min_support_ratio_ci_high'].values * 100
    
    # Создаём график
    fig, ax = plt.subplots()
    
    # Цвета
    color_perc = '#1f77b4'  # синий
    color_supp = '#ff7f0e'  # оранжевый
    
    # Percolation с CI
    ax.plot(timelimits, perc_avg, 'o-', color=color_perc, label='Percolation (avg)', linewidth=2, markersize=6)
    ax.fill_between(timelimits, perc_ci_low, perc_ci_high, color=color_perc, alpha=0.2, label='Percolation (90% CI)')
    
    # Min support ratio с CI
    ax.plot(timelimits, supp_avg, 's-', color=color_supp, label='Min support ratio (avg)', linewidth=2, markersize=6)
    ax.fill_between(timelimits, supp_ci_low, supp_ci_high, color=color_supp, alpha=0.2, label='Min support ratio (90% CI)')
    
    # Настройка осей
    ax.set_xlabel('Time limit (ms)')
    ax.set_ylabel('Value (%)')
    ax.set_xscale('log')
    ax.set_xlim(400, 80000)
    ax.set_ylim(0, 100)
    
    # Сетка
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_axisbelow(True)
    
    # Легенда
    ax.legend(loc='lower right', framealpha=0.9)
    
    # Метки на оси X
    ax.set_xticks([500, 1000, 2000, 5000, 10000, 30000, 60000])
    ax.set_xticklabels(['0.5s', '1s', '2s', '5s', '10s', '30s', '60s'])

    plt.tight_layout()
    
    # Сохраняем в PDF
    output_path = 'papers_general/figures/time_comparison.pdf'
    plt.savefig(output_path, format='pdf', bbox_inches='tight')
    print(f"График сохранён в {output_path}")
    
    # Также сохраним PNG для превью
    plt.savefig('papers_general/figures/time_comparison.png', format='png', bbox_inches='tight', dpi=150)
    print("PNG превью сохранено в papers_general/figures/time_comparison.png")

if __name__ == '__main__':
    main()
