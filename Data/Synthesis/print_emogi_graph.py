import struct
import pathlib
import scipy.sparse
import argparse

prs = argparse.ArgumentParser()
prs.add_argument("--read", nargs="+", default=None, help="Files to read (just give the .bel, not with bonus extensions)")
prs.add_argument("--dense", action="store_true", help="Display dense adjacency matrix (default %(default)s)")
args = prs.parse_args()
args.read = [pathlib.Path(a) for a in args.read]

def read_header(path, suffix):
    with open(path.with_suffix(f'.bel.{suffix}'),'rb') as data:
        readdata = data.read()
    header = struct.unpack('qq', readdata[:16])
    print("Header :", header)
    return header

def read(path, suffix):
    with open(path.with_suffix(f'.bel.{suffix}'),'rb') as data:
        readdata = data.read()
    n_elem = len(readdata)
    data = struct.unpack('q'*(n_elem//8), readdata)[2:]
    print(suffix, ":", data)
    return data


for fname in args.read:
    if fname.suffix != '.bel':
        raise ValueError("--read argument must end with .bel")
    header = read_header(fname, 'dst')
    rows = read(fname, 'dst')
    cols = read(fname, 'col')
    # Actually need this many elems
    max_range = max(max(rows),max(cols))
    vals = list(range(1,max_range+1)) + [_ for _ in range(max_range, header[0])]
    print(vals)
    if args.dense:
        adj = scipy.sparse.csr_matrix((vals, (rows,cols)), shape=(max_range+1,max_range+1)).toarray()
        for row in adj:
            print(", ".join([str(_) for _ in row]))

