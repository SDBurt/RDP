make: rdpr.c rdps.c rdpFunctions.h rdpStruct.h
	gcc rdpFunctions.h rdpStruct.h rdpr.c -o rdpr
	gcc rdpFunctions.h rdpStruct.h rdps.c -o rdps -lm

rdpr: rdpr.c rdpFunctions.h rdpStruct.h
	gcc rdpFunctions.h rdpStruct.h rdpr.c -o rdpr

rdps: rdps.c rdpFunctions.c rdpStruct.c
	gcc rdpFunctions.h rdpStruct.h rdps.c -o rdps

clean:
	rm rdps
	rm rdpr
