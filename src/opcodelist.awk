#!/usr/bin/awk -f

BEGIN {
	print "#include \"virginian.h\"\n"
	print "const char *virg_opstring(int op) {"
	print "\tstatic const char* ops[] = {"
}

/OP_/ {
	print "\t\t/* " $3 " */   \"" substr($1, 4) "\","
}

END {
	print "\t\"\"};"
	print "\treturn ops[op];}\n"
}

