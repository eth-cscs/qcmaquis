#ifndef __ALPS_DENSE_MATRIX_ALGORITHMS_HPP__
#define __ALPS_DENSE_MATRIX_ALGORITHMS_HPP__

#include "dense_matrix/matrix_concept_check.hpp"
#include "dense_matrix/diagonal_matrix.h"

#include <boost/numeric/bindings/lapack/driver/gesvd.hpp>
#include <boost/numeric/bindings/lapack/driver/gesdd.hpp>
#include <boost/numeric/bindings/lapack/driver/syevd.hpp>
#include <boost/numeric/bindings/lapack/computational/geqrf.hpp>
#include <boost/numeric/bindings/std/vector.hpp>

namespace blas
{
    namespace detail {
        template<typename T> struct sv_type { typedef T type; };
        template<typename T>
        struct sv_type<std::complex<T> > { typedef T type; };
    }
    
    template<typename T, class MemoryBlock>
    void svd(dense_matrix<T, MemoryBlock> M,
             dense_matrix<T, MemoryBlock> & U,
             dense_matrix<T, MemoryBlock>& V,
             typename associated_diagonal_matrix<dense_matrix<typename detail::sv_type<T>::type, MemoryBlock> >::type & S)
    {
        BOOST_CONCEPT_ASSERT((blas::Matrix<dense_matrix<T, MemoryBlock> >));
        typename dense_matrix<T, MemoryBlock>::size_type k = std::min(num_rows(M), num_columns(M));
        resize(U, num_rows(M), k);
        resize(V, k, num_columns(M));
        
        std::vector<typename detail::sv_type<T>::type> S_(k);
//        int info = boost::numeric::bindings::lapack::gesdd('S', M, S_, U, V);
        int info = boost::numeric::bindings::lapack::gesvd('S', 'S', M, S_, U, V);
        if (info != 0)
            throw std::runtime_error("Error in SVD!");
        
        S = typename associated_diagonal_matrix<dense_matrix<typename detail::sv_type<T>::type, MemoryBlock> >::type(S_);
    }
    
    template<typename T, class MemoryBlock>
    void qr(dense_matrix<T, MemoryBlock> M,
            dense_matrix<T, MemoryBlock> & Q,
            dense_matrix<T, MemoryBlock> & R)
    {
        typename dense_matrix<T, MemoryBlock>::size_type k = std::min(num_rows(M), num_columns(M));
        std::vector<double> tau(k);
        
        int info = 0; //boost::numeric::bindings::lapack::geqrf(M, tau);
        if (info != 0)
            throw std::runtime_error("Error in geqrf");
        
        resize(Q, num_rows(M), k);
        resize(R, k, num_columns(M));
        
        // get R
        std::fill(elements(R).first, elements(R).second, 0);
        for (std::size_t c = 0; c < num_columns(M); ++c)
            for (std::size_t r = 0; r <= c; ++r)
                R(r, c) = M(r, c);
        
        // get Q from householder reflections in M
        std::fill(elements(Q).first, elements(Q).second, 0);
        
    }
    
    template<typename T, class MemoryBlock>
    dense_matrix<T, MemoryBlock> conjugate(dense_matrix<T, MemoryBlock> M)
    {
        M.inplace_conjugate();
        return M;
    }
    
    template<typename T, class MemoryBlock>
    void syev(dense_matrix<T, MemoryBlock> M,
              dense_matrix<T, MemoryBlock> & evecs,
              std::vector<double> & evals)
    {
        assert(num_rows(M) == num_columns(M));
        assert(evals.size() == num_rows(M));
        boost::numeric::bindings::lapack::syevd('V', M, evals);
        // to be consistent with the SVD, I reorder in decreasing order
        std::reverse(evals.begin(), evals.end());
        // and the same with the matrix
        evecs.resize(num_rows(M), num_columns(M));
        for (std::size_t c = 0; c < num_columns(M); ++c)
			std::copy(column(M, c).first, column(M, c).second,
                      column(evecs, num_columns(M)-1-c).first);
    }
    
    template<typename T, class MemoryBlock>
    void syev(dense_matrix<T, MemoryBlock> M,
              dense_matrix<T, MemoryBlock> & evecs,
              typename associated_diagonal_matrix<dense_matrix<T, MemoryBlock> >::type & evals)
    {
        assert(num_rows(M) == num_columns(M));
        std::vector<double> evals_(num_rows(M));
        syev(M, evecs, evals_);
        evals = typename associated_diagonal_matrix<dense_matrix<T, MemoryBlock> >::type(evals_);
    }
    
} /* namespace blas */

#endif
