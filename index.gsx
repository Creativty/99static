<!doctype html><!-- <!-- gsx: core::include! "methods.gsx" -->
<html>
	<head>
		<title><!-- gsx: TITLE --></title>
	</head>
	<body><!-- gsx: core::debug! "Hello world" -->
		<!-- gsx: core::include! "header.gsx" -->
		<p><!-- gsx: "Hello world" --></p>
		<p><!-- gsx: -9223372036854775807 --></p>
		<p>replace("Hello", "ll", "wart") = "<!-- gsx: core::replace! "Hello" "ll" "wart" -->"</p>
		<p>((2 * 3) + 4) / 5 = <!-- gsx: core::div! core::add! core::mul! 2 3 4 5 --></p>
	</body>
</html>
