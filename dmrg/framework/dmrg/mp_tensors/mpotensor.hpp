/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2013-2013 by Bela Bauer <bauerb@phys.ethz.ch>
 *                            Sebastian Keller <sebkelle@phys.ethz.ch>
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

template<class Matrix, class SymmGroup>
MPOTensor<Matrix, SymmGroup>::MPOTensor(index_type ld,
                                        index_type rd,
                                        prempo_t tags,
                                        op_table_ptr tbl_,
                                        BondProperty const& lb, 
                                        BondProperty const& rb)
: left_i(ld)
, right_i(rd)
, col_tags(ld, rd)
, operator_table(tbl_)
, leftbond(ld)
, rightbond(rd)
{
    using namespace boost::tuples;
    row_index.resize(ld);

    if (tags.size() > 0 && operator_table.get() != NULL) {

        // sort tags in order used by the CSC (sparse) matrix
        std::sort(tags.begin(), tags.end(), MPOTensor_detail::col_cmp<typename prempo_t::value_type>());

        for (typename prempo_t::const_iterator it = tags.begin(); it != tags.end(); ++it) {
            internal_value_type & element = col_tags(get<0>(*it), get<1>(*it)).ref();
            if (element.size() == 0)
            {
                element = internal_value_type(1, std::make_pair(get<2>(*it), get<3>(*it)));
                row_index[get<0>(*it)].insert(get<1>(*it));
            }
            else {
                // avoid resize, as that might increase the capacity beyond the new size
                internal_value_type new_element(element.size() + 1);
                std::copy(element.begin(), element.end(), new_element.begin()+1);
                *new_element.begin() = std::make_pair(get<2>(*it), get<3>(*it));
                std::swap(element, new_element);
            }
        }
        for (std::size_t i = 0; i < operator_table->size(); ++i)
            operator_table->operator[](i).update_sparse();
    }
    else {
        // Initialize a private operator table
        operator_table = op_table_ptr(new OPTable<Matrix, SymmGroup>());
    }

    if (lb.size() == row_dim() && rb.size() == col_dim())
    {
        leftbond = lb;
        rightbond = rb;
    }

    // provide information about number of non-zeros in rows and columns
    row_non_zeros.resize(row_dim());
    col_non_zeros.resize(col_dim());
    for (index_type b2 = 0; b2 < col_dim(); ++b2)
    {
        col_proxy col_b2 = column(b2);
        for (typename col_proxy::const_iterator col_it = col_b2.begin(); col_it != col_b2.end(); ++col_it)
        {
            index_type b1 = col_it.index();
            row_non_zeros[b1]++;
            col_non_zeros[b2]++;
        }
    }

    num_one_rows_ = std::count(row_non_zeros.begin(), row_non_zeros.end(), 1);
    num_one_cols_ = std::count(col_non_zeros.begin(), col_non_zeros.end(), 1);
}

template<class Matrix, class SymmGroup>
bool MPOTensor<Matrix, SymmGroup>::has(index_type left_index,
                                       index_type right_index) const
{
    assert(left_index < left_i && right_index < right_i);
    return col_tags.find_element(left_index, right_index) != NULL;
}

// warning: this method allows to (indirectly) change the op in the table, all tags pointing to it will
//          get a modified matrix!
//          better design needed
template<class Matrix, class SymmGroup>
void MPOTensor<Matrix, SymmGroup>::set(index_type li, index_type ri, op_t const & op, value_type scale_){
    if (this->has(li, ri)) {
        (*col_tags.find_element(li, ri))[0].second = scale_;
        tag_type tag = (*col_tags.find_element(li, ri))[0].first;
        (*operator_table)[tag] = op;
        (*operator_table)[tag].update_sparse();
    }
    else {
        tag_type new_tag = operator_table->register_op(op);
        (*operator_table)[new_tag].update_sparse();
        col_tags(li, ri) = internal_value_type(1, std::make_pair(new_tag, scale_));
        row_index[li].insert(ri);
    }
}

template<class Matrix, class SymmGroup>
MPOTensor_detail::term_descriptor<Matrix, SymmGroup, true>
MPOTensor<Matrix, SymmGroup>::at(index_type left_index, index_type right_index) const {
    assert(this->has(left_index, right_index));
    typename CSCMatrix::value_type const & p = col_tags(left_index, right_index);
    return MPOTensor_detail::term_descriptor<Matrix, SymmGroup, true>(p, operator_table);
}

// warning: this method allows to (indirectly) change the op in the table, all tags pointing to it will
//          get a modified matrix!
//          better design needed
template<class Matrix, class SymmGroup>
MPOTensor_detail::term_descriptor<Matrix, SymmGroup, false>
MPOTensor<Matrix, SymmGroup>::at(index_type left_index, index_type right_index) {
    if (!this->has(left_index, right_index))
        this->set(left_index, right_index, op_t(), 1.);
    typename CSCMatrix::value_type & p = col_tags(left_index, right_index).ref();
    return MPOTensor_detail::term_descriptor<Matrix, SymmGroup, false>(p, operator_table);
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::row_proxy MPOTensor<Matrix, SymmGroup>::row(index_type row_i) const
{  
    return row_proxy(row_index[row_i].begin(), row_index[row_i].end());
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::col_proxy MPOTensor<Matrix, SymmGroup>::column(index_type col_i) const
{  
    return col_proxy(col_tags, col_i);
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::tag_type
MPOTensor<Matrix, SymmGroup>::tag_number(index_type left_index, index_type right_index, size_t index) const {
    assert (index < col_tags(left_index, right_index).size());
    return col_tags(left_index, right_index)[index].first;
}


template<class Matrix, class SymmGroup>
void MPOTensor<Matrix, SymmGroup>::multiply_by_scalar(value_type v)
{
    for (typename CSCMatrix::iterator2 it2 = col_tags.begin2(); it2 != col_tags.end2(); ++it2)
        for (typename CSCMatrix::iterator1 it1 = it2.begin(); it1 != it2.end(); ++it1)
            std::for_each((*it1).begin(), (*it1).end(), boost::lambda::bind(&std::pair<tag_type, value_type>::second, boost::lambda::_1) *= v);
}

template<class Matrix, class SymmGroup>
void MPOTensor<Matrix, SymmGroup>::divide_by_scalar(value_type v)
{
    for (typename CSCMatrix::iterator2 it2 = col_tags.begin2(); it2 != col_tags.end2(); ++it2)
        for (typename CSCMatrix::iterator1 it1 = it2.begin(); it1 != it2.end(); ++it1)
            std::for_each((*it1).begin(), (*it1).end(), boost::lambda::bind(&std::pair<tag_type, value_type>::second, boost::lambda::_1) /= v);
}


// getters

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::op_table_ptr MPOTensor<Matrix, SymmGroup>::get_operator_table() const
{
    return operator_table;
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::index_type MPOTensor<Matrix, SymmGroup>::num_row_non_zeros(index_type row_i) const
{
    return row_non_zeros[row_i];
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::index_type MPOTensor<Matrix, SymmGroup>::num_col_non_zeros(index_type col_i) const
{
    return col_non_zeros[col_i];
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::index_type MPOTensor<Matrix, SymmGroup>::num_one_rows() const
{
    return num_one_rows_;
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::index_type MPOTensor<Matrix, SymmGroup>::num_one_cols() const
{
    return num_one_cols_;
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::index_type MPOTensor<Matrix, SymmGroup>::row_dim() const
{
    return left_i;
}

template<class Matrix, class SymmGroup>
typename MPOTensor<Matrix, SymmGroup>::index_type MPOTensor<Matrix, SymmGroup>::col_dim() const
{
    return right_i;
}


// BondProperties

template<class Matrix, class SymmGroup>
typename MPOTensor_detail::BondProperty<SymmGroup> const& MPOTensor<Matrix, SymmGroup>::leftBond() const
{
    return leftbond;
}

template<class Matrix, class SymmGroup>
typename MPOTensor_detail::BondProperty<SymmGroup> const& MPOTensor<Matrix, SymmGroup>::rightBond() const
{
    return rightbond;
}



// serialization

template<class Matrix, class SymmGroup>
template<class Archive>
void MPOTensor<Matrix, SymmGroup>::serialize(Archive & ar, const unsigned int version)
{
    ar & left_i & right_i & leftbond & rightbond
       & row_non_zeros & col_non_zeros & col_tags & row_index & operator_table;
}

