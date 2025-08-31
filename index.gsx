<!-- gsx: core::include! "methods.gsx" -->
<!-- gsx: core::print! -->

<html>
	<head>
		<title><!-- gsx: core::env! "TITLE" --></title>
	</head>
	<body>
		<p><!-- gsx: core::print! core::xstr "Hello world" --></p>
		<!-- gsx: core::replace! core::include! "header.gsx" "header" methods::upper! "footer" -->
		<!-- gsx: core::factorial! core::add! core::mul! 2 1 1 -->
	</body>
</html>
