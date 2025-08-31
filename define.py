import  os
import  sys

def generate_definition(collection, proc, builtin = None, params = []):
    params_address = f"NULL"
    if len(params) > 0:
        params_address = f"params_{proc}"
        print(f"static struct parameter\tparams_{proc}[] = {{")
        kinds = {
            'a': "PARAMETER_ATOM",
            'v': "PARAMETER_VARIABLE",
            'd': "PARAMTER_DESTRUCTURE",
        }
        for param in params:
            [kind_symbol, name] = param.split(":")
            kind = "PARAMETER_INVALID" if not kind_symbol in kinds else kinds[kind_symbol]
            print(f"\t{{ .kind = {kind}, .data = {{ .name = \"{name}\" }} }},")
        print(f"}};")
        print(f"")

    is_builtin = builtin is not None
    address = [
        ".address = {",
        "\t" + (f".builtin = {'NULL' if builtin is None else '(void *)' + builtin}," if collection == "core" else ".ast = NULL,"),
        "},",
    ]
    print(f"static struct definition\tdef_{proc} = {{")
    print(f"\t.span = \"{collection}::{proc}\",")
    print(f"\t.params = {params_address},")
    print(f"\t.params_count = {len(params)},")
    print(f"\t")
    print(f"\t.is_builtin = {'true' if is_builtin else 'false'},")
    for line in address:
        print(f"\t{line}")
    print(f"}};")

    print(f"gsx_register_definition(ctx, def_{proc});")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"99static::define.py: Error: Invalid parameter count.", file = sys.stderr)
        exit(1)

    token = sys.argv[1]
    params = [] if len(sys.argv) <= 3 else sys.argv[3:]
    builtin = None if len(sys.argv) < 3 else sys.argv[2]
    scopes = token.split("::")
    if len(scopes) != 2 or len(scopes[0]) == 0 or len(scopes[1]) == 0:
        print(f"99static::define.py: Error: Invalid procedure name {arg}", file = sys.stderr)
        exit(1)
    if any([len(param.split(":")) < 2 for param in params]):
        print(f"99static::define.py: Error: Invalid parameters {params}", file = sys.stderr)
        exit(1)
    generate_definition(*scopes, builtin, params)
