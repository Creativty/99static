<!doctype html> <!-- gsx: std/include! "methods.gsx" -->
<html>
	<head>
		<title><!-- gsx: std/replace! std/TITLE "html" "gsx" --></title>
	</head>
	<body>
		<!-- gsx: std/include! "header.gsx" -->
		<p><!-- gsx: "Hello world" --></p>
		<p><!-- gsx: -1337 --></p>
		<p>replace("Hello", "ll", "wart") = "<!-- gsx: std/replace! "Hello" "ll" "wart" -->"</p>
		<p>((2 * 3) + 4) / 5 = <!-- gsx: std/div! std/sum! std/mul! 2 3 4 5 --></p>
	</body>
</html>
