"""
    Builds a nearly-arbitrarily-long graph for EMOGI
"""
import pathlib
import struct
import argparse
import numpy as np
import tqdm
import multiprocessing

def build():
    prs = argparse.ArgumentParser()
    prs.add_argument("--output", required=True, help="Base path and name (without .bel* extension) to output dataset to")
    prs.add_argument("--pattern", choices=['linked-list','branch','fork-join','self-loop'], default='linked-list',
                     help="Graph pattern to construct (default: %(default)s)")
    prs.add_argument("-n", "--n-nodes", dest='n', metavar='n', default=256, type=int,
                     help="Number of nodes in the graph (default %(default)s)")
    prs.add_argument("-w", "--width", dest='w', metavar='w', default=8, type=int,
                     help="Width or branching factor of the graph (default %(default)s)")
    return prs

def parse(args=None, prs=None):
    if prs is None:
        prs = build()
    if args is None:
        args = prs.parse_args()
    # Minimum sanity we can guarantee for all graphs
    assert args.w >= 1 and args.n > (1+args.w), "Graphs must have width >= 1 and number of nodes >= 1 + width"
    return args

class csr_exporter():
    def __init__(self, path, n):
        self.header_added = False
        self.dst = path.with_suffix('.bel.dst')
        self.dst_tmp = self.dst.with_suffix('.dst.tmp')
        self.dst_handle = None
        self.col = path.with_suffix('.bel.col')
        self.col_tmp = self.col.with_suffix('.col.tmp')
        self.col_handle = None
        self.iterable = tqdm.tqdm(total=n)
        #self.lock = multiprocessing.Manager().Lock()

    def __enter__(self):
        self.dst_handle = open(self.dst_tmp,'wb')
        self.col_handle = open(self.col_tmp,'wb')
        self.written = 0
        return self

    def export_edge(self, edge):
        #with self.lock:
        self.dst_handle.write(struct.pack('q', edge[0]))
        self.col_handle.write(struct.pack('q', edge[1]))
        self.written += 1
        self.iterable.update(1)

    def get_header(self):
        return struct.pack('qq', self.written, 0)

    def __exit__(self, exc_type, exc_value, traceback):
        if self.dst_handle is not None:
            self.dst_handle.close()
        if self.col_handle is not None:
            self.col_handle.close()
        # Put headers in place after the fact -- rewrites the file but should be easier on memory than
        # accumulating all the python objects forever
        if not self.header_added:
            header = self.get_header()
            with open(self.dst_tmp,'rb') as old, open(self.dst,'wb') as new:
                new.write(header) # Header
                new.write(old.read()) # Contents
            with open(self.col_tmp,'rb') as old, open(self.col,'wb') as new:
                new.write(header) # Header
                new.write(old.read()) # Contents
            self.header_added = True

#def fill_ll_tail(exporter, root, length):
#    for i in range(1, length+1):
#        exporter.export_edge((root+i-1, root+i))

def linked_list(args, exporter):
    """
        Writes a single-branch linked list with width == args.w and total number of nodes args.n
        Some example graphs:
            1) args.n = 4, args.w = 1
                0  Graph's head
                V
                1  LL 1's head
                V
                2  LL 1's 1st elem
                V
                3  LL 1's 2nd elem

            2) args.n = 12, args.w = 2
                         0 Graph's head
                         V
                     +-------+
                     V       V
        LL 1's Head  1       7  LL 2's Head
                     V       V
    LL 1's 1st elem  2       8  LL 2's 1st elem
                     V       V
    LL 1's 2nd elem  3       9  LL 2's 2nd elem
                     V       V
    LL 1's 3rd elem  4      10  LL 2's 3rd elem
                     V       V
    LL 1's 4th elem  5      11  LL 2's 4th elem
                     V
    LL 1's 5th elem  6

            3) args.n = 12, args.w = 4
                        0
                        V
                  +---+---+---+
                  V   V   V   V
                  1   4   7  10
                  V   V   V   V
                  2   5   8  11
                  V   V   V
                  3   6   9
    """
    graph_head_nodes = args.w + 1 # W lists connected to 1 head node
    ll_tail_nodes = args.n - graph_head_nodes # Remaining nodes
    ll_tail_cleanup = ll_tail_nodes % args.w # Number of linked lists with +1 element to use full budget
    ll_tail_length = [(ll_tail_nodes // args.w) + int(_ < ll_tail_cleanup) for _ in range(args.w)] # Length of each tail

    # Top of the graph, draw edges from head to linked list starts
    head_list = [(0,extent+1+sum(ll_tail_length[:extent])) for extent in range(args.w)]
    for edge in head_list:
        exporter.export_edge(edge)
    # Add each tail
    for (head,tail_root), tail_length in zip(head_list, ll_tail_length):
        #with multiprocessing.Pool() as pool:
        #    result_queue = []
        #    for i in range(1, tail_length+1):
        #        args = (exporter, tail_root, i)
        #        result_queue.append(pool.apply_async(fill_ll_tail, args))
        #    for _ in result_queue:
        #        _.get()
        for i in range(1, tail_length+1):
            exporter.export_edge((tail_root+i-1, tail_root+i))

def branch(args, exporter):
    """
        Writes a single-branch linked list with width == args.w and total number of nodes args.n
        Some example graphs:
            1) args.n = 7, args.w = 2
                 0
                 V
              +-----+
              V     V
              1     2
              V     V
            +---+ +---+
            3   4 5   6

            2) args.n = 14, args.w = 3
                             0
                             V
                  +----------+----------+
                  V          V          V
                  1          2          3
              +---+---+  +---+---+  +---+---+
              V   V   V  V   V   V  V   V   V
              4   5   6  7   8   9  10  11  12
              V
              13
    """
    queue = [0] # Queue head node for incremental construction
    next_queue = []
    next_insert = 1
    while next_insert < args.n: # Always one more vertex than edges, but we write graph as edges
        for parent in queue:
            for w in range(args.w):
                exporter.export_edge((parent,next_insert))
                next_queue.append(next_insert)
                next_insert += 1
                if next_insert >= args.n:
                    break
            # Else-continue-break used here to have the break above halt both loops
            else:
                continue
            break
        # Move to next depth in the graph
        queue = next_queue
        next_queue = []

def fork_join(args, exporter):
    """
        Writes a fixed-width fork, then join with width == args.w and total number of nodes args.n
        Some example graphs:
            1) args.n = 11, args.w = 2
                 0
                 V
              +-----+
              V     V
              1     2
              +-----+
                 V
                 3
                 V
              +-----+
              V     V
              4     5
              V     V
              +-----+
                 V
                 6
                 V
              +-----+
              V     V
              8     9
              V     V
              +-----+
                 V
                 10

            2) args.n = 12, args.w = 6
                  0
                  V
          +--+--+--+--+--+
          V  V  V  V  V  V
          1  2  3  4  5  6
          V  V  V  V  V  V
          +--+--+--+--+--+
                  V
                  7
                  V
          +--+--+--+--+
          V  V  V  V  V
          8  9  10 11 12
    """
    sync_points = [0]
    while sync_points[-1] < args.n:
        sync_points.append(sync_points[-1] + args.w+1)

    for sync_from, sync_to in zip(sync_points, sync_points[1:]):
        limit = args.w+1
        if sync_from+limit-1 >= args.n:
            limit -= (sync_from+limit-1-args.n)
        #edge_list.extend([(sync_from, sync_from+i) for i in range(1,limit)])
        for i in range(1, limit):
            exporter.export_edge((sync_from, sync_from+i))
        # Last entry will be >= n and should not be synced
        if sync_to < args.n:
            #edge_list.extend([(sync_from+i, sync_to) for i in range(1,limit)])
            for i in range(1,limit):
                exporter.export_edge((sync_from+i, sync_to))

def self_loop(args, exporter):
    """
        A chain of N self-connected vertices (no edges between distinct vertices)
        Example graphs:
            1) args.n = 3
        0-+    1-+    2-+
        ^ |    ^ |    ^ |
        +-+    +-+    +-+
    """
    for i in range(args.n):
        exporter.export_edge((i,i))

def main(args=None):
    args = parse(args)
    output = pathlib.Path(args.output)

    with csr_exporter(output, args.n) as exporter:
        if args.pattern == 'linked-list':
            linked_list(args, exporter)
        elif args.pattern == 'branch':
            branch(args, exporter)
        elif args.pattern == 'fork-join':
            fork_join(args, exporter)
        elif args.pattern == 'self-loop':
            self_loop(args, exporter)
        else:
            raise NotImplemented(f"Need implementation for {args.pattern}")
        header = exporter.get_header()
    """
    header, dst_data, col_data = csr_graph(edge_list)

    # Write to binary files
    with open(output.with_suffix('.bel.dst'), 'wb') as f_dst:
        f_dst.write(header + dst_data)

    with open(output.with_suffix('.bel.col'), 'wb') as f_col:
        f_col.write(header + col_data)
    """
    val_data = b''  # Assuming unweighted graph
    with open(output.with_suffix('.bel.val'), 'wb') as f_val:
        f_val.write(header + val_data)

if __name__ == '__main__':
    main()

