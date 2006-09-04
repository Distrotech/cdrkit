/* test_BITFIELD_HTOL.c derived from cdrtools aclocal.m4 by Joerg Schilling */
/* Return 1 if bitfields are high-to-low, 0 if bitfields are low-to-high */
int main()
{
	union {
		unsigned char ch;
		struct { unsigned char bf1:4, bf2:4; } bf;
	} u;
	u.ch = 0x12;
	return (u.bf.bf1 == 1);
}
