# CP-Profiler

CP-Profiler provides search tree visualisations for executions of constraint programming solvers.

## Table of Contents

- [Building](#building)
- [Usage](#usage)

### Building

Dependencies:

- Qt5.x

```
    git submodule update --init
    mkdir build && cd build
    qmake .. && make
```

### Usage

1. Starting CP-Profiler

    `cp-profiler.app/Contents/MacOS/cp-profiler` (Mac)

This starts a local TCP server listening on one of the available ports (6565 by default).

2. Executing a supported solver

The solver must implement the profiling protocol (TODO). Integration libraries are available if you wish to extend your solver to work with CP-Profiler.


### The Protocol (high level)

The following describes the protocol that a solver must implement to communicate with the profiler.

The protocol distinguishes between the following types of messages: **`Start`**, **`Restart`**, **`Done`**, and **`Node`**.



The **`Start`** message is sent at the beginning of the execution (**TODO**: multithreaded search). The message has two optional (?) parameters:

- `Name`: execution's descriptor to be presented to the user (e.g. the model's name)
- `Execution ID`: globally unique identifier used to distinguish between different executions.

**TODO**: any other parameters?

The **`Restart`** message is sent whenever a solver performs a restart in case of a restart-based search.

The **`Done`** message is sent to the profiler at the end of the execution to indicate that no further nodes should be expected.

The **`Node`** message is sent whenever a new node is explored by the solver and contains information necessary for reconstructing the search tree. The required parameters are:

- `Node ID`: unique node identifier in the execution.
- `Parent ID`: the identifier (`Node ID`) of the parent node. A root node can have an identifier of `-1`. (**TODO**: needs checking)
- `Alternative`: the node's position relative to its siblings; for the left-most child it is `0`, for the second left-most it is `1` etc.
- `Number of Children`: the number of children nodes. If not known, can be set to `0` and the node will be extended with extra children later on if necessary. It is, however, advisable to specify the number of children the profiler should expect (for example, the yet to arrive nodes can be visualised to give a better idea about the search).
- `Status`: allows to distinguish between different types of nodes. Supported values are:
     - *BRANCH*: internal node in the tree;
     - *SOLUTION*: leaf node representing a solution;
     - *FAILURE*: leaf node representing a failure;
     - *SKIPPED*: leaf node representing unexplored search space due to backjumping.

**Example**. The following sequence of nodes (excluding the `Start` and `Done` messages) produces the simple tree below:

|  `Label` | `Node ID` | `Parent ID` | `Alternative` | `Number of Children` | `Status` |
|:--------:|:---------:|:-----------:|:-------------:|:--------------------:|:--------:|
|   Root   |     0     |      -1     |       -1      |           2          |  BRANCH  |
|  Failure |     1     |      0      |       0       |           0          |  FAILED  |
| Solution |     2     |      0      |       1       |           0          |  SOLVED  |

![A simple search tree](todo)

### The Protocol (low level)

Each message starts with a four-byte integer encoding the size of the remainder of the message in bytes. This is followed by a single byte encoding the type of the message. The corresponding values are: **`Node`**: `0`, **`Done`**: `1`, **`Start`**: `2`, **`Restart`**: `3`.

#### `Node` message

In case the message is of the type **`Node`**, the following fields are added in order: `id`, `pid`, `alt`, `children` and `status`.

Node identifiers `id` and `pid` are represented using three four-byte integers: first identifies the identifier of the node within a thread, the second — the identifier of the restart (in a restart-based search), and the third — the identifier of the thread.
The `alt` and `children` fields are represented by a single four byte integer each.
The `status` field is represented by a single byte; its possible values are: *SOLVED*: 0, *FAILED*: 1, *BRANCH*: 2, *SKIPPED*: 3.
All multi-byte integer values are encoded using the *two's compliment* notation in the *big-endian order*.

Additionally, each node message can contain the following optional fields:
- `label`: branching decision (or any arbitrary string to be drawn together with the node);
- `nogood`: string representation of a newly generated nogood in a learning solver;
- `info`: arbitrary information about the node (*TODO*).

Field identifiers and their sizes in bytes:

| field name | field id | size (bytes) |
|:----------:|:--------:|:------------:|
|    `id`    |    n/a   |      12      |
|    `pid`   |    n/a   |      12      |
|    `alt`   |    n/a   |       4      |
| `children` |    n/a   |       4      |
|  `status`  |    n/a   |       1      |
|   `label`  |     0    |      any     |
|  `nogood`  |     1    |      any     |
|   `info`   |     2    |      any     |

**Example**. The following is a possible correspondence between a solver and the profiler that generates the simple tree above.The order in which different fields arrive is shown from top to bottom (rows are numbered for convenience).

*Message 1:*

| Row | Bytes                                                                              | Interpretation                |
|-----|------------------------------------------------------------------------------------|-------------------------------|
| 1   | `00 00 00 21`                                                                      | message size (33)             |
| 2   | `02`                                                                               | message type (*START*)        |
| 3   | `02`                                                                               | field (*info*)                |
| 4   | `00 00 00 1B`                                                                      | string size (27)              |
| 5   | `7b 22 6e 61 6d 65 22 3a 20 22 6d 69 6e 69 6d 61 6c 20 65 78 61 6d 70 6c 65 22 7d` | '{"name": "minimal example"}' |

*Message 2:*

| Row | Bytes         | Interpretation          |
|-----|---------------|-------------------------|
| 6   | `00 00 00 2B` | message size (43)       |
| 7   | `00`          | message type (**NODE**) |
| 8   | `00 00 00 00` | node id (0)             |
| 9   | `FF FF FF FF` | node restart id (-1)    |
| 10  | `FF FF FF FF` | node thread id (-1)     |
| 11  | `FF FF FF FF` | parent id (-1)          |
| 12  | `FF FF FF FF` | parent restart id (-1)  |
| 13  | `FF FF FF FF` | parent thread id (-1)   |
| 14  | `FF FF FF FF` | alternative (-1)        |
| 15  | `00 00 00 02` | children (2)            |
| 16  | `02`          | status (*BRANCH*)       |
| 17  | `00`          | field (label)           |
| 18  | `00 00 00 04` | string size (4)         |
| 19  | `52 6f 6f 74` | 'Root'                  |

*Message 3:*

| Row | Bytes         | Interpretation          |
|-----|---------------|-------------------------|
| 20  | `00 00 00 01` | message size (1)        |
| 21  | `01`          | message type (**DONE**) |


### Tree Visualisations

#### Traditional Tree Visualisation

When a new execution is connected to the profiler it will be added to the list of executions displayed at the top of the main window. For example, in the image below execution *golomb6a.fzn* is shown to be added to the profiler.
To display the execution, select its name from the list and click the *Show Tree* button. Note that the solver can still be running the execution, in which case the profiler will draw the search tree in real time.

![Profiler Conductor](https://bitbucket.org/Msgmaxim/cp-profiler2/raw/d2aad1af0805a47f89459f771f70516f29886a09/docs/images/doc_conductor1.png)

The image below shows an example of a traditional (node-link) visualisation of the search tree. Different types of nodes are shown differently: branch (internal) nodes are shown as blue circles; nodes representing failures are shown as red squares; solution nodes are shown as green diamonds.

Note that the root of the tree is shown in gold colour indicating the currently selected node. Arrow keys on the keyboard allow the user to navigate the tree by changing which node is selected. `Down` navigates to the first child of the current node, `Shift+Down` — to its last child, `Up` — to its the parent, `Left` — to its next sibling on the left, `Right` — to its next sibling on the right. 
Additionally, pressing `R` will navigate to the root of the tree.
The same actions are available under the **`Navigation`** menu.


![Traditional Visualisation Interface](https://bytebucket.org/Msgmaxim/cp-profiler2/raw/d2aad1af0805a47f89459f771f70516f29886a09/docs/images/doc_traditional_interface.png)

If a subtree contains no solutions, it can be collapsed into a special single node displayed as a large red triangle. By default, the tree will be collapse failed subtrees automatically during its construction as a way to deal with large trees. The image below shows the same search tree as above, but with all failed subtrees collapsed.

![Collapsed Failed Subtrees](https://bitbucket.org/Msgmaxim/cp-profiler2/raw/76bb21242ad5427b10b84f7c4cb60f9b557490a0/docs/images/doc_traditional_collapsed.png)

This view of the tree allows the user to show additional information for every node — its label, which usually represents the branching decision made by the solver to move from the parent node to its child. Pressing `L` on the keyboard will display labels for all descendants of the current node. `Shift+L` will display labels on the path to the current node.
For example, the visualisation above shows branching decisions on the path from the first solution (shown as the new current node) to the root of the tree.

Status bar at the bottom of the window displays node statistics: the depth of the tree and the counts of different types of nodes.
The scroll bar on the right lets the user to zoom in/out on the visualisation;

#### Similar Subtree Analysis

This analysis allows users to find similarities within a single search tree.

It can be initiated by selecting **`Similar Subtrees`** from the menu **`Analyses`** (shortcut: `Shift+S`).
The image below shows the result of running the analysis on the search tree above.
Horizontal bars on the left lists all similarities (patterns) found in the tree.
Here, the lengths of the bars indicate are configured to indicate how many subtrees belong to a particular pattern (*count*).
Additionally the bars are sorted so that the patterns with subtrees of larger *size* appear at the top.
Another property of a pattern is its *height*, which indicates the height/depth of subtrees that the pattern represent.

Note that the second from the top pattern is currently selected (shown with orange outline).
The view on the right shows a "preview" (traditional visualisation) of one of the subtrees representing the selected pattern.
The two rows below the show the result of computing the difference in labels on the path from the root to two of the subtrees representing the pattern (in this case it is the first two subtrees encountered when the tree is traversed in the depth-first-search order).

![Similar Subtrees Summary](https://bitbucket.org/Msgmaxim/cp-profiler2/raw/dec396e2537294be8cdf18b9594441ac710e937b/docs/images/doc_ss_analysis_hist.png)

Changing the configuration menu at the bottom of the window, the user can filter the list of patterns based on their *count* and *height* values.
They way the length of horizontal bars is determined and the sorting criteria can also be specified there.

Whenever a pattern on the left hand side is selected, the corresponding subtrees will be highlighted on the traditional visualisation by drawing their .
Additionally, if the option *Hide not selected* is selected (top of the window), the subtrees of t

![Similar Subtrees Highlighted](https://bitbucket.org/Msgmaxim/cp-profiler2/raw/dec396e2537294be8cdf18b9594441ac710e937b/docs/images/doc_ss_analysis.png)

**Elimination of subsumed patterns**

A pattern `P` is said to be subsumed by one or more patterns if subtrees of those patterns
contain all of the subtrees of `P` as descendants.

**Applying filters.**

Should filtered out patterns be allowed to subsume?

