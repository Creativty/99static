<!doctype html>
<!-- gsx: std/include! "methods.gsx" -->
<html>
	<head>
		<title><!-- gsx: std/replace! std/TITLE "html" "gsx" --></title>
	</head>
	<body>
		<!-- gsx: std/include! "header.gsx" -->
		<!-- gsx: std/define! select_id "my_select" -->
		<p>Answer: <!-- gsx: std/mul! std/sum! 20 1 2 --></p>
	</body>
</html>

<!-- IDEAS(xenobas) -->
<!-- loop construct -->
<!-- html attribute support -->
<!-- disallow infinite recursion in include -->
<!-- maybe a std/expose! that merges the definitions, where by default std/include! creates a scope and doesn't pollute global definitions -->

