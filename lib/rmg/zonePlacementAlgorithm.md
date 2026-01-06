### The Algorithm

For each zone (processed in ID order):

1.  **Identify Anchors**: Look at the zone's connections. Identify which connected zones have **already been placed** on the grid. Call these "Anchors".

2.  **Determine Placement Strategy**:
    *   **Case A: No Anchors** (The zone is the first of its cluster, e.g., a Player Start).
        *   Use the existing logic: Place it in a random corner/edge or at the position *most distant* from existing zones. This ensures separate clusters (like different players) start far apart.
    *   **Case B: Has Anchors** (The zone connects to something we've already placed).
        *   We want to place this zone as close as possible to its anchors.

3.  **Find Best Spot (for Case B)**:
    *   **Generate Candidate Spots**: For every Anchor, identify the "perfect" grid positions for the new zone:
        *   **Same Level**: The 4 grid cells immediately adjacent (Up, Down, Left, Right) to the Anchor.
        *   **Different Level**: The grid cell at the *exact same* (x,y) coordinate as the Anchor (overlapping).
    *   **Score Candidates**:
        *   Filter out spots that are already occupied or out of bounds.
        *   For each remaining candidate spot, count how many Anchors it touches.
        *   **Select the spot with the highest count.** (e.g., if a spot touches 2 existing neighbors, it's better than a spot touching only 1).
    *   **Fallback**: If all "perfect" spots are blocked (the anchors are completely surrounded), we evaluate all free cells on the grid and select the one that minimizes the **sum of Euclidean distances** to all anchors. 
        *   *Rationale*: By optimizing for the sum of distances (Geometric Median) rather than the Center of Mass, we ensure the new zone is placed close to *some* actual anchors, rather than being stranded in an empty middle space where it touches none (which happens with Center of Mass if anchors are spread out).

### Why this works
*   **Constructive**: It builds highly connected clusters step-by-step.
*   **Adjacency**: It explicitly forces connected zones to be grid neighbors (which translates to map adjacency).
*   **Verticality**: It naturally handles the surface/underground alignment by prioritizing overlapping coordinates for vertical connections.
*   **Order Invariant**: It works with the fixed ID iteration order by always looking "backward" at what is already there.