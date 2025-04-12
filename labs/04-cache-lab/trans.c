/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char trans_desc_v1[] = "block transpose_v1";
void trans_v1(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, i_c, j_c, tmp, k, diag_val;
    int blockSize;
    if (M == 32)
    {
        blockSize = 8;
    }
    else if (M == 64)
    {
        blockSize = 4;
    }
    else if (M == 61)
    {
        blockSize = 16;
    }
    
    for (i_c = 0; i_c < N; i_c += blockSize)
    {
        for (j_c = 0; j_c < M; j_c += blockSize)
        {
            for (i = i_c; (i < i_c + blockSize) && (i < N); i++)
            {
                for (j = j_c; (j < j_c + blockSize) && (j < M); j++)
                {
                    if (j != i)
                    {
                        tmp = A[i][j];
                        B[j][i] = tmp;
                    }
                    else
                    {
                        k = i;
                        diag_val = A[i][j];
                    }
                }
                if (i_c == j_c)
                {
                    B[k][k] = diag_val;
                }
            }
        }
    }
}

char trans_desc_v2[] = "block transpose_v2";
void trans_v2(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, i_c, j_c, tmp, tmp2, k, diag_val;
    int blockSize;
    if (M == 32)
    {
        blockSize = 8;
    }
    else if (M == 64)
    {
        blockSize = 4;
    }
    else if (M == 61)
    {
        blockSize = 16;
    }
    for (i_c = 0; i_c < N; i_c += blockSize)
    {
        for (j_c = 0; j_c < M; j_c += blockSize)
        {
            if (i_c != j_c)
            {
                for (i = i_c; (i < i_c + blockSize) && (i < N); i++)
                {
                    for (j = j_c; (j < j_c + blockSize) && (j < M); j++)
                    {
                        tmp = A[i][j];
                        B[j][i] = tmp;
                    }
                }
            }
            else
            {
                for (i = i_c; (i < i_c + blockSize) && (i < N); i++)
                {
                    for (j = j_c; (j < j_c + blockSize) && (j < M); j++) 
                    {
                        if (j != i)
                        {    
                            tmp = A[i][j];
                            B[M - i - 1][N - j - 1] = tmp;
                        }
                        else
                        {
                            k = i;
                            diag_val = A[i][i];
                        }
                    }
                    B[k][k] = diag_val;
                }
            }
        }
    }
    for (i_c = 0; i_c < N/2; i_c += blockSize)
    {
        j_c = i_c;
        for (i = i_c; (i < i_c + blockSize) && (i < N); i++)
        {
            for (j = j_c; (j < j_c + blockSize) && (j < M); j++) 
            {
                if (j != i)
                {
                    tmp = B[M - i - 1][N - j - 1];
                    tmp2 = B[j][i];
                    B[j][i] = tmp;
                    B[M - i - 1][N - j - 1] = tmp2;
                }
            }
        }
    }
}

char trans_desc_64_64_opt[] = "64x64 block transpose optimal sol";
void trans_64_64(int M, int N, int A[N][M], int B[M][N])
{
    // https://zhuanlan.zhihu.com/p/387662272
    // use 8x8 block
    // idea 1: for the non-diagonal blocks, the upper 4x8 and lower 4x8 share the same cache lines
    // therefore, need to process the upper 4x8 first
    // row 0, col 0-8 -> col 0, row 0 - 4 + col 4, row 0 - 4 (both parts are in the upper 4x8)
    // idea 2: for the diagonal blocks, need to borrow a 4x8 space of next non-diagonal blocks

    int i, j, i_c, j_c;
    int x0, x1, x2, x3, x4, x5, x6, x7;

    for (i_c = 0; i_c < N; i_c += 8)
    {
        // 1. process the diagonal 
        j_c = (i_c == 0) ? 8 : 0;
        // 1.1 copy lower 4x8 of A to borrowed upper 4x8 of B
        for (i = i_c + 4; i < i_c + 8; i++)
        {
            x0 = A[i][i_c+0];
            x1 = A[i][i_c+1];
            x2 = A[i][i_c+2];
            x3 = A[i][i_c+3];
            x4 = A[i][i_c+4];
            x5 = A[i][i_c+5];
            x6 = A[i][i_c+6];
            x7 = A[i][i_c+7]; 

            B[i-4][j_c+0] = x0;
            B[i-4][j_c+1] = x1;
            B[i-4][j_c+2] = x2;
            B[i-4][j_c+3] = x3;
            B[i-4][j_c+4] = x4;
            B[i-4][j_c+5] = x5;
            B[i-4][j_c+6] = x6;
            B[i-4][j_c+7] = x7;
        }
        // 1.2 take the transpose of lower-left and lower-right 4x4 in place respectively
        for (i = 0; i < 4; i++)
        {
            for (j = i + 1; j < 4; j++)
            {
                x0 = B[i_c+i][j_c+j];
                B[i_c+i][j_c+j] = B[i_c+j][j_c+i];
				B[i_c+j][j_c+i] = x0;

				x0 = B[i_c+i][j_c+j+4];
				B[i_c+i][j_c+j+4] = B[i_c+j][j_c+i+4];
				B[i_c+j][j_c+i+4] = x0;
            }
        }
		// 1.3 move the upper 4x8 blocks from A to B
        for (i = i_c; i < i_c + 4; ++i){
			x0 = A[i][i_c+0];
			x1 = A[i][i_c+1];
			x2 = A[i][i_c+2];
			x3 = A[i][i_c+3];
			x4 = A[i][i_c+4];
			x5 = A[i][i_c+5];
			x6 = A[i][i_c+6];
			x7 = A[i][i_c+7];

			B[i][i_c+0] = x0;
			B[i][i_c+1] = x1;
			B[i][i_c+2] = x2;
			B[i][i_c+3] = x3;
			B[i][i_c+4] = x4;
			B[i][i_c+5] = x5;
			B[i][i_c+6] = x6;
			B[i][i_c+7] = x7;
		}
        // 1.2 take the transpose of upper-left and upper-right 4x4 in place respectively
        for (i = i_c; i < i_c + 4; i++)
        {
            for (j = i + 1; j < i_c + 4; j++)
            {
                x0 = B[i][j];
                B[i][j] = B[j][i];
				B[j][i] = x0;

				x0 = B[i][j+4];
				B[i][j+4] = B[j][i+4];
				B[j][i+4] = x0;
            }
        }

        // 1.3 swaping the lower-left and upper-right
		for (i = 0; i < 4; ++ i){
			x0 = B[i_c+i][i_c+4];
			x1 = B[i_c+i][i_c+5];
			x2 = B[i_c+i][i_c+6];
			x3 = B[i_c+i][i_c+7];

			B[i_c+i][i_c+4] = B[i_c+i][j_c+0];
			B[i_c+i][i_c+5] = B[i_c+i][j_c+1];
			B[i_c+i][i_c+6] = B[i_c+i][j_c+2];
			B[i_c+i][i_c+7] = B[i_c+i][j_c+3];

			B[i_c+i][j_c+0] = x0;
			B[i_c+i][j_c+1] = x1;
			B[i_c+i][j_c+2] = x2;
			B[i_c+i][j_c+3] = x3;

		}

		// 1.4 filling the original lower 4x8 from the block-shifting block
		for (i = 0; i < 4; ++ i){
			B[i_c+i+4][i_c+0] = B[i_c+i][j_c+0];
			B[i_c+i+4][i_c+1] = B[i_c+i][j_c+1];
			B[i_c+i+4][i_c+2] = B[i_c+i][j_c+2];
			B[i_c+i+4][i_c+3] = B[i_c+i][j_c+3];
			B[i_c+i+4][i_c+4] = B[i_c+i][j_c+4];
			B[i_c+i+4][i_c+5] = B[i_c+i][j_c+5];
			B[i_c+i+4][i_c+6] = B[i_c+i][j_c+6];
			B[i_c+i+4][i_c+7] = B[i_c+i][j_c+7];
		}

        // 2. process the non-diagonal
        for (j_c = 0; j_c < M; j_c += 8)
        {
            if (i_c == j_c) continue;
            for (i = i_c; i < i_c + 4; i++)
            {
                x0 = A[i][j_c+0];
                x1 = A[i][j_c+1];
                x2 = A[i][j_c+2];
                x3 = A[i][j_c+3];
                x4 = A[i][j_c+4];
                x5 = A[i][j_c+5];
                x6 = A[i][j_c+6];
                x7 = A[i][j_c+7];

                B[j_c+0][i] = x0;
                B[j_c+1][i] = x1;
                B[j_c+2][i] = x2;
                B[j_c+3][i] = x3;

                B[j_c+0][i+4] = x4;
                B[j_c+1][i+4] = x5;
                B[j_c+2][i+4] = x6;
                B[j_c+3][i+4] = x7;
            }

            for (j = j_c; j < j_c + 4; j++)
            {
                x0 = A[i_c+4][j];
                x1 = A[i_c+5][j];
                x2 = A[i_c+6][j];
                x3 = A[i_c+7][j];
                
                x4 = B[j][i_c+4];
                x5 = B[j][i_c+5];
                x6 = B[j][i_c+6];
                x7 = B[j][i_c+7];

                B[j][i_c+4] = x0;
                B[j][i_c+5] = x1;
                B[j][i_c+6] = x2;
                B[j][i_c+7] = x3;

                B[j+4][i_c+0] = x4;
                B[j+4][i_c+1] = x5;
                B[j+4][i_c+2] = x6;
                B[j+4][i_c+3] = x7;
            }

            for (i = i_c + 4; i < i_c + 8; j++)
            {
                x0 = A[i][j_c + 4];
                x1 = A[i][j_c + 5];
                x2 = A[i][j_c + 6];
                x3 = A[i][j_c + 7];

                B[j_c + 4][i] = x0;
                B[j_c + 5][i] = x1;
                B[j_c + 6][i] = x2;
                B[j_c + 7][i] = x3;
            }

        }
    }
}

void transpose_64(int M, int N, int A[N][M], int B[M][N]){
	// loop indices
	int i, j, ii, jj;
	// temporary variables
	int a0, a1, a2, a3, a4, a5, a6, a7;
	// main loop: ii, jj for each block of size 8x8

	for (jj = 0; jj < N; jj += 8){
		// process diagonal blocks first
		
		// ii: j-index of target block (block-shifting)
		// more specifically, use the upper half of [jj, ii] to transpose [jj, jj] block 
		// the target block is the one that will be used immediately after the diagonal processing
		if (jj == 0) ii = 8; else ii = 0;

		// move the lower 4x8 blocks from A to B, with block-shifting to the target block 
		for (i = jj; i < jj + 4; ++i){
			a0 = A[i+4][jj+0];
			a1 = A[i+4][jj+1];
			a2 = A[i+4][jj+2];
			a3 = A[i+4][jj+3];
			a4 = A[i+4][jj+4];
			a5 = A[i+4][jj+5];
			a6 = A[i+4][jj+6];
			a7 = A[i+4][jj+7];

			B[i][ii+0] = a0;
			B[i][ii+1] = a1;
			B[i][ii+2] = a2;
			B[i][ii+3] = a3;
			B[i][ii+4] = a4;
			B[i][ii+5] = a5;
			B[i][ii+6] = a6;
			B[i][ii+7] = a7;
		}

		// taking transpose of lower-left and lower-right 4x4 within themselves respectively
		for (i = 0; i < 4; ++ i){
			for (j = i + 1; j < 4; ++j){
				a0 = B[jj+i][ii+j];
				B[jj+i][ii+j] = B[jj+j][ii+i];
				B[jj+j][ii+i] = a0;

				a0 = B[jj+i][ii+j+4];
				B[jj+i][ii+j+4] = B[jj+j][ii+i+4];
				B[jj+j][ii+i+4] = a0;
			}
		}

		// moving the upper 4x8 blocks from A to B
		for (i = jj; i < jj + 4; ++i){
			a0 = A[i][jj+0];
			a1 = A[i][jj+1];
			a2 = A[i][jj+2];
			a3 = A[i][jj+3];
			a4 = A[i][jj+4];
			a5 = A[i][jj+5];
			a6 = A[i][jj+6];
			a7 = A[i][jj+7];

			B[i][jj+0] = a0;
			B[i][jj+1] = a1;
			B[i][jj+2] = a2;
			B[i][jj+3] = a3;
			B[i][jj+4] = a4;
			B[i][jj+5] = a5;
			B[i][jj+6] = a6;
			B[i][jj+7] = a7;
		}

		// taking transpose of upper-left and upper-right 4x4 within themselves respectively
		for (i = jj; i < jj + 4; ++i){
			for (j = i + 1; j < jj + 4; ++j){
				a0 = B[i][j];
				B[i][j] = B[j][i];
				B[j][i] = a0;

				a0 = B[i][j+4];
				B[i][j+4] = B[j][i+4];
				B[j][i+4] = a0;
			}
		}
		
		// swaping the lower-left and upper-right
		for (i = 0; i < 4; ++ i){
			a0 = B[jj+i][jj+4];
			a1 = B[jj+i][jj+5];
			a2 = B[jj+i][jj+6];
			a3 = B[jj+i][jj+7];

			B[jj+i][jj+4] = B[jj+i][ii+0];
			B[jj+i][jj+5] = B[jj+i][ii+1];
			B[jj+i][jj+6] = B[jj+i][ii+2];
			B[jj+i][jj+7] = B[jj+i][ii+3];

			B[jj+i][ii+0] = a0;
			B[jj+i][ii+1] = a1;
			B[jj+i][ii+2] = a2;
			B[jj+i][ii+3] = a3;

		}

		// filling the original lower 4x8 from the block-shifting block
		for (i = 0; i < 4; ++ i){
			B[jj+i+4][jj+0] = B[jj+i][ii+0];
			B[jj+i+4][jj+1] = B[jj+i][ii+1];
			B[jj+i+4][jj+2] = B[jj+i][ii+2];
			B[jj+i+4][jj+3] = B[jj+i][ii+3];
			B[jj+i+4][jj+4] = B[jj+i][ii+4];
			B[jj+i+4][jj+5] = B[jj+i][ii+5];
			B[jj+i+4][jj+6] = B[jj+i][ii+6];
			B[jj+i+4][jj+7] = B[jj+i][ii+7];
		}

		// processing off-diagonal blocks
		for (ii = 0; ii < M; ii += 8){
			if (ii == jj){
				// skip diagonal blocks
				continue;
			}else{
				// taking transpose of upper-left 4x4 and upper-right 4x4 within themselves respectively
				for (i = ii; i < ii + 4; ++i){
					a0 = A[i][jj+0];
					a1 = A[i][jj+1];
					a2 = A[i][jj+2];
					a3 = A[i][jj+3];
					a4 = A[i][jj+4];
					a5 = A[i][jj+5];
					a6 = A[i][jj+6];
					a7 = A[i][jj+7];

					B[jj+0][i] = a0;
					B[jj+1][i] = a1;
					B[jj+2][i] = a2;
					B[jj+3][i] = a3;
					B[jj+0][i+4] = a4;
					B[jj+1][i+4] = a5;
					B[jj+2][i+4] = a6;
					B[jj+3][i+4] = a7;
				}

				// taking transpose of lower-left 4x4 and store to upper-right 4x4, and move upper-right 4x4 to lower-left 4x4
				for (j = jj; j < jj + 4; ++j){
					a0 = A[ii+4][j];
					a1 = A[ii+5][j];
					a2 = A[ii+6][j];
					a3 = A[ii+7][j];

					a4 = B[j][ii+4];
					a5 = B[j][ii+5];
					a6 = B[j][ii+6];
					a7 = B[j][ii+7];

					B[j][ii+4] = a0;
					B[j][ii+5] = a1;
					B[j][ii+6] = a2;
					B[j][ii+7] = a3;

					B[j+4][ii+0] = a4;
					B[j+4][ii+1] = a5;
					B[j+4][ii+2] = a6;
					B[j+4][ii+3] = a7;
				}

				// taking transpose of lower-right 4x4
				for (i = ii + 4; i < ii + 8; ++i){
					a0 = A[i][jj+4];
					a1 = A[i][jj+5];
					a2 = A[i][jj+6];
					a3 = A[i][jj+7];

					B[jj+4][i] = a0;
					B[jj+5][i] = a1;
					B[jj+6][i] = a2;
					B[jj+7][i] = a3;
				}
			}
		}
	}
}

char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M != 64)
    {
        trans_v1(M, N, A, B);
    }
    else
    {
        transpose_64(M, N, A, B);
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

    registerTransFunction(trans_v1, trans_desc_v1);

    registerTransFunction(trans_v2, trans_desc_v2);

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

