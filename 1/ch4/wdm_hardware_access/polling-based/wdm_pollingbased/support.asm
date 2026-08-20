PUBLIC gen_int

_DATA SEGMENT
_DATA ENDS
 
_TEXT	SEGMENT

gen_int PROC
	int 50h
	ret
gen_int ENDP

_TEXT	ENDS

END
