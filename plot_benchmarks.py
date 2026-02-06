#!/usr/bin/env python3
"""
Plot benchmark results comparing three versions across seven benchmarks.
"""

import pandas as pd
import matplotlib.pyplot as plt
import os
from pathlib import Path

# Configuration
RESULTS_DIR = Path("results")
VERSIONS = ["elevate_sc", "elevate_no_sc", "no_elevate"]
BENCHMARKS = ["clock", "cpu", "getppid", "read", "recv", "send", "write"]
VERSION_LABELS = {
    "elevate_sc": "Elevate + SC",
    "elevate_no_sc": "Elevate No SC",
    "no_elevate": "No Elevate"
}
COLORS = {
    "elevate_sc": "#2E86AB",      # Blue
    "elevate_no_sc": "#A23B72",   # Purple
    "no_elevate": "#F18F01"       # Orange
}

def load_benchmark_data(version, benchmark):
    """Load data for a specific version and benchmark."""
    filepath = RESULTS_DIR / version / f"new_lebench_{benchmark}.csv"
    if not filepath.exists():
        print(f"Warning: {filepath} not found")
        return None
    
    df = pd.read_csv(filepath)
    return df

def plot_benchmark(benchmark, save_dir="plots"):
    """Create a plot comparing all versions for a given benchmark."""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot each version
    for version in VERSIONS:
        data = load_benchmark_data(version, benchmark)
        if data is not None:
            # Check if this benchmark has Size column (I/O benchmarks) or not (clock/cpu)
            if 'Size' in data.columns:
                # I/O benchmarks: Group by Size and calculate mean latency
                latency_col = 'Latency' if 'Latency' in data.columns else 'latency'
                grouped = data.groupby('Size')[latency_col].mean().reset_index()
                x_data = grouped['Size']
                y_data = grouped[latency_col]
                xlabel = 'Size (bytes)'
            else:
                # Clock/CPU benchmarks: Calculate mean latency across all iterations
                latency_col = 'latency' if 'latency' in data.columns else 'Latency'
                # For display, just show mean as a single point at x=0
                mean_latency = data[latency_col].mean()
                x_data = [0]
                y_data = [mean_latency]
                xlabel = 'Benchmark'
            
            ax.plot(x_data, y_data, 
                   marker='o', markersize=4, linewidth=2,
                   label=VERSION_LABELS[version], 
                   color=COLORS[version], alpha=0.8)
    
    # Formatting
    ax.set_xlabel(xlabel, fontsize=12, fontweight='bold')
    ax.set_ylabel('Latency (seconds)', fontsize=12, fontweight='bold')
    ax.set_title(f'{benchmark.upper()} Benchmark Comparison', 
                fontsize=14, fontweight='bold')
    ax.legend(loc='best', fontsize=10)
    ax.grid(True, alpha=0.3, linestyle='--')
    
    # Use scientific notation for y-axis if values are very small
    ax.ticklabel_format(style='scientific', axis='y', scilimits=(0,0))
    
    plt.tight_layout()
    
    # Save plot
    os.makedirs(save_dir, exist_ok=True)
    output_path = os.path.join(save_dir, f"{benchmark}_comparison.png")
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_path}")
    
    return fig

def plot_all_benchmarks(save_dir="plots", show=False):
    """Plot all benchmarks and optionally display them."""
    print("Generating benchmark comparison plots...")
    print(f"Results directory: {RESULTS_DIR}")
    print(f"Output directory: {save_dir}")
    print("-" * 60)
    
    for benchmark in BENCHMARKS:
        print(f"Processing {benchmark}...")
        fig = plot_benchmark(benchmark, save_dir)
        if show:
            plt.show()
        else:
            plt.close(fig)
    
    print("-" * 60)
    print(f"All plots saved to '{save_dir}/' directory")

def create_summary_grid(save_dir="plots"):
    """Create a summary figure with all benchmarks in a grid."""
    fig, axes = plt.subplots(3, 3, figsize=(18, 14))
    axes = axes.flatten()
    
    for idx, benchmark in enumerate(BENCHMARKS):
        ax = axes[idx]
        
        # Plot each version
        for version in VERSIONS:
            data = load_benchmark_data(version, benchmark)
            if data is not None:
                # Check if this benchmark has Size column (I/O benchmarks) or not (clock/cpu)
                if 'Size' in data.columns:
                    # I/O benchmarks: Group by Size and calculate mean latency
                    latency_col = 'Latency' if 'Latency' in data.columns else 'latency'
                    grouped = data.groupby('Size')[latency_col].mean().reset_index()
                    x_data = grouped['Size']
                    y_data = grouped[latency_col]
                    xlabel = 'Size (bytes)'
                else:
                    # Clock/CPU benchmarks: Calculate mean latency across all iterations
                    latency_col = 'latency' if 'latency' in data.columns else 'Latency'
                    mean_latency = data[latency_col].mean()
                    x_data = [0]
                    y_data = [mean_latency]
                    xlabel = 'Benchmark'
                
                ax.plot(x_data, y_data, 
                       marker='o', markersize=3, linewidth=1.5,
                       label=VERSION_LABELS[version], 
                       color=COLORS[version], alpha=0.8)
        
        # Formatting
        ax.set_xlabel(xlabel, fontsize=9)
        ax.set_ylabel('Latency (s)', fontsize=9)
        ax.set_title(f'{benchmark.upper()}', fontsize=10, fontweight='bold')
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.ticklabel_format(style='scientific', axis='y', scilimits=(0,0))
        
        if idx == 0:  # Only show legend on first plot
            ax.legend(loc='best', fontsize=8)
    
    # Hide extra subplots
    for idx in range(len(BENCHMARKS), len(axes)):
        axes[idx].set_visible(False)
    
    plt.suptitle('Benchmark Comparison - All Tests', 
                fontsize=16, fontweight='bold', y=0.995)
    plt.tight_layout()
    
    # Save summary plot
    os.makedirs(save_dir, exist_ok=True)
    output_path = os.path.join(save_dir, "summary_all_benchmarks.png")
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"Saved summary: {output_path}")
    plt.close(fig)

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='Plot benchmark comparison results')
    parser.add_argument('--output', '-o', default='plots', 
                       help='Output directory for plots (default: plots)')
    parser.add_argument('--show', action='store_true', 
                       help='Display plots interactively')
    parser.add_argument('--summary', action='store_true', 
                       help='Also create a summary grid plot')
    
    args = parser.parse_args()
    
    # Generate individual plots
    plot_all_benchmarks(save_dir=args.output, show=args.show)
    
    # Generate summary grid if requested
    if args.summary:
        print("\nGenerating summary grid plot...")
        create_summary_grid(save_dir=args.output)
