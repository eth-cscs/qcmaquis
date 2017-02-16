/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2017 Department of Chemistry and the PULSE Institute, Stanford University
 *                    Laboratory for Physical Chemistry, ETH Zurich
 *               2017-2017 by Sebastian Keller <sebkelle@phys.ethz.ch>
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

#ifndef ENGINE_COMMON_TASKS_HPP
#define ENGINE_COMMON_TASKS_HPP

#include <vector>
#include <map>
#include <utility>

#include "utils/sizeof.h"

namespace contraction {

namespace common {

    //forward declaration

    template <class Matrix, class SymmGroup>
    class ContractionGroup;

}

namespace SU2 {

    // forward declaration

    template <class Matrix, class OtherMatrix, class SymmGroup>
    void shtm_tasks(MPOTensor<Matrix, SymmGroup> const & mpo,
                    common::LeftIndices<Matrix, OtherMatrix, SymmGroup> const & left,
                    common::RightIndices<Matrix, OtherMatrix, SymmGroup> const & right,
                    DualIndex<SymmGroup> const &,
                    Index<SymmGroup> const &,
                    ProductBasis<SymmGroup> const &,
                    typename SymmGroup::charge,
                    typename SymmGroup::charge,
                    unsigned,
                    common::ContractionGroup<Matrix, SymmGroup> &);
}

namespace common {

    namespace detail { 

        template <typename T>
        struct micro_task
        {
            typedef unsigned short IS;

            T scale;
            unsigned in_offset;
            IS b2, k, l_size, r_size, stripe, out_offset;
        };

    } // namespace detail

    template <typename T>
    struct task_compare
    {
        bool operator ()(detail::micro_task<T> const & t1, detail::micro_task<T> const & t2)
        {
            return t1.out_offset < t2.out_offset;
        }
    };

    template <class Matrix, class SymmGroup>
    struct task_capsule : public std::map<
                                          std::pair<typename SymmGroup::charge, typename SymmGroup::charge>,
                                          std::vector<detail::micro_task<typename Matrix::value_type> >,
                                          compare_pair<std::pair<typename SymmGroup::charge, typename SymmGroup::charge> >
                                         >
    {
        typedef typename SymmGroup::charge charge;
        typedef typename Matrix::value_type value_type;
        typedef detail::micro_task<value_type> micro_task;
        typedef std::map<std::pair<charge, charge>, std::vector<micro_task>, compare_pair<std::pair<charge, charge> > > map_t;
    };

    template <class Matrix, class SymmGroup>
    struct Schedule
    {
        typedef std::vector<contraction::common::task_capsule<Matrix, SymmGroup> > schedule_t;
    }; 

    template <class Matrix, class SymmGroup>
    class MatrixGroup
    {
        typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
        typedef typename task_capsule<Matrix, SymmGroup>::micro_task micro_task;

    public:
        void add_line(unsigned b1, unsigned k, bool check = false)
        {
            // if size is zero or we see a new b1 for the first time and the previous b1 did yield terms
            if (bs.size() == 0 || (*bs.rbegin() != b1 && tasks.rbegin()->size() > 0))
            {
                bs.push_back(b1);
                ks.push_back(k);
                tasks.push_back(std::vector<micro_task>());
                if (check)
                {
                    maquis::cout << "b1 = " << b1 << ", bs add ";
                    std::copy(bs.begin(), bs.end(), std::ostream_iterator<index_type>(std::cout, " "));
                    maquis::cout << std::endl;
                }
            }
            // if the previous b1 didnt yield any terms overwrite it with the new b1
            else if (*bs.rbegin() != b1 && tasks.rbegin()->size() == 0)
            {
                *bs.rbegin() = b1;
                *ks.rbegin() = k;
                if (check)
                {
                    maquis::cout << "b1 = " << b1 << ", bs ovw ";
                    std::copy(bs.begin(), bs.end(), std::ostream_iterator<index_type>(std::cout, " "));
                    maquis::cout << std::endl;
                }
            }
        }

        void push_back(micro_task mt)
        {
            assert(tasks.size() > 0);
            tasks[tasks.size()-1].push_back(mt);
        }

        std::vector<micro_task> & current_row()
        {
            return tasks[tasks.size()-1];
        }

        void print_stats() const
        {
            typedef boost::tuple<unsigned, unsigned, unsigned> triple;
            typedef std::map<triple, unsigned> amap_t;

            unsigned cnt = 0;
            amap_t b2_col;
            for (int i = 0; i < tasks.size(); ++i)
                for (int j = 0; j < tasks[i].size(); ++j)
                {
                    triple tt(tasks[i][j].b2, tasks[i][j].k, tasks[i][j].in_offset); 
                    if (b2_col.count(tt) == 0)
                        b2_col[tt] = cnt++;
                }

            alps::numeric::matrix<unsigned> alpha(tasks.size(), b2_col.size(), 0);
            for (int i = 0; i < tasks.size(); ++i)
                for (int j = 0; j < tasks[i].size(); ++j)
                {
                    triple tt(tasks[i][j].b2, tasks[i][j].k, tasks[i][j].in_offset); 
                    alpha(i, b2_col[tt]) = 1;
                }

            maquis::cout << "      ";
            for (amap_t::const_iterator it = b2_col.begin(); it != b2_col.end(); ++it)
                maquis::cout << std::setw(4) << boost::get<2>(it->first);
            maquis::cout << std::endl;
            maquis::cout << "      ";
            for (amap_t::const_iterator it = b2_col.begin(); it != b2_col.end(); ++it)
                maquis::cout << std::setw(4) << boost::get<1>(it->first);
            maquis::cout << std::endl;
            maquis::cout << "      ";
            for (amap_t::const_iterator it = b2_col.begin(); it != b2_col.end(); ++it)
                maquis::cout << std::setw(4) << boost::get<0>(it->first);
            maquis::cout << std::endl;

            std::string hline(6 + 4 * b2_col.size(), '_');
            maquis::cout << hline << std::endl;

            for (int i = 0; i < bs.size(); ++i)
            {
                maquis::cout << std::setw(4) << bs[i] << "| ";
                for (amap_t::const_iterator it = b2_col.begin(); it != b2_col.end(); ++it)
                {
                    int col = it->second;
                    int val = alpha(i, col);
                    if (val == 0)
                        maquis::cout << std::setw(4) << "."; 
                    else
                        maquis::cout << std::setw(4) << alpha(i, col); 
                }
                maquis::cout << std::endl;
            }
            maquis::cout << std::endl << std::endl;
        }
        
    private:
        std::vector<std::vector<micro_task> > tasks;
        std::vector<index_type> bs, ks;
    };

    template <class Matrix, class SymmGroup>
    class ContractionGroup
    {
        typedef typename SymmGroup::charge charge;
        typedef boost::tuple<unsigned, charge> duple;

    public:
        

    //private:
        std::vector<Matrix> T;
        std::map<duple, MatrixGroup<Matrix, SymmGroup> > mgroups;
    };
    
    template<class Matrix, class SymmGroup, class TaskCalc>
    typename Schedule<Matrix, SymmGroup>::schedule_t
    create_contraction_schedule(MPSTensor<Matrix, SymmGroup> const & initial,
                                Boundary<typename storage::constrained<Matrix>::type, SymmGroup> const & left,
                                Boundary<typename storage::constrained<Matrix>::type, SymmGroup> const & right,
                                MPOTensor<Matrix, SymmGroup> const & mpo,
                                TaskCalc task_calc)
    {
        typedef typename storage::constrained<Matrix>::type SMatrix;
        typedef typename SymmGroup::charge charge;
        typedef typename Matrix::value_type value_type;
        typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
        typedef typename task_capsule<Matrix, SymmGroup>::map_t map_t;
        typedef typename task_capsule<Matrix, SymmGroup>::micro_task micro_task;

        initial.make_left_paired();

        typename Schedule<Matrix, SymmGroup>::schedule_t contraction_schedule(mpo.row_dim());
        MPSBoundaryProductIndices<Matrix, SMatrix, SymmGroup> indices(initial.data().basis(), right, mpo);
        LeftIndices<Matrix, SMatrix, SymmGroup> left_indices(left, mpo);
        RightIndices<Matrix, SMatrix, SymmGroup> right_indices(right, mpo);

        // MPS indices
        Index<SymmGroup> const & physical_i = initial.site_dim(),
                                 right_i = initial.col_dim();
        Index<SymmGroup> left_i = initial.row_dim(),
                         out_right_i = adjoin(physical_i) * right_i;

        common_subset(out_right_i, left_i);
        ProductBasis<SymmGroup> in_left_pb(physical_i, left_i);
        ProductBasis<SymmGroup> out_right_pb(physical_i, right_i,
                                             boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                                                 -boost::lambda::_1, boost::lambda::_2));
        index_type loop_max = mpo.row_dim();
        omp_for(index_type b1, parallel::range<index_type>(0,loop_max), {
            task_capsule<Matrix, SymmGroup> tasks;
            task_calc(b1, indices, mpo, left_indices[b1], left_i, out_right_i, in_left_pb, out_right_pb, tasks);

            for (typename map_t::iterator it = tasks.begin(); it != tasks.end(); ++it)
                std::sort((it->second).begin(), (it->second).end(), task_compare<value_type>());

            contraction_schedule[b1] = tasks;
        });

        size_t sz = 0, data = 0;
        for (int b1 = 0; b1 < loop_max; ++b1)
        {
            task_capsule<Matrix, SymmGroup> const & tasks = contraction_schedule[b1];
            for (typename map_t::const_iterator it = tasks.begin(); it != tasks.end(); ++it)
            {
                sz += (it->second).size() * sizeof(micro_task);
                for (typename map_t::mapped_type::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2)
                    data += 8 * it2->r_size * it2->l_size;
            }            
        }
        maquis::cout << "Schedule size: " << sz / 1024 << "KB " << data / 1024 / 1024 <<  "MB "
                                          << (data * 24) / sz / 1024 << "KB "
                                          << "T " << 8*::utils::size_of(indices.begin(), indices.end())/1024/1024 << "MB "
                                          << "R " << 8*size_of(right)/1024/1024 << "MB "
                                          << initial.data().n_blocks() << " MPS blocks" << std::endl;


        /*
        { // separate scope

        // input_per_mps , for each location in the output MPS, list which input blocks from S and T are required

        typedef typename DualIndex<SymmGroup>::const_iterator const_iterator;

        typedef boost::tuple<unsigned, unsigned, unsigned> triple;
        typedef std::map<triple, unsigned> map4;
        typedef std::map<charge, map4> map3;
        typedef std::map<unsigned, map3> map2;
        typedef std::map<charge, map2> map1;
        map1 stasks; // [outcharge][outoffset][middlecharge][input_triple]

        std::map<charge, unsigned> middle_size;

        // MPS block
        for (int lb = 0; lb < left_i.size(); ++lb)
        {
            charge out_charge = left_i[lb].first;
             
            // loop over boundary
            for (int b1 = 0; b1 < loop_max; ++b1)
            {
                // find connecting middle charge
                int cnt = 0;
                const_iterator lit = left[b1].basis().left_lower_bound(out_charge);
                for ( ; lit != left[b1].basis().end() && lit->lc == out_charge; ++lit)
                {
                    charge middle_charge = lit->rc;
                    size_t ms = lit->rs;
                    middle_size[middle_charge] = ms;
                
                    // find out_charge in contraction_schedule[b1]
                    std::vector<detail::micro_task<value_type> > const & tvec
                        = contraction_schedule[b1][std::make_pair(middle_charge, out_charge)];
                    for (int i = 0; i < tvec.size(); ++i)
                        //stasks[out_charge][tvec[i].out_offset][boost::make_tuple(tvec[i].b2, tvec[i].k, tvec[i].in_offset)]++;
                        stasks[out_charge][tvec[i].out_offset][middle_charge][boost::make_tuple(tvec[i].b2, tvec[i].k, tvec[i].in_offset)]++;

                    cnt++; 
                }
                //if (cnt > 1) { maquis::cout << left[b1].basis() << std::endl; exit(1); }
            }
        }
        

        if (mpo.row_dim() == 178)
        {
            std::ofstream ips(("ips" + boost::lexical_cast<std::string>(initial.sweep)).c_str());
            for (typename map1::const_iterator it1 = stasks.begin();
                  it1 != stasks.end(); ++it1)
            {
                ips << "MPS charge " << it1->first << ", ls " << left_i.size_of_block(it1->first) << std::endl;
                for (typename map2::const_iterator it2 = it1->second.begin();
                   it2 != it1->second.end(); ++it2)
                {
                    ips << "  offset " << it2->first << std::endl; //<< "    ";
                    for (typename map3::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3)
                    {
                        ips << "    mc " << it3->first << "x " << middle_size[it3->first] << std::endl << "      ";
                        for (typename map4::const_iterator it4 = it3->second.begin(); it4 != it3->second.end(); ++it4)
                        {
                            if (it4->second > 1)
                            ips << boost::get<0>(it4->first)
                                << "," << boost::get<1>(it4->first)
                                << "," << boost::get<2>(it4->first)
                                << ": " << it4->second
                                << "  ";
                        }
                        ips << std::endl;
                    }
                    ips << std::endl;
                } 
                ips << std::endl;
            }
            ips.close();
        }

        } //scope
        */

        // output_per_T , for each block in T, list all locations in the output MPS that need this block

        //typedef std::map<unsigned, unsigned> map3;
        //typedef std::map<charge, map3 > map2;
        //typedef std::map<triple, map2 > map1;
        //
        //map1 stasks;
        //// MPS block
        //for (int lb = 0; lb < left_i.size(); ++lb)
        //{
        //    charge out_charge = left_i[lb].first;
        //     
        //    // loop over boundary
        //    for (int b1 = 0; b1 < loop_max; ++b1)
        //    {
        //        // find connecting middle charge
        //        int cnt = 0;
        //        const_iterator lit = left[b1].basis().left_lower_bound(out_charge);
        //        for ( ; lit != left[b1].basis().end() && lit->lc == out_charge; ++lit)
        //        {
        //            charge middle_charge = lit->rc;
        //        
        //            // find out_charge in contraction_schedule[b1]
        //            std::vector<detail::micro_task<value_type> > const & tvec
        //                = contraction_schedule[b1][std::make_pair(middle_charge, out_charge)];
        //            for (int i = 0; i < tvec.size(); ++i)
        //                //stasks[out_charge][tvec[i].out_offset][boost::make_tuple(tvec[i].b2, tvec[i].k, tvec[i].in_offset)]++;
        //                stasks[boost::make_tuple(tvec[i].b2, tvec[i].k, tvec[i].in_offset)][out_charge][tvec[i].out_offset]++;

        //            cnt++; 
        //        }
        //        if (cnt > 3) { maquis::cout << left[b1].basis() << std::endl; exit(1); }
        //    }
        //}

        //if (mpo.row_dim() == 178)
        //for (typename map1::const_iterator it1 = stasks.begin(); it1 != stasks.end(); ++it1)
        //{
        //    maquis::cout << boost::get<0>(it1->first)
        //                 << "," << boost::get<1>(it1->first)
        //                 << "," << boost::get<2>(it1->first)
        //                 << "| ";

        //    for (typename map2::const_iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
        //    {
        //        maquis::cout << it2->first << " "; 
        //        for (typename map3::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3)
        //            maquis::cout << it3->first << ":" << it3->second << " ";
        //    }
        //    maquis::cout << std::endl;    
        //}            

        return contraction_schedule;
    }


} // namespace common
} // namespace contraction

    template <typename T>
    std::ostream & operator << (std::ostream & os, contraction::common::detail::micro_task<T> t)
    {
        os << "b2 " << t.b2 << " oo " << t.out_offset << " scale " << t.scale;
        return os;
    }

#endif
