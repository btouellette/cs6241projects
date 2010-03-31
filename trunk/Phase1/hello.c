#include <stdio.h>

const char *g_s = "This is not really an array.";
unsigned g_a[123][45];

int main() {
        unsigned i;
        int x[55];
	x[13]=101;
	
	int y[9][21];
	y[8][20]=202;

        g_a[54][40]=303;

	/*int m = 4;
	int n = 3;
	int z[m][n];
	z[2][3]=303;*/
}
