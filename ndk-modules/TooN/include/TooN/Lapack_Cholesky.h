// -*- c++ -*-

//     Copyright (C) 2009 Tom Drummond (twd20@cam.ac.uk)

//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//2. Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
//THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
//AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
//LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.


#ifndef TOON_INCLUDE_LAPACK_CHOLESKY_H
#define TOON_INCLUDE_LAPACK_CHOLESKY_H

#include <TooN/TooN.h>

#include <TooN/lapack.h>

#include <assert.h>

namespace TooN {


/**
Decomposes a positive-semidefinite symmetric matrix A (such as a covariance) into L*L^T, where L is lower-triangular.
Also can compute A = S*S^T, with S lower triangular.  The LDL^T form is faster to compute than the class Cholesky decomposition.
The decomposition can be used to compute A^-1*x, A^-1*M, M*A^-1*M^T, and A^-1 itself, though the latter rarely needs to be explicitly represented.
Also efficiently computes det(A) and rank(A).
It can be used as follows:
@code
// Declare some matrices.
Matrix<3> A = ...; // we'll pretend it is pos-def
Matrix<2,3> M;
Matrix<2> B;
Vector<3> y = make_Vector(2,3,4);
// create the Cholesky decomposition of A
Cholesky<3> chol(A);
// compute x = A^-1 * y
x = cholA.backsub(y);
//compute A^-1
Matrix<3> Ainv = cholA.get_inverse();
@endcode
@ingroup gDecomps

Cholesky decomposition of a symmetric matrix.
Only the lower half of the matrix is considered
This uses the non-sqrt version of the decomposition
giving symmetric M = L*D*L.T() where the diagonal of L contains ones
@param Size the size of the matrix
@param Precision the precision of the entries in the matrix and its decomposition
**/
template <int Size, typename Precision=DefaultPrecision>
class Lapack_Cholesky {
public:

    Lapack_Cholesky(){}
	
	template<class P2, class B2>
	Lapack_Cholesky(const Matrix<Size, Size, P2, B2>& m) 
	  : my_cholesky(m), my_cholesky_lapack(m) {
		SizeMismatch<Size,Size>::test(m.num_rows(), m.num_cols());
		do_compute();
	}

	/// Constructor for Size=Dynamic
	Lapack_Cholesky(int size) : my_cholesky(size,size), my_cholesky_lapack(size,size) {}

	template<class P2, class B2> void compute(const Matrix<Size, Size, P2, B2>& m){
		SizeMismatch<Size,Size>::test(m.num_rows(), m.num_cols());
		SizeMismatch<Size,Size>::test(m.num_rows(), my_cholesky.num_rows());
		my_cholesky_lapack=m;
		do_compute();
	}



	void do_compute(){
		FortranInteger N = my_cholesky.num_rows();
		FortranInteger info;
		potrf_("L", &N, my_cholesky_lapack.my_data, &N, &info);
		for (int i=0;i<N;i++) {
		  int j;
		  for (j=0;j<=i;j++) {
		    my_cholesky[i][j]=my_cholesky_lapack[j][i];
		  }
		  // LAPACK does not set upper triangle to zero, 
		  // must be done here
		  for (;j<N;j++) {
		    my_cholesky[i][j]=0;
		  }
		}
		assert(info >= 0);
		if (info > 0) {
			my_rank = info-1;
		} else {
		    my_rank = N;
		}
	}

	int rank() const { return my_rank; }

	template <int Size2, typename P2, typename B2>
		Vector<Size, Precision> backsub (const Vector<Size2, P2, B2>& v) const {
		SizeMismatch<Size,Size2>::test(my_cholesky.num_cols(), v.size());

		Vector<Size, Precision> result(v);
		FortranInteger N=my_cholesky.num_rows();
		FortranInteger NRHS=1;
		FortranInteger info;
		potrs_("L", &N, &NRHS, my_cholesky_lapack.my_data, &N, result.my_data, &N, &info);     
		assert(info==0);
		return result;
	}

	template <int Size2, int Cols2, typename P2, typename B2>
		Matrix<Size, Cols2, Precision, ColMajor> backsub (const Matrix<Size2, Cols2, P2, B2>& m) const {
		SizeMismatch<Size,Size2>::test(my_cholesky.num_cols(), m.num_rows());

		Matrix<Size, Cols2, Precision, ColMajor> result(m);
		FortranInteger N=my_cholesky.num_rows();
		FortranInteger NRHS=m.num_cols();
		FortranInteger info;
		potrs_("L", &N, &NRHS, my_cholesky_lapack.my_data, &N, result.my_data, &N, &info);     
		assert(info==0);
		return result;
	}

	template <int Size2, typename P2, typename B2>
		Precision mahalanobis(const Vector<Size2, P2, B2>& v) const {
		return v * backsub(v);
	}

	Matrix<Size,Size,Precision> get_L() const {
		return my_cholesky;
	}

	Precision determinant() const {
		Precision det = my_cholesky[0][0];
		for (int i=1; i<my_cholesky.num_rows(); i++)
			det *= my_cholesky[i][i];
		return det*det;
	}

	Matrix<> get_inverse() const {
		Matrix<Size, Size, Precision> M(my_cholesky.num_rows(),my_cholesky.num_rows());
		M=my_cholesky_lapack;
		FortranInteger N = my_cholesky.num_rows();
		FortranInteger info;
		potri_("L", &N, M.my_data, &N, &info);
		assert(info == 0);
		for (int i=1;i<N;i++) {
		  for (int j=0;j<i;j++) {
		    M[i][j]=M[j][i];
		  }
		}
		return M;
	}

private:
	Matrix<Size,Size,Precision> my_cholesky;     
	Matrix<Size,Size,Precision> my_cholesky_lapack;     
	FortranInteger my_rank;
};


}

#endif
