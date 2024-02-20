import json
import numpy as np
import pathlib

def backspace_json(data):
    olen = len(data)
    while data:
        plen = len(data)
        print(len(data))
        try:
            rval = json.loads(data)
            break
        except:
            delta = 0
            if data[0] == '[' and data[-1] == ']':
                data = data[:-1]
                delta -= 1
            data = data[:-1]
            delta -= 1
            if data[0] == '[' and data[-1] != ']':
                if plen == olen:
                    data = data[:-1]
                    delta -= 1
                data += ']'
                delta += 1
            if len(data) == plen:
                raise ValueError("\n".join(["Infinite loop",str(delta), data[:100],data[-100:]]))
    if not data:
        raise ValueError("Couldn't salvage it")
    return rval

def incomplete_json(data):
    try:
        return json.loads(data)
    except:
        # Structure breakpoints
        seek_rbrace = []
        seek_rbrack = []
        seek_rquote = []
        index = 0
        if data[-1] == ',':
            data = data[:-1]
        while index < len(data):
            if data[index] == '{':
                #print("Looking for '}' to match: ", data[:index], "-->", data[index:])
                seek_rbrace.append(index)
            elif data[index] == '}':
                #print("Found '}' to match: ", data[seek_rbrace[-1]:index+1])
                seek_rbrace.pop(-1)
            elif data[index] == '[':
                #print("Looking for ']' to match: ", data[:index], "-->", data[index:])
                seek_rbrack.append(index)
            elif data[index] == ']':
                #print("Found ']' to match: ", data[seek_rbrack[-1]:index+1])
                seek_rbrack.pop(-1)
            elif data[index] == '"':
                if len(seek_rquote) > 0:
                    seek_rquote.pop()
                else:
                    seek_rquote.append(index)
            index += 1
        # Identify suffix to add
        visit = np.asarray(seek_rbrace + seek_rbrack + seek_rquote)
        suffix = np.asarray(list('}'*len(seek_rbrace) + ']'*len(seek_rbrack) + '"'*len(seek_rquote)))
        order = np.argsort(-1 * visit)
        suffix_str = "".join(suffix[order])

        # There might be cases where this still fails, TBD
        clipped_json = json.loads(data+suffix_str)
        return clipped_json

def load_incomplete_json(fname, n=0, to=-1, via=incomplete_json):
    with open(fname, 'r') as f:
        contents = "".join([_ for idx, _ in enumerate(f.readlines()) if idx >= n and (to==-1 or idx < to)])
    return via(contents)

if __name__ == "__main__":
    import sys
    if len(sys.argv) == 1:
        test_battery = [
            ("EXPECT OK", '[{"a":1,"b":2,"c":[1,2,3]},{"a":2,"c":[4,{"d":2}]}]',),
            ("EXPECT FIXABLE", '[{"a":1,"b":2,"c":[1,2,3]},{"a":2,',),
            ("EXPECT FIXABLE", '[{"test": [{"a": "abc',),
            ("EXPECT UNFIXABLE", '{["cannot_use_list_as_key"]}',),
        ]
        for (expectation, test) in test_battery:
            try:
                ok = json.loads(test)
                print(f"{expectation}: VALID JSON: {ok}")
            except:
                try:
                    fixed = incomplete_json(test)
                    print(f"{expectation}: FIXED JSON: {test} --> {fixed}")
                except:
                    print(f"{expectation}: Unable to fix JSON: {test}")
    else:
        for name in sys.argv[1:]:
            ok = load_incomplete_json(name, n=43823, via=backspace_json)
            pname = pathlib.Path(name)
            with open(pname.with_name(pname.stem+"_fixed"+pname.suffix),"w") as f:
                json.dump(ok, f)
