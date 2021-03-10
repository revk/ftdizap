ftdizap: ftdizap.c
	cc -O -o $@ $< -lpopt -lftdi1 -lusb-1.0
