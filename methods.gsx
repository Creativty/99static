<!-- Constant definition -->
<!-- gsx: core::define! consts::hello_world1	"Hello world" --> <!-- Legal -->
<!-- gsx: core::define! consts::hello_world2	core::rot13! "Hello world" --> <!-- Legal -->
<!-- gsx: core::define! consts::hello_world3	consts::hello_world2 --> <!-- Legal -->
<!-- gsx: core::define! consts::hello_world4	consts::hello_world1 "and something else" --> <!-- Illegal -->
<!-- gsx: core::define! 42						1337 --> <!-- Illegal -->
<!-- gsx: core::define! "string"				"Hello world" --> <!-- Illegal -->

<!-- Function definition -->
<!-- gsx: core::define! methods::rot13_static = "13" --> <!-- Legal -->
<!-- gsx: core::define! methods::rot13_concat string = core::concat! core::rot13! string "13" --> <!-- Legal -->

<!-- Function aliasing -->
<!-- gsx: core::define! methods::rot13c methods::rot13_concat --> <!-- Legal -->

<!-- Function pattern matching -->
<!-- gsx: core::define! methods::upper [] = [] -->
<!-- gsx: core::define! methods::upper [x] = core::if! core::contains! x 'a'..='z' core::char! core::add! core::int! x 32 core::else! x -->
<!-- gsx: core::define! methods::upper [x:xs] = core::concat! methods::upper! x methods::upper! xs -->
