import sys

# example signature:
#   foo__impl = void foo(int num, unsigned long long id, kv char const* name, kv char const* pw, opt SHOW_NAME, opt SHOW_PASS)
#
# generates a macro that can be called like:
#   foo(1, 2)
#   foo(1, 2, name = "hi")
#   foo(1, 2, name = "hi", SHOW_NAME)
#   foo(1, 2, pass="y", name = "hi", SHOW_NAME, SHOW_PASS)
#   foo(1, 2, SHOW_NAME)

def splitlast(x, sep):
    where = x.rfind(sep)
    return x[:where], x[where:]

sig = None
if len(sys.argv) > 1:
    sig = sys.argv[1]
else:
    sig = input()
backing_fn, sig = (x.strip() for x in sig.split("="))

fn_name, sig = sig.split("(")
sig = sig.split(")")[0]
fn_ret, fn_name = (x.strip() for x in splitlast(fn_name, " "))

req_args = []
kv_args  = []
opt_args = []
auto_bool = False

for arg in sig.split(","):
    arg = arg.strip()
    arg, name = (x.strip() for x in splitlast(arg, " "))

    if arg.startswith("kv"):
        arg = arg[2:].strip()
        kv_args.append((name, arg))
    elif arg.startswith("opt"):
        arg = arg[3:].strip()
        if len(arg) == 0:
            arg = "bool"
            auto_bool = True
        opt_args.append((name, arg))
    else:
        req_args.append((name, arg))

prefix = f"__{fn_name}__"

if auto_bool:
    print("#include <stdbool.h>")

print(f"""
#ifndef {prefix}
#define {prefix}

#define {prefix}PASTE2(A,B) A ## B
#define {prefix}CAT2(A,B) {prefix}PASTE2(A,B)
#define {prefix}EMPTY()   /**/
#define {prefix}DELAY(F)  F {prefix}EMPTY()
""")

for name, ty in kv_args:
    print("#define "+prefix+"_"+name+" "+name+".have=1, ."+name+".value.have")

for name, ty in opt_args:
    print("#define "+prefix+"_"+name+" "+name+"=1")

print(f"""
struct {prefix}argt
{{""")

for name, ty in req_args:
    print(f"  {ty} {name};")

for name, ty in kv_args:
    print("  struct { int have; union { "+ty+" have; char _none; } value; } "+name+";")

for name, ty in opt_args:
    print(f"  {ty} {name};")

print("};")

print("")
print(fn_ret+" "+backing_fn+" (struct "+prefix+"argt"+" a);")
print("")

print(f"#define {fn_name}({", ".join(f"_in_{x}" for x,_ in req_args)}, ...) {backing_fn}((struct {prefix}argt){{ {", ".join(f".{x}=(_in_{x})" for x,_ in req_args)} {prefix}CAT2({prefix}ATTRS, __VA_OPT__(1))(__VA_ARGS__) }})")
print()
print(f"#define {prefix}ATTRS() /**/")
for i in range(len(opt_args) + len(kv_args)):
    print(f"#define {prefix}ATTRS{i+1}(attr, ...)  ,. {prefix}DELAY({prefix}CAT2({prefix}_, attr)) {prefix}CAT2({prefix}ATTRS, __VA_OPT__({i+2}))(__VA_ARGS__)")

print("\n#endif")
