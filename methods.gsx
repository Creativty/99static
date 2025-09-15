<!-- gsx: std/define! procs/debug std/debug -->
<!-- gsx: std/define! consts/number 126 -->
<!-- gsx: std/define! consts/string "one" -->
<!-- gsx: std/define! consts/result std/div! consts/number 3 -->
<!-- gsx: std/define! consts/array [42, std/mul! 21 2, consts/result] -->
<!-- gsx: std/define! procs/debug_example () std/debug! "123" -->
<!-- gsx: std/define! procs/debug_string () std/debug! consts/string -->
<!-- gsx: std/define! procs/debug_arg (arg) std/debug! arg -->
<!-- gsx: std/define! procs/debug_sum (lhs, rhs) std/debug! std/sum! lhs rhs -->
<!-- gsx: std/define! procs/rot_13 (arg) std/mod! std/sum! arg 13 26 -->
