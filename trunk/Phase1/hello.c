#include <stdio.h>
#include <stdint.h>

unsigned Global[123][45];

int main() {
  unsigned i;
  int x[55];
  x[13]=101;

  int y[9][21];
  y[8][20]=202;

  /* The following creates a getElementPointerConstantExpr without its
   * inbounds property set, as an operand to a store. How can this case
   * be detected?
   */
   Global[122][45]=0;

   int a = 54;
   int b = 25 + 15;
   Global[a][b]=303;

   /*int m = 4;
     int n = 3;
     int z[m][n];
     z[2][3]=303;*/

   // An illegal read access. This should halt program execution.
   y[0][0] = y[0][21];

   return 0;
}
