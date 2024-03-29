/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2011-2011 by Bela Bauer <bauerb@phys.ethz.ch>
 * 
 * This software is part of the ALPS Applications, published under the ALPS
 * Application License; you can use, redistribute it and/or modify it under
 * the terms of the license, either version 1 or (at your option) any later
 * version.
 * 
 * You should have received a copy of the ALPS Application License along with
 * the ALPS Applications; see the file LICENSE.txt. If not, the license is also
 * available from http://alps.comp-phys.org/.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#include "dmrg/block_matrix/block_matrix.h"

#include "utils/function_objects.h"
#include "utils/bindings.hpp"

#include <alps/type_traits/is_complex.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/ptr_container/serialize_ptr_vector.hpp>

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup>::block_matrix()
{
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup>::block_matrix(Index<SymmGroup> const & rows,
                                              Index<SymmGroup> const & cols)
{
    assert(rows.size() == cols.size());

    basis_.resize(rows.size());
    for (size_type k = 0; k < rows.size(); ++k)
        basis_[k] = typename DualIndex<SymmGroup>::value_type(rows[k].first, cols[k].first, rows[k].second, cols[k].second);

    for (size_type k = 0; k < rows.size(); ++k)
        data_.push_back(new Matrix(basis_[k].ls, basis_[k].rs));
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup>::block_matrix(DualIndex<SymmGroup> const & basis)
: basis_(basis)
{
    for (size_type k = 0; k < basis_.size(); ++k)
        data_.push_back(new Matrix(basis_[k].ls, basis_[k].rs));
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup>::block_matrix(block_matrix const& rhs)
: basis_(rhs.basis())
, data_(rhs.data_)
{
}

template<class Matrix, class SymmGroup>
template <class OtherMatrix>
block_matrix<Matrix, SymmGroup>::block_matrix(block_matrix<OtherMatrix, SymmGroup> const& rhs)
: basis_(rhs.basis())
{
    data_.reserve(rhs.n_blocks());
    for (size_type k = 0; k < rhs.n_blocks(); ++k)
        data_.push_back(new Matrix(rhs[k]));
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup> & block_matrix<Matrix, SymmGroup>::operator=(block_matrix rhs)
{
    swap(*this, rhs);
    return *this;
}

template<class Matrix, class SymmGroup>
template<class OtherMatrix>
block_matrix<Matrix, SymmGroup> & block_matrix<Matrix, SymmGroup>::operator=(const block_matrix<OtherMatrix, SymmGroup> & rhs)
{
    basis_ = rhs.basis();
    data_.resize(rhs.n_blocks());
    for(int k = 0; k < data_.size(); k++){
        Matrix tmp(rhs[k]);
        swap(data_[k], tmp);
    }
    return *this;
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup> & block_matrix<Matrix, SymmGroup>::operator+=(block_matrix const & rhs)
{
    for (size_type k = 0; k < rhs.n_blocks(); ++k)
    {
        charge rhs_rc = rhs.basis_[k].lc;
        charge rhs_cc = rhs.basis_[k].rc;
        if (this->has_block(rhs_rc, rhs_cc))
            (*this)(rhs_rc, rhs_cc) += rhs.data_[k];
        else
            this->insert_block(rhs.data_[k], rhs_rc, rhs_cc);
    }
    return *this;
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup> & block_matrix<Matrix, SymmGroup>::operator-=(block_matrix const & rhs)
{
    for (size_type k = 0; k < rhs.n_blocks(); ++k)
    {
        charge rhs_rc = rhs.basis_[k].lc;
        charge rhs_cc = rhs.basis_[k].rc;
        if (this->has_block(rhs_rc, rhs_cc))
            (*this)(rhs_rc, rhs_cc) -= rhs.data_[k];
        else
            this->insert_block(-1*rhs.data_[k], rhs_rc, rhs_cc);
    }
    return *this;
}

template<class Matrix, class SymmGroup>
typename block_matrix<Matrix, SymmGroup>::size_type block_matrix<Matrix, SymmGroup>::insert_block(Matrix const & mtx, charge c1, charge c2)
{
    assert( !has_block(c1, c2) );
    size_type i1 = basis_.insert(typename DualIndex<SymmGroup>::value_type(c1, c2, num_rows(mtx), num_cols(mtx)));
    Matrix* block = new Matrix(mtx);
    data_.insert(data_.begin() + i1, block);
    
    return i1;
}

template<class Matrix, class SymmGroup>
typename block_matrix<Matrix, SymmGroup>::size_type block_matrix<Matrix, SymmGroup>::insert_block(Matrix * mtx, charge c1, charge c2)
{
    assert( !has_block(c1, c2) );
    size_type i1 = basis_.insert(typename DualIndex<SymmGroup>::value_type(c1, c2, num_rows(*mtx), num_cols(*mtx)));
    data_.insert(data_.begin() + i1, mtx);
    
    return i1;
}

template<class Matrix, class SymmGroup>
Index<SymmGroup> block_matrix<Matrix, SymmGroup>::left_basis() const
{ 
    Index<SymmGroup> ret(basis_.size());
    for (size_type s = 0; s < basis_.size(); ++s)
        ret[s] = std::make_pair(basis_[s].lc, basis_[s].ls);

    return ret;
}

template<class Matrix, class SymmGroup>
Index<SymmGroup> block_matrix<Matrix, SymmGroup>::right_basis() const
{
    Index<SymmGroup> ret(basis_.size());
    for (size_type s = 0; s < basis_.size(); ++s)
        ret[s] = std::make_pair(basis_[s].rc, basis_[s].rs);

    return ret;
}


template<class Matrix, class SymmGroup>
std::vector<const typename Matrix::value_type*> block_matrix<Matrix, SymmGroup>::data_view() const
{
    std::vector<const value_type*> ret(n_blocks());
    for (size_t i = 0; i < n_blocks(); ++i)
        ret[i] = (*this)[i].get_values().data();
    return ret;
}

template<class Matrix, class SymmGroup>
std::vector<typename Matrix::value_type*> block_matrix<Matrix, SymmGroup>::data_view_nc()
{
    std::vector<value_type*> ret(n_blocks());
    for (size_t i = 0; i < n_blocks(); ++i)
        ret[i] = (*this)[i].get_values().data();
    return ret;
}

template<class Matrix, class SymmGroup>
DualIndex<SymmGroup> const & block_matrix<Matrix, SymmGroup>::basis() const { return basis_; }

template<class Matrix, class SymmGroup>
typename Matrix::size_type block_matrix<Matrix, SymmGroup>::n_blocks() const { return data_.size(); }

template<class Matrix, class SymmGroup>
std::string block_matrix<Matrix, SymmGroup>::description() const
{
    std::ostringstream oss;
    oss << basis_;
    return oss.str();
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::shift_basis(typename block_matrix<Matrix, SymmGroup>::charge diff)
{
    basis_.shift(diff);
}

template<class Matrix, class SymmGroup>
Matrix & block_matrix<Matrix, SymmGroup>::operator[](size_type c) { return data_[c]; }

template<class Matrix, class SymmGroup>
Matrix const & block_matrix<Matrix, SymmGroup>::operator[](size_type c) const { return data_[c]; }

template<class Matrix, class SymmGroup>
typename block_matrix<Matrix, SymmGroup>::size_type block_matrix<Matrix, SymmGroup>::find_block(charge r, charge c) const
{
    assert(basis_.size() == data_.size());
    return basis_.position(r,c);
}

template<class Matrix, class SymmGroup>
bool block_matrix<Matrix, SymmGroup>::has_block(charge r, charge c) const
{
    return basis_.position(r,c) != basis_.size();
}

template<class Matrix, class SymmGroup>
bool block_matrix<Matrix, SymmGroup>::has_block(std::pair<charge, size_type> const & r,
                                                std::pair<charge, size_type> const & c) const
{
    return has_block(r.first, c.first);
}

template<class Matrix, class SymmGroup>
typename Matrix::value_type & block_matrix<Matrix, SymmGroup>::operator()(std::pair<charge, size_type> const & r,
                                                                          std::pair<charge, size_type> const & c)
{
    return data_[basis_.position(r.first, c.first)](r.second, c.second);
}

template<class Matrix, class SymmGroup>
typename Matrix::value_type const & block_matrix<Matrix, SymmGroup>::operator()(std::pair<charge, size_type> const & r,
                                                                                std::pair<charge, size_type> const & c) const
{
    return data_[basis_.position(r.first, c.first)](r.second, c.second);
}
// Remove by Tim 06/08/2012, presently not used in any DMRG/TE code
//template<class Matrix, class SymmGroup>
//void block_matrix<Matrix, SymmGroup>::remove_rows_from_block(size_type block, size_type r, size_type k)
//{ // we should add an assert block < data_.size()
//    remove_rows(data_[block], r, k);
//    rows_[block].second -= k;
//}

// Remove by Tim 06/08/2012, presently not used in any DMRG/TE code
//template<class Matrix, class SymmGroup>
//void block_matrix<Matrix, SymmGroup>::remove_cols_from_block(size_type block, size_type r, size_type k)
//{ // we should add an assert block < data_.size()
//    remove_cols(data_[block], r, k);
//    cols_[block].second -= k;
//}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup> const & block_matrix<Matrix, SymmGroup>::operator*=(const scalar_type& v)
{
    for(size_t k = 0; k < n_blocks(); ++k)
        data_[k] *= v;

    return *this;
}

template<class Matrix, class SymmGroup>
block_matrix<Matrix, SymmGroup> const & block_matrix<Matrix, SymmGroup>::operator/=(const scalar_type& v)
{
    for(size_t k = 0; k < n_blocks(); ++k)
        data_[k] /= v;
    return *this;
}


template<class Matrix, class SymmGroup>
typename block_matrix<Matrix, SymmGroup>::scalar_type block_matrix<Matrix, SymmGroup>::trace() const
{
    std::vector<scalar_type> vt; vt.reserve(data_.size());
    std::transform(data_.begin(), data_.end(), back_inserter(vt), utils::functor_trace());
    return maquis::accumulate(vt.begin(), vt.end(), scalar_type(0.));
}

template<class Matrix, class SymmGroup>
typename block_matrix<Matrix, SymmGroup>::real_type block_matrix<Matrix, SymmGroup>::norm() const
{
    std::vector<real_type> vt; vt.reserve(data_.size());
    for(size_t k = 0; k < n_blocks(); ++k){
        vt.push_back(norm_square(data_[k]));
    }
    return maquis::sqrt(maquis::accumulate(vt.begin(), vt.end(), real_type(0.)));
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::transpose_inplace()
{
    block_matrix<Matrix, SymmGroup> new_bm;
    for (size_type i=0; i < basis_.size(); ++i) {
        new_bm.insert_block(utils::functor_transpose()(data_[i]), basis_[i].rc, basis_[i].lc);
    }
    swap(*this, new_bm);
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::conjugate_inplace()
{
    std::for_each(data_.begin(), data_.end(), utils::functor_conj_inplace());
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::adjoint_inplace()
{
    std::for_each(data_.begin(), data_.end(), utils::functor_adjoint_inplace());
    for (size_type i=0; i < basis_.size(); ++i) {
        std::swap(basis_[i].lc, basis_[i].rc);
        std::swap(basis_[i].ls, basis_[i].rs);
    }
}

template<class Matrix, class SymmGroup>
template<class Generator>
void block_matrix<Matrix, SymmGroup>::generate(Generator g)
{
    for(size_type k = 0; k < n_blocks(); ++k) maquis::dmrg::detail::generate_impl(data_[k], g);
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::cleanup_zeros(value_type const& tol)
{
    for (size_type i=n_blocks(); i >=1; --i) {
        const size_type k = i-1;
        const size_type nzeros = maquis::dmrg::detail::zeroout((*this)[k], tol);
        if (nzeros == num_rows((*this)[k])*num_cols((*this)[k]))
            remove_block(k);
    }
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::clear()
{
    data_.clear();
    basis_ = DualIndex<SymmGroup>();
}

template<class Matrix, class SymmGroup>
std::ostream& operator<<(std::ostream& os, block_matrix<Matrix, SymmGroup> const & m)
{
    os << "Basis: " << m.basis() << std::endl;
    for (std::size_t k = 0; k < m.n_blocks(); ++k)
        os << "Block (" << m.basis()[k].lc << "," << m.basis()[k].rc
           << "):\n" << m[k] << std::endl;
    os << std::endl;
    return os;
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::match_and_add_block(Matrix const & mtx, charge c1, charge c2)
{
    size_type match = this->find_block(c1, c2);
    if (match < this->n_blocks())
    {
        if (num_rows(mtx) == num_rows((*this)[match]) &&
            num_cols(mtx) == num_cols((*this)[match]))
            (*this)[match] += mtx;
        else if (num_rows(mtx) > num_rows((*this)[match]) &&
                 num_cols(mtx) > num_cols((*this)[match]))
        {
            resize_block(match, num_rows(mtx), num_cols(mtx));
            (*this)[match] += mtx;
        } else {
            size_type maxrows = std::max(num_rows(mtx),
                                           num_rows((*this)[match]));
            size_type maxcols = std::max(num_cols(mtx),
                                           num_cols((*this)[match]));
            
            Matrix cpy(mtx); // only in this case do we need to copy the argument matrix
            
            resize_block(match, maxrows, maxcols);
            resize(cpy, maxrows, maxcols);
            
            (*this)[match] += cpy;
        }
    } else
        insert_block(mtx, c1, c2);
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::resize_block(charge r, charge c,
                                                   size_type new_r, size_type new_c,
                                                   bool pretend)
{
    resize_block(find_block(r, c), new_r, new_c, pretend);
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::resize_block(size_type pos,
                                                   size_type new_r, size_type new_c,
                                                   bool pretend)
{
    if (!pretend)
        resize((*this)[pos], new_r, new_c);

    basis_[pos].ls = new_r;
    basis_[pos].rs = new_c;
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::remove_block(charge r, charge c)
{
    assert( has_block(r, c) );
    
    size_type which = basis_.position(r,c);
    
    basis_.erase(basis_.begin() + which);
    data_.erase(data_.begin() + which);
}

template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::remove_block(size_type which)
{
    assert( which < data_.size() );

    basis_.erase(basis_.begin() + which);
    data_.erase(data_.begin() + which);
}

template<class Matrix, class SymmGroup>
template<class Archive>
void block_matrix<Matrix, SymmGroup>::load(Archive & ar)
{
    Index<SymmGroup> r_, c_;
    ar["rows_"] >> r_;
    ar["cols_"] >> c_;

    basis_.resize(r_.size());
    for (size_type s = 0; s < r_.size(); ++s)
        basis_[s] = typename DualIndex<SymmGroup>::value_type(r_[s].first, c_[s].first, r_[s].second, c_[s].second);

    data_.clear();
    if (alps::is_complex<typename Matrix::value_type>() && !ar.is_complex("data_"))
    {
        typedef typename alps::numeric::matrix<typename alps::numeric::real_type<typename Matrix::value_type>::type> LoadMatrix;
        std::vector<LoadMatrix> tmp;
        ar["data_"] >> tmp;
        for(typename std::vector<LoadMatrix>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
            data_.push_back(new Matrix(maquis::bindings::matrix_cast<Matrix>(*it)));
    } else {
        std::vector<Matrix> tmp;
        ar["data_"] >> tmp;
        // TODO: is swap here possible?
        for(typename std::vector<Matrix>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
            data_.push_back(new Matrix(*it));
    }
}

template<class Matrix, class SymmGroup>
template<class Archive>
void block_matrix<Matrix, SymmGroup>::save(Archive & ar) const
{
    Index<SymmGroup> r_ = left_basis();
    Index<SymmGroup> c_ = right_basis();
    ar["rows_"] << r_;
    ar["cols_"] << c_;

    std::vector<Matrix> tmp(data_.begin(), data_.end());
    ar["data_"] << tmp;
}

template<class Matrix, class SymmGroup>
template <class Archive>
void block_matrix<Matrix, SymmGroup>::serialize(Archive & ar, const unsigned int version)
{
    ar & basis_ & data_;
}


template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::reserve(charge c1, charge c2,
                                              size_type r, size_type c)
{
    if (this->has_block(c1, c2))
    {
        size_type pos = basis_.position(c1, c2);
        size_type maxrows = std::max(basis_[pos].ls, r);
        size_type maxcols = std::max(basis_[pos].rs, c);
    
        basis_[pos].ls = maxrows;
        basis_[pos].rs = maxcols;
    } else {
        
        assert(basis_.size() == data_.size());
        
        size_type i1 = basis_.insert(typename DualIndex<SymmGroup>::value_type(c1, c2, r, c));
        Matrix* block = new Matrix(1,1);
        data_.insert(data_.begin() + i1, block); 
    }
    assert( this->has_block(c1,c2) );
}

template<class Matrix, class SymmGroup>
inline void block_matrix<Matrix, SymmGroup>::reserve_pos(charge c1, charge c2,
                                                         size_type ri, size_type ci)
{ reserve(c1, c2, ri+1, ci+1); }


template<class Matrix, class SymmGroup>
void block_matrix<Matrix, SymmGroup>::allocate_blocks()
{
    assert(basis_.size() == n_blocks());
    for (size_type k = 0; k < n_blocks(); ++k)
        resize(data_[k], basis_[k].ls, basis_[k].rs);
}

template<class Matrix, class SymmGroup>
bool block_matrix<Matrix, SymmGroup>::reasonable() const
{
    for (size_type k=0; k<n_blocks(); ++k)
        if (num_rows((*this)[k]) != basis_[k].ls || num_cols((*this)[k]) != basis_[k].rs)
            return false;
    return true;
}

template<class Matrix, class SymmGroup>
std::size_t block_matrix<Matrix, SymmGroup>::num_elements() const
{
    std::size_t ret = 0;
    for (size_type k = 0; k < n_blocks(); ++k)
        ret += num_rows(data_[k])*num_cols(data_[k]);
    return ret;
}
