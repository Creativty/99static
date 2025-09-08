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

syntax keyword	gsxKeyword		in

syntax match	gsxScope		/\v\w+\ze::/ containedin=gsxComment
syntax match	gsxCall			/\v\w+!/ containedin=gsxComment
syntax match	gsxIdentifier	/\w\+\(!\|::\)\@!\>/ containedin=gsxComment

syntax match	gsxParamsSeparator		/=/ containedin=gsxComment

syntax match	gsxArrayOpen			/\[/ containedin=gsxArray
syntax match	gsxArrayClose			/\]/ containedin=gsxArray
syntax match	gsxArraySeparator		/,/ containedin=gsxArray
syntax match	gsxDestructureSeparator	/:/ containedin=gsxArray
syntax region	gsxArray				start="\[*" end="*\]" oneline

syntax match	gsxNumber		/\<[0-9]\+\>/ contained
syntax region	gsxChar			start=+\%(L\|U\|u8\)\='+ skip=+\\\\\|\\'+ end=+'+ oneline contained
syntax region	gsxString		start=+\%(L\|U\|u8\)\="+ skip=+\\\\\|\\"+ end=+"+ oneline contained
syntax region	gsxComment		start=/<!--\s*gsx:\s*/ end='-->' contains=gsxKeyword,gsxChar,gsxString,gsxIdentifier,gsxCall,gsxScope,gsxNumber,gsxParamsSeparator,gsxArraySeparator,gsxArray oneline
syntax cluster	gsxNormal		contains=gsxParamsSeparator,gsxArraySeparator
syntax cluster	gsxArraySpecial	contains=gsxArrayOpen,gsxArrayClose

syntax cluster	htmlPreProc		add=gsxComment
syntax cluster	htmlTop			add=gsxComment

highlight default link gsxComment				Comment
highlight default link gsxNumber				Number
highlight default link gsxString				String
highlight default link gsxChar					Character
highlight default link gsxCall					Function
highlight default link gsxScope					Special
highlight default link gsxKeyword				Special
highlight default link gsxIdentifier			Macro
highlight default link gsxNormal				Normal
highlight default link gsxArraySpecial			SpecialComment
