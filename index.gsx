<!doctype html><!-- <!-- gsx: std/include! "methods.gsx" -->
<html>
	<head>
		<title><!-- gsx: std/TITLE --></title>
	</head>
	<body><!-- gsx: std/debug! "this is a debug message!" -->
		<!-- gsx: std/include! "header.gsx" -->
		<p><!-- gsx: "Hello world" --></p>
		<p><!-- gsx: -1337 --></p>
		<p>replace("Hello", "ll", "wart") = "<!-- gsx: std/replace! "Hello" "ll" "wart" -->"</p>
		<p>((2 * 3) + 4) / 5 = <!-- gsx: std/div! std/add! std/mul! 2 3 4 5 --></p>
	</body>
</html>
