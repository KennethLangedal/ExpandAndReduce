# Expand & Reduce: a Framework Bridging Branch-and-Reduce and Dynamic Programming

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-GNU17-blue.svg)](https://gcc.gnu.org/)
[![Heidelberg University](https://img.shields.io/badge/Heidelberg-University-c1002a)](https://www.uni-heidelberg.de)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.22661369.svg)](https://doi.org/10.5281/zenodo.22661369)

By [Martin Vatshelle](https://orcid.org/0009-0009-1788-2509), [Kenneth Langedal](https://orcid.org/0009-0001-6838-4640), and [Ernestine Großmann](https://orcid.org/0000-0002-9678-0253)

---

## Description

This is the implementation of **Expand & Reduce (ENR)** for computing maximum weight independent sets (MWIS). The method is based on an unguided search for edge expansions that yield a smaller graph after applying domination and clique fold. Following Gellner et al., we restrict edge expansions to a vertex's neighborhood. This localizes the graph blow-up and makes it easier to track candidate vertices for domination. If expanding and reducing yield a smaller graph, we continue from this smaller instance; otherwise, we revert the changes and start from another vertex.

---

## Artifact Evaluation (SIAM ALENEX)

This artifact is submitted for the **Availability** and **Reproducibility** badges.

* **Archival Dataset (Zenodo)**: [10.5281/zenodo.22661369](https://doi.org/10.5281/zenodo.22661369)
* **Pre-computed Experimental Results**: The paper's reduction and runtime numbers are listed in [`results.csv`](results.csv).

### Prerequisites
* **Operating System**: Linux / Unix-like environment
* **Compiler**: GCC supporting `-std=gnu17` (GCC $\ge 9$ recommended)
* **Tools**: GNU Make, bash, `curl` or `wget`, `tar`

### Quick Smoke Test (< 5 seconds)
To compile the code and verify the reduction pipeline on small instances in `data/tiny/`:
```bash
./runme.sh
# or: bash scripts/run_test.sh
```

### Full Paper Reproduction
To reproduce the experimental evaluation across all 32 benchmark graphs:
```bash
./runme.sh full
# or: bash scripts/run_full.sh
```
* **Dataset**: If the full dataset is not present in `data/full/`, the script will automatically invoke `scripts/download_instances.sh` to fetch and unpack the 32 benchmark instances from Zenodo.
* **Estimated Runtime**: Processing all 32 instances sequentially takes several hours depending on hardware (several large SNAP/OSM instances run for ~1 hour each).
* **Output**: Generated results are saved to `results_reproduced.csv` and can be compared directly against [`results.csv`](results.csv). Reduced graphs and metadata files are saved to `output/full/reduced/` and `output/full/meta/`.

### Manual Dataset Download
If you wish to download the 32 benchmark instances separately without running the full reproduction script:
```bash
bash scripts/download_instances.sh
```

### Baseline Method: Learn & Reduce
In our paper, we compare against *Learn & Reduce* (Gellner et al.).
* **Repository**: [https://github.com/manansg/learn-and-reduce](https://github.com/manansg/learn-and-reduce)
* **Configuration**: Ran with default parameters and standard pre-trained models on the same benchmark instances as described in their paper.
* **Evaluation**: The baseline numbers reported in the paper were obtained by running their public implementation under the same hardware and environment configurations.

### AI Disclosure
The artifact evaluation and reproduction automation scripts (`scripts/`, `runme.sh`) were generated with assistance from Google DeepMind's Gemini 3.7 Flash. The scientific research, algorithmic techniques, paper, and core C implementation (`src/`, `include/`) were authored entirely by the researchers without AI assistance.

---

## Compiling

To build the executable `ENR` manually:

```bash
git clone https://github.com/KennethLangedal/ExpandAndReduce.git
cd ExpandAndReduce
make
```

---

## Usage

### Running the Program

```bash
./ENR [input graph] [reduced graph] [meta file]
```

### Input Format

Expand-and-reduce expects graphs in the METIS graph format. A graph with **N** vertices is stored using **N + 1** lines. The first line lists the number of vertices, the number of edges, and the weight type. For CHILS, the first line should use `10` as the weight type to indicate integer vertex weights. Each subsequent line first gives the weight and then lists the neighbors of that node.

Here is an example of a graph with 3 vertices of weights 15, 15, and 20, where vertex 3 (weight 20) is connected to vertices 1 and 2 (weight 15 each):

```
3 2 10
15 3
15 3
20 1 2
```

Vertices are 1-indexed, and edges appear in the neighborhoods of both endpoints.

### Output Format

The output consists of two files:
1. A **reduced graph** in the same METIS format as the input.
2. A **metadata file** describing vertex assignments and mappings:

| Section | Description | Format |
| --- | --- | --- |
| Summary | Counts for included, excluded, and remaining vertices | Three lines with labeled integers |
| Included | Vertices in the independent set | Space-separated list |
| Excluded | Vertices removed from graph | Space-separated list |
| Mapping | Vertices remaining in the reduced graph | 1 line per reduced vertex |

For the example graph above, the resulting metadata file looks like:

```
% Reduced Graph Metadata
included: 2
excluded: 1
remaining: 0

% Vertices included in the independent set
included_vertices: 1 2 

% Vertices excluded from the independent set
excluded_vertices: 3 

% Reverse mapping (reduced vertex ID: original set)
```

Additionally, `ENR` outputs a single CSV line to `STDOUT` in the format:
```
instance,n,m,nk,mk,offset,tred
```
where:
* `instance`: Name/path of the input graph
* `n`, `m`: Original vertex and edge counts
* `nk`, `mk`: Reduced vertex and edge counts
* `offset`: Weight offset accumulated by reductions
* `tred`: Reduction time in seconds

The original experimental output is preserved in [`results.csv`](results.csv).

---

## License
This project is licensed under the [MIT License](LICENSE).