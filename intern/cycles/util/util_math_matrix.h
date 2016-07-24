/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_MATH_MATRIX_H__
#define __UTIL_MATH_MATRIX_H__

CCL_NAMESPACE_BEGIN

#define MAT(A, size, row, col) A[(row)*(size)+(col)]

ccl_device_inline void math_matrix_zero(float *A, int n)
{
	for(int i = 0; i < n*n; i++)
		A[i] = 0.0f;
}

ccl_device_inline void math_vector_zero(float *v, int n)
{
	for(int i = 0; i < n; i++)
		v[i] = 0.0f;
}

ccl_device_inline void math_vec3_zero(float3 *v, int n)
{
	for(int i = 0; i < n; i++)
		v[i] = make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline void math_matrix_zero_lower(float *A, int n)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) = 0.0f;
}

ccl_device_inline void math_matrix_identity(float *A, int n)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col < n; col++)
			MAT(A, n, row, col) = (row == col)? 1.0f: 0.0f;
}

/* In-place Cholesky-Banachiewicz decomposition of the square, positive-definite matrix A
 * into a lower triangular matrix L so that A = L*L^T. A is being overwritten by L.
 * Also, only the lower triangular part of A is ever accessed. */
ccl_device void math_cholesky(float *A, int n)
{
	for(int row = 0; row < n; row++) {
		for(int col = 0; col <= row; col++) {
			float sum_col = MAT(A, n, row, col);
			for(int k = 0; k < col; k++)
				sum_col -= MAT(A, n, row, k) * MAT(A, n, col, k);
			if(row == col) sum_col = sqrtf(sum_col);
			else           sum_col = sum_col / MAT(A, n, col, col);
			MAT(A, n, row, col) = sum_col;
		}
	}
}

/* Perform a Singular Value decomposition on A.
 * Returns the transpose of V and the squared singular values. A is destroyed in the process.
 * Note: Still buggy, therefore the old version from the proof-of-concept implementation is used for now. */
ccl_device int math_svd(float *A, float *V, float *S2, int n)
{
	/* Initialize V to the identity matrix. */
	for(int row = 0; row < n; row++)
		for(int col = 0; col < n; col++)
			MAT(V, n, row, col) = (row == col)? 1.0f: 0.0f;

	const float eps1 = 1e-7f;
	const float eps2 = 10.0f * n * eps1*eps1;
	const float eps3 = 0.1f * eps1;

	int estimated_rank = n;
	for(int rotations = n, i = 0; rotations > 0 && i < 7; i++) {
		rotations = estimated_rank * (estimated_rank - 1)/2;
		for(int col1 = 0; col1 < estimated_rank-1; col1++) {
			for(int col2 = col1+1; col2 < estimated_rank; col2++) {
				float p = 0.0f, q = 0.0f, r = 0.0f;
				for(int row = 0; row < n; row++) {
					float c1 = MAT(A, n, row, col1), c2 = MAT(A, n, row, col2);
					p += c1*c2;
					q += c1*c1;
					r += c2*c2;
				}
				S2[col1] = q;
				S2[col2] = r;

				float x, y;
				if(q >= r) {
					if(q < eps2*S2[0] || fabsf(p) < eps3*q) {
						rotations--;
						continue;
					}
					const float invQ = 1.0f / q;
					p *= invQ;
					r = 1.0f - r*invQ;
					const float pr = p*r;
					const float invVt = 1.0f / sqrtf(4.0f * pr*pr);
					x = sqrtf(0.5f * (1.0f + r*invVt));
					y = p*invVt / x;
				}
				else {
					const float invR = 1.0f / r;
					p *= invR;
					q = q*invR - 1.0f;
					const float pq = p*q;
					const float invVt = 1.0f / sqrtf(4.0f * pq*pq);
					y = sqrtf(0.5f * (1.0f - q*invVt));
					if(p < 0.0f) y = -y;
					x = p*invVt / y;
				}

#define ROT(A, n, row1, row2, col1, col2, x, y) { float c1 = MAT(A, n, row1, col1), c2 = MAT(A, n, row2, col2); MAT(A, n, row1, col1) = c1*(x)+c2*(y); MAT(A, n, row2, col2) = -c1*(y)+c2*(x); }
				for(int row = 0; row < n; row++) {
					ROT(A, n, row, row, col1, col2, x, y);
					/* V is stored as its transpose. */
					ROT(V, n, col1, col2, row, row, x, y);
				}
#undef ROT
			}
		}
		while(estimated_rank > 2 && S2[estimated_rank-1] < (S2[0] + eps3)*eps3)
			estimated_rank--;
	}

	return estimated_rank;
}

ccl_device_inline void math_matrix_add_diagonal(float *A, int n, float val)
{
	for(int row = 0; row < n; row++)
		MAT(A, n, row, row) += val;
}

/* Add Gramian matrix of v to A.
 * The Gramian matrix of v is v^T*v, so element (i,j) is v[i]*v[j].
 * Obviously, the resulting matrix is symmetric, so only the lower triangluar part is stored. */
ccl_device_inline void math_add_gramian(float *A, int n, float *v, float weight)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) += v[row]*v[col]*weight;
}

ccl_device_inline void math_add_vec3(float3 *v, int n, float *x, float3 w)
{
	for(int i = 0; i < n; i++)
		v[i] += w*x[i];
}

ccl_device_inline void math_lower_tri_to_full(float *A, int n)
{
	for(int row = 0; row < n; row++)
		for(int col = row+1; col < n; col++)
			MAT(A, n, row, col) = MAT(A, n, col, row);
}

ccl_device_inline float math_dot(float *a, float *b, int n)
{
	float d = 0.0f;
	for(int i = 0; i < n; i++)
		d += a[i]*b[i];
	return d;
}

/* Solve the linear equation system L*x = b through forward substitution, where L is a lower triangular matrix.
 * x is initially set to the right-hand-side vector and is overwritten with the solution vector x. */
ccl_device_inline void math_substitute_forward_vec3(float *L, int n, float3 *x)
{
	for(int row = 0; row < n; row++) {
		float3 sum = make_float3(0.0f, 0.0f, 0.0f);
		for(int col = 0; col < row; col++)
			sum += MAT(L, n, row, col) * x[col];
		x[row] = (x[row] - sum) / MAT(L, n, row, row);
	}
}

/* Solve the linear equation system L*x = b through backsubstitution, where L is a upper triangular matrix.
 * In this implementation, instead of L, L^T is passed instead.
 * x is initially set to the right-hand-side vector and is overwritten with the solution vector x. */
ccl_device_inline void math_substitute_back_vec3(float *LT, int n, float3 *x)
{
	for(int row = n-1; row >= 0; row--) {
		float3 sum = make_float3(0.0f, 0.0f, 0.0f);
		for(int col = row+1; col < n; col++)
			sum += MAT(LT, n, col, row) * x[col];
		x[row] = (x[row] - sum) / MAT(LT, n, row, row);
	}
}

ccl_device_inline void math_inverse_lower_tri(float *L, float *invL, int n)
{
	for(int comp = 0; comp < n; comp++) {
		for(int row = 0; row < comp; row++)
			MAT(invL, n, row, comp) = 0.0f;
		MAT(invL, n, comp, comp) = 1.0f / MAT(L, n, comp, comp);
		for(int row = comp+1; row < n; row++) {
			float sum = 0.0f;
			for(int col = comp; col < row; col++)
				sum += MAT(L, n, row, col) * MAT(invL, n, col, comp);
			MAT(invL, n, row, comp) = -sum/MAT(L, n, row, row);
		}
	}
}

/* Inverts the lower triangular matrix L and overwrites it with the transpose
 * of the result. */
ccl_device_inline void math_inverse_lower_tri_inplace(float *L, int n)
{
	for(int row = 0; row < n; row++)
		MAT(L, n, row, row) = 1.0f / MAT(L, n, row, row);

	for(int comp = 0; comp < n; comp++) {
		for(int row = comp+1; row < n; row++) {
			float sum = 0.0f;
			for(int col = comp; col < row; col++)
				sum += MAT(L, n, row, col) * MAT(L, n, comp, col);
			MAT(L, n, comp, row) = -sum*MAT(L, n, row, row);
		}
	}
}

#define LSQ_SIZE 5
/* Utility functions for least-squares-fitting a one-dimensional linear function: f(x) = a*x+b. */
ccl_device_inline void math_lsq_init(double *lsq)
{
	for(int i = 0; i < 5; i++)
		lsq[i] = 0.0;
}

ccl_device_inline void math_lsq_add(double *lsq, double x, double y)
{
	lsq[0] += 1.0;
	lsq[1] += x;
	lsq[2] += x*x;
	lsq[3] += y;
	lsq[4] += x*y;
}

/* Returns the first-order coefficient a of the fitted function. */
ccl_device_inline double math_lsq_solve(double *lsq, double *zeroth)
{
	double inv_det = 1.0 / (lsq[0]*lsq[2] - lsq[1]*lsq[1] + 1e-4);
	if(zeroth)
		*zeroth = (lsq[2]*lsq[3] - lsq[1]*lsq[3]) * inv_det;
	return (lsq[0]*lsq[4] - lsq[1]*lsq[3]) * inv_det;
}

ccl_device float math_largest_eigenvalue(float *A, int n, float *vec, float *tmp)
{
	/* Matrix-Vector-Multiplication that only accesses the lower triangular part of A. */
	float fac = 0.0f;
	float eigval = 1.0f;

	for(int r = 0; r < n; r++)
		fac += vec[r]*vec[r];
	fac = 1.0f / sqrtf(fac);
	for(int r = 0; r < n; r++)
		vec[r] *= fac;

	for(int i = 0; i < 100; i++) {
		fac = 0.0f;
		for(int r = 0; r < n; r++) {
			tmp[r] = 0.0f;
			int c;
			for(c = 0; c <= r; c++)
				tmp[r] += MAT(A, n, r, c)*vec[c];
			for(; c < n; c++)
				tmp[r] += MAT(A, n, c, r)*vec[c];
			fac += tmp[r]*tmp[r];
		}

		if(fac < 1e-10f) return 0.0f;
		float new_eigval = sqrtf(fac);

		fac = 1.0f / sqrtf(fac);
		for(int r = 0; r < n; r++) {
			vec[r] = tmp[r]*fac;
		}

		if(fabsf(new_eigval - eigval)/max(new_eigval, 1e-7f) < 1e-6f)
			return new_eigval;
		eigval = new_eigval;
	}

	return 0.0f;
}

/* TODO(lukas): Fix new code and remove this. */
ccl_device int svd(float *A, float *V, float *S2, int n)
{
    int  i, j, k, EstColRank = n, RotCount = n, SweepCount = 0;
    int slimit = 8;
    float eps = 1e-8f;
    float e2 = 10.f * n * eps * eps;
    float tol = 0.1f * eps;
    float vt, p, x0, y0, q, r, c0, s0, d1, d2;

    for(int r = 0; r < n; r++)
        for(int c = 0; c < n; c++)
            V[r*n+c] = (c == r)? 1.0f: 0.0f;

    while (RotCount != 0 && SweepCount++ <= slimit) {
        RotCount = EstColRank * (EstColRank - 1) / 2;

        for (j = 0; j < EstColRank-1; ++j) {
            for (k = j+1; k < EstColRank; ++k) {
                p = q = r = 0.0;

                for (i = 0; i < n; ++i) {
                    x0 = A[i * n + j];
                    y0 = A[i * n + k];
                    p += x0 * y0;
                    q += x0 * x0;
                    r += y0 * y0;
                }

                S2[j] = q;
                S2[k] = r;

                if (q >= r) {
                    if (q <= e2 * S2[0] || fabsf(p) <= tol * q)
                        RotCount--;
                    else {
                        p /= q;
                        r = 1.f - r/q;
                        vt = sqrtf(4.0f * p * p + r * r);
                        c0 = sqrtf(0.5f * (1.f + r / vt));
                        s0 = p / (vt*c0);

                        // Rotation
                        for (i = 0; i < n; ++i) {
                            d1 = A[i * n + j];
                            d2 = A[i * n + k];
                            A[i * n + j] = d1*c0+d2*s0;
                            A[i * n + k] = -d1*s0+d2*c0;
                        }
                        for (i = 0; i < n; ++i) {
                            d1 = V[i * n + j];
                            d2 = V[i * n + k];
                            V[i * n + j] = d1 * c0 + d2 * s0;
                            V[i * n + k] = -d1 * s0 + d2 * c0;
                        }
                    }
                } else {
                    p /= r;
                    q = q / r - 1.f;
                    vt = sqrtf(4.f * p * p + q * q);
                    s0 = sqrtf(0.5f * (1.f - q / vt));
                    if (p < 0.f)
                        s0 = -s0;
                    c0 = p / (vt * s0);

                    // Rotation
                    for (i = 0; i < n; ++i) {
                        d1 = A[i * n + j];
                        d2 = A[i * n + k];
                        A[i * n + j] = d1 * c0 + d2 * s0;
                        A[i * n + k] = -d1 * s0 + d2 * c0;
                    }
                    for (i = 0; i < n; ++i) {
                        d1 = V[i * n + j];
                        d2 = V[i * n + k];
                        V[i * n + j] = d1 * c0 + d2 * s0;
                        V[i * n + k] = -d1 * s0 + d2 * c0;
                    }
                }
            }
        }
        while (EstColRank >= 3 && S2[EstColRank-1] <= S2[0] * tol + tol * tol)
            EstColRank--;
    }

	for(int row = 0; row < n; row++)
		for(int col = 0; col < row; col++) {
			float temp = V[row*n+col];
			V[row*n+col] = V[col*n+row];
			V[col*n+row] = temp;
		}
    return EstColRank;
}

#undef MAT

CCL_NAMESPACE_END

#endif  /* __UTIL_MATH_MATRIX_H__ */