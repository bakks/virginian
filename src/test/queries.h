#define query_size 10
static const char *queries[] = {
	"select id, uniformi, normali5 from test where uniformi > 60 and normali5 < 0",
	"select id, uniformf, normalf5 from test where uniformf > 60.0 and normalf5 < 0.0",
	"select id, uniformi, normali5 from test where uniformi > -60 and normali5 < 5",
	"select id, uniformf, normalf5 from test where uniformf > -60.0 and normalf5 < 5.0",
	"select id, uniformi, normali20 from test where (normali20 + 40) > (uniformi - 10)",
	"select id, uniformf, normalf20 from test where (normalf20 + 40.0) > (uniformf - 10.0)",
	"select id, normali5, normali20 from test where normali5 * normali20 >= -5 and normali5 * normali20 <= 5",
	"select id, normalf5, normalf20 from test where normalf5 * normalf20 >= -5.0 and normalf5 * normalf20 <= 5.0",
	"select id, uniformi, normali5, normali20 from test where uniformi >= -1 and uniformi <= 1 or normali5 >= -1 and normali5 <= 1 or normali20 >= -1 and normali20 <= 1",
	"select id, uniformf, normalf5, normalf20 from test where uniformf >= -1.0 and uniformf <= 1.0 or normalf5 >= -1.0 and normalf5 <= 1.0 or normalf20 >= -1.0 and normalf20 <= 1.0",
};

