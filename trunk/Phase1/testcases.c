unsigned Global[123][45];

int main(int argc, char** argv) {
  unsigned i;
  int x[55][10];
  x[13][1]=101;

  int y[9][21];
  y[8][20]=202;

  /* The following creates a getElementPointerConstantExpr without its
   * inbounds property set, as an operand to a store. How can this case
   * be detected?
   */
   Global[122][44]=0;

   int a = 54;
   int b = 25 + 15;
   Global[a][b]=303;

   // Legal access into a compile-time-unknown-sized array.
   int k = 4;
   int m = 4;
   int n = 4;
   int z[k][m][n];
   z[3][3][3]=303;
   z[3][3][3]=404;

   // An illegal read access. This should halt program execution.
   y[0][0] = y[0][20];

   // Whether the follwing check succeeds depends on the number of arguements
   // passed to the program.
   char w[argc];
   w[argc/2 + 1] = 123;

   int c = 97;
   int test[c][c-1];
   test[39][39] = 1;
   if(test[39][39] == 1)
     test[39][39] = 1; 

   while(test[35][35] < 10)
   {
     test[35][35] = test[35][35] + 1;
     if(c < 100)
     {
       test[29][29] = 1;
     }
     else
     {
       test[29][29] = 1;
     }
   }



   int ARR[15];
   int ind;
   for (ind = 0; ind < 15; ind++) ARR[ind] = ind * ind;

   return 0;
}
