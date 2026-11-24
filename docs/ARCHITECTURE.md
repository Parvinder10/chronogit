# ChronoGit — Architecture & System Design

ChronoGit is a lightweight, high-performance local version control engine built from scratch in C++20. It implements content-addressable storage, immutable snapshots, directed acyclic commit graphs, branch management, Myers line-level diffing, three-way merge reconciliation, repository integrity verification, and mark-and-sweep garbage collection.

---

## 1. High-Level Architecture Overview

ChronoGit strictly separates storage, tree construction, staging, branch references, diff computation, graph merging, and user interface.

```mermaid
flowchart TD
    User([Developer / CLI / Web UI]) --> CLI[CLI App & Command Parsers]
    CLI --> Repo[Repository Facade]

    subgraph Working State
        WS[Workspace Scanner & Ignore Rules]
        IDX[Index / Staging Area (.chronogit/index)]
    end

    subgraph Storage Subsystem
        TB[Tree Builder Trie]
        CAS[Object Database (.chronogit/objects)]
        COMP[LZ77 Compressor / Decompressor]
        HASH[SHA-256 Cryptographic Hasher]
        ATOM[Atomic File Replacer]
    end

    subgraph Graph & Versioning Engines
        BM[Branch Manager (.chronogit/refs)]
        CO[Checkout Engine & Safety Guard]
        DIFF[Myers Line Diff Engine]
        LCA[Lowest Common Ancestor Finder]
        M3[Three-Way File & Tree Merge Engine]
    end

    subgraph Maintenance
        FSCK[Fsck Integrity Checker]
        GC[Mark-and-Sweep Garbage Collector]
    end

    Repo --> WS
    Repo --> IDX
    Repo --> BM
    Repo --> CO
    Repo --> CAS
    Repo --> DIFF
    Repo --> M3
    Repo --> FSCK
    Repo --> GC

    TB --> CAS
    CAS --> COMP
    CAS --> HASH
    CAS --> ATOM
```

---

## 2. Subsystem Breakdown

### 2.1 Content-Addressable Storage (CAS)
- **Object Integrity**: Every object is identified strictly by the SHA-256 hash of its canonical representation: `"<type> <size>\0<payload>"`.
- **Directory Sharding**: Objects are stored under `.chronogit/objects/ab/cdef1234...` (the first 2 hex digits form the shard directory, and the remaining 62 characters form the filename). This prevents filesystem slowdowns in directories containing thousands of objects.
- **Deduplication**: When an object is written, if an object with the same hash already exists, writing is skipped. Identical file contents across different commits, directories, or branches share a single compressed object on disk.
- **Built-in LZ77 Compression**: Objects are compressed before being persisted, achieving a 40–80% reduction in disk footprint compared to raw snapshots.

### 2.2 Staging Area (Index)
- Acts as the intermediate transaction log between the working directory and the commit database.
- Tracks relative file paths, Blob SHA-256 hashes, file modes (`0100644` regular, `0100755` executable), file sizes, and microsecond timestamps (`mtime`).
- Enables fast status checks: if size and mtime are unchanged, ChronoGit skips reading file contents from disk.
- Written using atomic replacement (`AtomicFileWriter`) to guarantee that process termination never leaves a half-written index.

### 2.3 Tree Builder & Hierarchical Tree Generation
- Converts flat sorted index paths (e.g. `src/net/socket.cpp`) into a hierarchical tree graph.
- Generates subtrees for nested directories (`net/` tree referencing `socket.cpp` blob; `src/` tree referencing `net/` subtree; root tree referencing `src/`).
- Because trees are also content-addressable objects, subtrees that have not changed across commits retain their exact hashes and are reused instantly without allocation or disk writes.

### 2.4 Branch Manager & HEAD State
- References are stored under `.chronogit/refs/heads/<branch_name>`.
- The `.chronogit/HEAD` file stores either:
  1. A symbolic reference: `ref: refs/heads/main`
  2. A direct commit hash: `7f9a12...` (Detached HEAD inspection)
- Switching branches is protected: if the working tree has uncommitted local modifications or untracked files that would be overwritten by switching, the operation aborts before modifying any files.

### 2.5 Myers Diff Engine
- Implements Eugene Myers' $O(ND)$ Difference Algorithm ("An $O(ND)$ Difference Algorithm and Its Variations", 1986).
- Operates on vectors of lines, discovering the Shortest Edit Script (SES) and Longest Common Subsequence (LCS).
- Generates unified diff hunks with configurable context lines (default 3) and standard hunk headers (`@@ -old_start,old_count +new_start,new_count @@`).
- Supports diffing:
  - Working directory vs Index (unstaged changes)
  - Index vs HEAD (staged changes)
  - Commit vs Commit

### 2.6 Merge Engine & Three-Way Reconciliation
- **Lowest Common Ancestor (LCA)**: Traverses commit parent pointers via Breadth-First Search (BFS) to find the closest common ancestor commit in arbitrary DAG topologies (including branching and diamond merge patterns).
- **Fast-Forward Detection**: If the LCA is the current HEAD commit, ChronoGit simply advances the branch reference to the target commit without creating an artificial merge commit.
- **Three-Way Merge (`diff3`)**:
  - Compares Base ($B$), Ours ($O$), and Theirs ($T$) simultaneously.
  - Automatically reconciles clean, non-overlapping modifications made on both branches.
  - Detects overlapping conflicting edits and writes clear conflict markers:
    ```
    <<<<<<< HEAD
    [Local Branch Version]
    =======
    [Incoming Branch Version]
    >>>>>>> [Target Branch]
    ```
  - Writes `.chronogit/MERGE_HEAD` and preserves conflict state until resolved and committed.

### 2.7 Recovery & Integrity Subsystem (`fsck`)
- Verifies that every object on disk decompresses properly and that its SHA-256 hash matches its filename address.
- Parses every Tree and Commit object to verify that all referenced child trees, blobs, and parent commits exist and are reachable.
- Detects dangling objects (objects not reachable from any ref).

### 2.8 Garbage Collection (`gc`)
- Implements mark-and-sweep reachability analysis.
- Starts from all branch heads, HEAD, and current Index entries.
- Traverses commit trees and parent chains, marking all reachable objects.
- Sweeps unreachable objects, reporting count and bytes freed, with full `--dry-run` safety mode.

---

## 3. Data Safety & Atomic File Operations

To prevent data corruption during power loss, process termination, or disk errors:
1. **Atomic File Write Protocol**:
   - Data is written to a temporary sibling file in the same directory: `<target>.tmp.<pid>.<rand>`.
   - The stream is flushed and closed.
   - On Windows: `MoveFileExW(temp, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` atomically replaces the destination.
   - On POSIX: `std::filesystem::rename` provides atomic replacement.
2. **Immutable Objects**:
   - Once written, files in `objects/` are never overwritten or modified in-place; they are immutable.
3. **Repository Path Traversal Protection**:
   - All relative paths are normalized and checked to prevent path traversal attacks (`..` or absolute drive specifications) from modifying files outside the repository root.
