import sys 
import os
from generic_lexer import Lexer

path = sys.argv[1]

with open(path, 'r') as f:
    content = f.read()

rules = {
    "VARIABLE": r"[a-zA-Z_$]+[a-zA-Z0-9_$]*",
    "NUMBER": r"(0b)?[0-9]+",
    "SPACE": r"[ \n]",
    ":": r"\:",
    "?": r"\?",
    "<": r"\<",
    ">": r"\>",
    "(": r"\(",
    ")": r"\)",
    ";": r";",
    "{": r"\{",
    "}": r"\}",
    "=": r"=",
    "*": r"\*",
    ",": r",",
    "COMMENT": r"\#.*",
    "STRING": r"\".*\"",
}

out = []
for token in Lexer(rules, False, content):
    if not (token.name == "SPACE" or token.name == "COMMENT"):
        out.append(token)

def escape(name):
    keywords = ["volatile","for","while","do","break","continue","goto","static","inline","extern"]
    for kw in keywords:
        if name == kw:
            return "_" + name
    return name

def popty(ty):
    v = out.pop(0)
    if (v.name != ty):
        raise Exception("Expected " + ty + " got " + v.name)
    return v 

def until(ty):
    res = []
    while True:
        a = out.pop(0)
        if a.name == ty:
            break
        res.append(a)
    return res 

def bundle(tks):
    res = []
    colon = False 
    for tk in tks:
        if (tk.name == ":"):
            colon = True
        else:
            if colon:
                if isinstance(res[-1], list):
                    res[-1].append(tk)
                else: 
                    res[-1] = [res[-1], tk]
                colon = False 
            else:
                res.append(tk)
    return res

def chunk(tks, count):
    res = []
    while len(tks) > 0:
        x = [tks.pop(0) for _ in range(count)]
        res.append(x)
    return res

def immedv(tks):
    if not isinstance(tks, list):
        return [tks.val]

    res = []
    for tk in tks:
        if tk.name != ':':
            res.append(tk.val)
    return res

def parse(fileHeader, fileSource):
    ty = escape(popty("VARIABLE").val)
    if ty == "include":
        f = popty("STRING").val 
        fileHeader.write("#include " + f)
    else:
        name = escape(popty("VARIABLE").val)
        popty("(")

        args = []
        while out[0].name == "VARIABLE":
            n = popty("VARIABLE").val
            ty = until(";")
            args.append((n, ty))

        fileHeader.write("// Table " + name + "\n")
        
        fileHeader.write("typedef struct {\n\n")
        for arg in args:
            fileHeader.write("  struct {\n")
            enum = 'a'
            for ty in arg[1]:
                if ty.name == "?":
                    fileHeader.write("    bool set;\n")
                else:
                    x = ty.val
                    if ty.val == "byte":
                        x = "uint8_t"
                    elif ty.val == "str":
                        x = "const char *"
                    fileHeader.write("    " + x + " " + str(enum) + ";\n")
                    enum = chr(ord(enum) + 1)

            fileHeader.write("  } " + escape(arg[0]) + ";\n")
        fileHeader.write("} " + name + "__entry;\n\n")
            
        popty(")")

        entry_prefix = name + "_"
        if out[0].val == "enum_entry_prefix":
            out.pop(0)
            entry_prefix = out.pop(0).val[1:-1]

        popty("{")

        options = {}
        while out[0].val == "entry":
            popty("VARIABLE")
            nam = escape(popty("VARIABLE").val)
            try:
                values = {x[0].val: immedv(x[1]) for x in chunk(bundle(until(";")), 2)}
                for arg in args:
                    if arg[1][0].name != "?":
                        if arg[0] not in values:
                            raise Exception("non-optional field " + arg[0] + " not set")
                options[nam] = values
            except Exception as e:
                raise Exception("error in entry " + nam + ": " + str(e))

        popty("}")

        fileHeader.write("#define " + name + "__len (" + str(len(options)) + ")\n\n")
        fileHeader.write("extern " + name + "__entry " + name + "__entries[" + name + "__len];\n\n")

        fileHeader.write("typedef enum {\n")
        for i, key in enumerate(options):
            fileHeader.write("  " + entry_prefix + key + " = " + str(i) + ",\n")
        fileHeader.write("} " + name + ";\n")

        fileSource.write("// Table " + name + "\n\n")
        fileSource.write(name + "__entry " + name + "__entries[" + name + "__len] = {\n")

        for key in options:
            val = options[key]
            fileSource.write("  [" + entry_prefix + key + "] = (" + name + "__entry) {\n")
            for member in args:
                memberKey = member[0]
                memberTypes = member[1]
                if memberKey in val:
                    enum = 'a'
                    imm = val[memberKey]

                    if memberTypes[0].name == '?':
                        fileSource.write("    ." + escape(memberKey) + ".set = true,\n")

                    for x in imm:
                        fileSource.write("    ." + escape(memberKey) + "." + enum + " = " + x + ",\n")
                        enum = chr(ord(enum) + 1)
                else:
                    fileSource.write("    ." + escape(memberKey) + ".set = false,\n")

            fileSource.write("  },\n")

        fileSource.write("};\n\n");


with open("build/" + path + ".h", 'w') as f0:
    with open("build/" + path + ".c", 'w') as f1:
        f0.write("#include <stdint.h>\n#include <stdbool.h>\n\n")
        f1.write("#include \"" + os.path.basename(path) + ".h\"\n\n")
        while len(out) > 0:
            parse(f0, f1)
