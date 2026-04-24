# Expand \& Reduce

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-GNU17-blue.svg)](https://gcc.gnu.org/)
[![Heidelberg University](https://img.shields.io/badge/Heidelberg-University-c1002a)](https://www.uni-heidelberg.de)

By [Martin Vatshelle](https://orcid.org/0009-0009-1788-2509), [Kenneth Langedal](https://orcid.org/0009-0001-6838-4640), and [Ernestine Großmann](https://orcid.org/0000-0002-9678-0253)

## Description
This is the preliminary implementation for expand-and-reduce, based on an unguided search for edge expansions that yield a smaller graph after applying domination and clique fold. Following Gellner et al., we restrict edge expansions to a vertex's neighborhood. This localizes the graph blow-up and makes it easier to track candidate vertices for domination. If expanding and reducing yield a smaller graph, we continue from this smaller instance; otherwise, we revert the changes and start from another vertex.

## Installation

There are no dependencies for this project beyond a ```-std=gnu17``` compatible C compiler.

```bash
git clone https://github.com/KennethLangedal/ExpandAndReduce.git
cd ExpandAndReduce
make
```

## Usage

### Input Format

Expand-and-reduce expects graphs on the METIS graph format. A graph with **N** vertices is stored using **N + 1** lines. The first line lists the number of vertices, the number of edges, and the weight type. For CHILS, the first line should use 10 as the weight type to indicate integer vertex weights. Each subsequent line first gives the weight and then lists the neighbors of that node.

Here is an example of a graph with 3 vertices of weight 15, 15, and 20, where the weight 20 vertex is connected to the two 15-weight vertices.

```
3 2 10
15 3
15 3
20 1 2
```

Notice that vertices are 1-indexed, and edges appear in the neighborhoods of both endpoints.

### Running the Program

```bash
./ENR [input graph] [reduced graph] [meta file]
```

### Output Format

The output consists of two files. A reduced graph on the same METIS format as the input, and a meta file defined below.

| Section | Description | Format |
| --- | --- | --- |
| Summary | Counts for included, excluded, and remaining vertices | Three lines with labeled integers |
| Included | Vertices in the independent set | Space-separated list |
| Excluded | Vertices removed from graph | Space-separated list |
| Mapping | Vertices remaining in the reduced graph | 1 line per reduced vertex |

As with the input format, all vertices are 1-indexed.

For the example graph above, the resulting meta file would look like this:

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

In addition to these two files, the program also outputs a single CSV line to the STDOUT on the following format.

```
instance,n,m,nk,mk,offset,tred
```
The [results.csv](results.csv) file contains this output from our experimental evaluation.

## License
This project is licensed under the [MIT License](LICENSE).