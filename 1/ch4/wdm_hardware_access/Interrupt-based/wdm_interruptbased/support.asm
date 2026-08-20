PUBLIC gen_int

_DATA SEGMENT
_DATA ENDS
 
_TEXT	SEGMENT

gen_int PROC	; ECX = vector(50h - 80h)
	cmp cx, 50h
	je isr_50
	cmp cx, 60h
	je isr_60
	cmp cx, 70h
	je isr_70
	cmp cx, 80h
	je isr_80
	ret

isr_50:
	int 50h
	ret

isr_60:
	int 60h
	ret

isr_70:
	int 70h
	ret

isr_80:
	int 80h
	ret

gen_int ENDP

_TEXT	ENDS

END
