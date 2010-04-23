int main(int argc, char** argv) {
  int n = argc*100; 
  char A[n][n];
  int i, j;

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      A[i][j] = (i*j - j)%100 + (i==0?0:A[i-1][j]);
    }
  }

  return 0;
}
