typedef struct line line;

struct line {
	int ptr1, ptr2, length;
	line *prev, *next;
	char *text;
};

void
del_char(line *l) {
	if (l->ptr1 > 0) l->ptr1--;
}

void
ins_char(line *l, char c){
	if (l->ptr1 == l->ptr2){
		l->length += TEXT_WIDTH;
		l->text = realloc(l->text, l->length);
		l->ptr2 += TEXT_WIDTH;
		int i;
		for(i=l->ptr2; i < l->length; i++){
			l->text[i] = l->text[i-TEXT_WIDTH];
		}
	}
	l->text[l->ptr1++] = c;
}

int
ptr_lr(line *l, int left){
	if (left){
		if(l->ptr1 > 0){
			l->text[--(l->ptr2)] = l->text[--(l->ptr1)];
			return 1;
		}
	} else {
		if(l->ptr2 < l->length){
			l->text[++(l->ptr1)] = l->text[++(l->ptr2)];
			return 1;
		}
	}
	return 0;
}

void
print_line
{
	; /* | >>> | <   > */
}
