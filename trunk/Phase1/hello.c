#include <stdio.h>

unsigned Global[123][45];

int main() {
        unsigned i;
        int x[55];
	x[13]=101;
	
	int y[9][21];
	y[8][20]=202;

        Global[0][0]=0;

        int a = 54;
        int b = 25 + 15;
        Global[a][b]=303;


	/*int m = 4;
	int n = 3;
	int z[m][n];
	z[2][3]=303;*/
}
