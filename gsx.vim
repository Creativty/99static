" Vim syntax file
" Language:		gsx
" Maintainer:	Abderrahim Indjaren <aindjare@proton.me>
" Last Change:	2025 Aug 29

syntax clear

if exists("b:current_syntax")
	finish
endif

runtime!		syntax/html.vim
let				b:current_syntax = "gsx"

syntax case		match

syntax match	gsxScope		/\v\w+\ze::/ containedin=gsxComment
syntax match	gsxCall			/\v\w+!/ containedin=gsxComment
syntax match	gsxIdentifier	/\w\+\(!\|::\)\@!\>/ containedin=gsxComment

syntax match	gsxEquals		/=/ containedin=gsxComment

syntax match	gsxNumber		/\<[0-9]\+\>/ contained
syntax region	gsxString		start=+\%(L\|U\|u8\)\="+ skip=+\\\\\|\\"+ end=+"+ contained
syntax region	gsxComment		start=/<!--\s*gsx:\s*/ end='-->' contains=gsxString,gsxIdentifier,gsxCall,gsxScope,gsxNumber,gsxEquals oneline
syntax cluster	gsxNormal		contains=gsxEquals

syntax cluster	htmlPreProc		add=gsxComment
syntax cluster	htmlTop			add=gsxComment

highlight default link gsxComment		Comment
highlight default link gsxNumber		Number
highlight default link gsxString		String
highlight default link gsxCall			Function
highlight default link gsxScope			Special
highlight default link gsxIdentifier	Macro
highlight default link gsxNormal		Normal
