<!-- gsx: core::define! methods::upper [] = [] -->
<!-- gsx: core::define! methods::upper [x] = core::if! x in 'a'..='z' core::char! core::add! core::int! x 32 core::else! x -->
<!-- gsx: core::define! methods::upper [x:xs] = core::concat! methods::upper! x methods::upper! xs -->
