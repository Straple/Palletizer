#!/usr/bin/env python3
"""
Скрипт для построения графика зависимости качества укладки от веса w2.
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
    
    # Фильтруем только LNSSolver с timelimit_ms=10000, w1=1.0 и разными w2
    weight_data = df[
        (df['algorithm'] == 'LNSSolver') & 
        (df['timelimit_ms'] == 10000) & 
        (df['pallets'] == 'one') &
        (df['score_percolation_mult'] == 1.0)
    ].sort_values('score_min_support_ratio_mult')
    
    # Убираем дубликаты по w2 (берём первую строку для каждого w2)
    weight_data = weight_data.drop_duplicates(subset=['score_min_support_ratio_mult'])
    
    print(f"Найдено {len(weight_data)} строк с данными по весам")
    print(weight_data[['score_min_support_ratio_mult', 'percolation_avg', 'min_support_ratio_avg']])
    
    # Извлекаем данные
    w2_values = weight_data['score_min_support_ratio_mult'].values
    
    # Percolation
    perc_avg = weight_data['percolation_avg'].values * 100  # в проценты
    perc_ci_low = weight_data['percolation_ci_low'].values * 100
    perc_ci_high = weight_data['percolation_ci_high'].values * 100
    
    # Min support ratio
    supp_avg = weight_data['min_support_ratio_avg'].values * 100
    supp_ci_low = weight_data['min_support_ratio_ci_low'].values * 100
    supp_ci_high = weight_data['min_support_ratio_ci_high'].values * 100
    
    # Создаём график
    fig, ax = plt.subplots()
    
    # Цвета
    color_perc = '#1f77b4'  # синий
    color_supp = '#ff7f0e'  # оранжевый
    
    # Percolation с CI
    ax.plot(w2_values, perc_avg, 'o-', color=color_perc, label='Percolation (avg)', linewidth=2, markersize=6)
    ax.fill_between(w2_values, perc_ci_low, perc_ci_high, color=color_perc, alpha=0.2, label='Percolation (90% CI)')
    
    # Min support ratio с CI
    ax.plot(w2_values, supp_avg, 's-', color=color_supp, label='Min support ratio (avg)', linewidth=2, markersize=6)
    ax.fill_between(w2_values, supp_ci_low, supp_ci_high, color=color_supp, alpha=0.2, label='Min support ratio (90% CI)')
    
    # Настройка осей
    ax.set_xlabel('$w_2$')
    ax.set_ylabel('Value (%)')
    ax.set_xlim(-0.2, 5.2)
    ax.set_ylim(0, 100)
    
    # Сетка
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_axisbelow(True)
    
    # Легенда
    ax.legend(loc='lower right', framealpha=0.9)
    
    plt.tight_layout()
    
    # Сохраняем в PDF
    output_path = 'papers/figures/weights_comparison.pdf'
    plt.savefig(output_path, format='pdf', bbox_inches='tight')
    print(f"График сохранён в {output_path}")
    
    # Также сохраним PNG для превью
    plt.savefig('papers/figures/weights_comparison.png', format='png', bbox_inches='tight', dpi=150)
    print("PNG превью сохранено в papers/figures/weights_comparison.png")

if __name__ == '__main__':
    main()
