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
#include <malloc.h>

#include <thread>
#include <mutex>

#include "dmrg/utils/accelerator.h"

#include "utils/sizeof.h"
#include "dmrg/utils/aligned_allocator.hpp"

#include "dmrg/mp_tensors/contractions/numeric/numeric.h"
#include "dmrg/mp_tensors/contractions/numeric/gemm_template.h"

#include "dmrg/mp_tensors/contractions/numeric/gpu.h"
#include "dmrg/utils/cuda_helpers.hpp"

#include "dmrg/mp_tensors/contractions/task/mps_stage.hpp"

namespace contraction {
namespace common {

using boost::get; 

template <class T> class WorkSet;

template <class Matrix, class SymmGroup>
class Cohort
{
    typedef MPOTensor_detail::index_type index_type;
    typedef typename Matrix::value_type value_type;

private:

    class SUnit
    {
    public:

        void push_back(value_type scale, index_type ti, index_type col)
        {
            alpha.push_back(scale);
            b2count++;

            tidx.push_back(ti);
            tidx.push_back(col);
        }

        unsigned add_line(unsigned b)
        {
            unsigned ret = b2count;

            if(b2count) {
                b2s.push_back(b2count);
                b1.push_back(b);
                b2count=0;
            }

            return ret;
        }

        std::size_t n_tasks() const { return alpha.size(); }

        unsigned offset;
        unsigned ms=0;
        std::vector<index_type> tidx;
        std::vector<value_type> alpha;
        std::vector<index_type> b2s;
        std::vector<index_type> b1;

        index_type* dev_tidx;
        value_type* dev_alpha;
        index_type* dev_b2s;
        index_type* dev_b1;

        void stage(accelerator::device* dev)
        {
            dev_tidx = (index_type*)dev->stage_vector(tidx);
            dev_alpha = (value_type*)dev->stage_vector(alpha);
            dev_b2s = (index_type*)dev->stage_vector(b2s);
            dev_b1  = (index_type*)dev->stage_vector(b1);
        }

    private:
        unsigned b2count=0;
    };

    class SUnitVectorStage
    {
    public:

        index_type*  dev_offset;
        index_type*  dev_ms;
        index_type*  dev_nb1;

        index_type** dev_vtidx;
        index_type** dev_vb2s;
        index_type** dev_vb1;
        value_type** dev_valpha;

        void stage(accelerator::device* dev, std::vector<SUnit> const & suv)
        {
            offset.resize(suv.size());
            ms.resize(suv.size());
            nb1.resize(suv.size());
            vtidx.resize(suv.size());
            vb2s.resize(suv.size());
            vb1.resize(suv.size());
            valpha.resize(suv.size());

            for (unsigned x = 0; x < suv.size(); ++x)
            {
                offset[x] = suv[x].offset;
                ms[x] = suv[x].ms;
                nb1[x] = suv[x].b2s.size();

                vtidx[x] = suv[x].dev_tidx;
                vb2s[x] = suv[x].dev_b2s;
                vb1[x]  = suv[x].dev_b1;
                valpha[x] = suv[x].dev_alpha;
            }

            dev_offset = (index_type*)dev->stage_vector(offset);
            dev_ms = (index_type*)dev->stage_vector(ms);
            dev_nb1 = (index_type*)dev->stage_vector(nb1);

            dev_vtidx = (index_type**)dev->stage_vector(vtidx);
            dev_vb2s  = (index_type**)dev->stage_vector(vb2s);
            dev_vb1   = (index_type**)dev->stage_vector(vb1);
            dev_valpha = (value_type**)dev->stage_vector(valpha);
        }

    private:
        std::vector<unsigned> offset;
        std::vector<unsigned> ms;
        std::vector<unsigned> nb1;
        std::vector<index_type*> vtidx;
        std::vector<index_type*> vb2s;
        std::vector<index_type*> vb1;
        std::vector<value_type*> valpha;
    };

public:

    Cohort() {}
    Cohort(index_type mpodim) : mpo_offsets(mpodim) {}
    Cohort(
           Index<SymmGroup> const & phys_i,
           index_type l_block,
           index_type r_block,
           index_type l_size,
           index_type r_size,
           index_type ci_,
           index_type ci_eff_,
           index_type mpodim,
           bool left = false
          )
          : lb(l_block), rb(r_block), ls(l_size), rs(r_size), ci(ci_), ci_eff(ci_eff_), mpo_offsets(mpodim), nSrows(mpodim), sfold(phys_i.size())
          , suv(phys_i.sum_of_sizes())
    {
        unsigned ssum = 0;
        for (unsigned s = 0; s < phys_i.size(); ++s)
        {
            sfold[s] = ssum;
            ssum += phys_i[s].second;
        }

        // right version of create_s has a the b index on the left side
        if (left) sblock = rs;
        else      sblock = ls;
    }

    void push_back(unsigned s, unsigned ss2, value_type scale, unsigned ti, unsigned col)
    {
        unsigned sid = sfold[s] + ss2;
        suv[sid].push_back(scale, ti, col);
    }

    void add_line(index_type b1)
    {
        for (unsigned sid = 0; sid < suv.size(); ++sid)
            mpo_offsets[b1] += suv[sid].add_line(b1); // mpo_offsets[b1] == number of entries for this row
    }

    void add_unit(unsigned s, unsigned ss, unsigned m_size, unsigned offset)
    {
        for (unsigned i=0; i < ss; ++i)
        {
            suv[sfold[s] + i].offset = i * m_size + offset;
            suv[sfold[s] + i].ms = m_size;
            stripe += m_size;
        }
    }

    void finalize()
    {
        compute_mpo_offsets();
    }

    template <class DefaultMatrix, class OtherMatrix>
    void prop_l(MPSTensor<DefaultMatrix, SymmGroup> const & bra_mps,
                std::vector<std::vector<value_type>> const & T,
                unsigned ci,
                Boundary<OtherMatrix, SymmGroup> & new_left) const
    {
        std::vector<value_type> sloc = create_s(T);

        int M = num_cols(bra_mps.data()[lb]);
        int N = new_left.index().n_blocks(ci) * new_left.index().right_size(ci);
        blas_gemm('T', 'N', M, N, stripe, value_type(1),
                  &bra_mps.data()[lb](0,0), stripe, &sloc[0], stripe, value_type(0), new_left[ci], M);
    }

    template <class DefaultMatrix, class OtherMatrix>
    void prop_r(MPSTensor<DefaultMatrix, SymmGroup> const & bra_mps,
                std::vector<std::vector<value_type>> const & T,
                unsigned ci,
                Boundary<OtherMatrix, SymmGroup> & new_right) const
    {
        std::vector<value_type> sloc = create_s_r(T);

        int M = new_right.index().n_blocks(ci) * ls;
        int N = rs;
        DefaultMatrix buf(M,N);
        blas_gemm('N', 'T', M, N, stripe, value_type(1),
                   &sloc[0], M, &bra_mps.data()[rb](0,0), rs, value_type(0), &buf(0,0), M);

        for (unsigned b = 0; b < new_right.index().n_blocks(ci); ++b)
            for (unsigned col = 0; col < rs; ++col)
                std::copy(&buf(ls*b,col), &buf(ls*b,col) + ls, new_right[ci] + (b*rs + col)*ls);
    }

    template <class DefaultMatrix, class OtherMatrix>
    void prop_r_gpu(MPSTensor<DefaultMatrix, SymmGroup> const & bra_mps,
                    value_type** dev_T,
                    unsigned ci,
                    Boundary<OtherMatrix, SymmGroup> & new_right) const
    {
        create_s_r_gpu(dev_T);

        int M = new_right.index().n_blocks(ci) * ls;
        int N = rs;
        int K = stripe;

        value_type* dev_r = dev_S + bit_twiddling::round_up<BUFFER_ALIGNMENT>(M * size_t(K));

        value_type one(1.0), zero(0.);
        cublasSetStream(accelerator::gpu::get_handle(), ws->stream);
        cublasOperation_t cuop[2] = {CUBLAS_OP_N, CUBLAS_OP_T};
        cublasDgemm(accelerator::gpu::get_handle(),
                    cuop[0], cuop[1], M, N, K, &one, dev_S, M, (value_type*)bra_mps.device_data()[rb], N, &zero, dev_r, M);

        copy_v(ws->stream, ls, rs, new_right.index().n_blocks(ci), dev_r, (value_type*)new_right.device_data()[ci]);

        cudaMemcpyAsync( new_right[ci], (value_type*)new_right.device_data()[ci], M*N * sizeof(value_type), cudaMemcpyDeviceToHost,
                         ws->stream);
    }

    template <class DefaultMatrix, class OtherMatrix>
    void contract(
                  Boundary<OtherMatrix, SymmGroup> const & left,
                  std::vector<std::vector<value_type>> const & T,
                  DefaultMatrix & output,
                  std::mutex & out_mutex) const
    {
        std::vector<value_type> sloc = create_s_r(T);

        int M = rs;
        int N = stripe;
        int K = nSrows * ls;

        const value_type* luse = left[ci_eff];
        std::vector<value_type> lbuf;
        if (ci != ci_eff)
        {
            lbuf = std::vector<value_type>(M * size_t(K));
            for (size_t offset = 0; offset < M * size_t(K); offset += rs * ls)
            {
                for (unsigned c = 0; c < rs; ++c)
                for (unsigned r = 0; r < ls; ++r)
                    lbuf[offset + r*rs + c] = *(left[ci_eff] + offset + c*ls + r);
            }
            luse = lbuf.data();
        }

        DefaultMatrix buf(M,N);
        blas_gemm('N', 'N', M, N, K, value_type(1), luse, M, sloc.data(), K, value_type(0), buf.get_values().data(), M);

        //std::lock_guard<std::mutex> lk(out_mutex);
        parallel_critical
        output += buf;
    }

    template <class OtherMatrix>
    void contract_gpu(Boundary<OtherMatrix, SymmGroup> const & left,
                      value_type** dev_T,
                      value_type* dev_out) const
    {
        create_s_r_gpu(dev_T);

        int M = rs;
        int N = stripe;
        int K = nSrows * ls;

        value_type* dev_l = (ci != ci_eff) ? dev_S + bit_twiddling::round_up<BUFFER_ALIGNMENT>(K * size_t(N)) : (value_type*)left.device_data()[ci_eff];
        if (ci != ci_eff)
            transpose_v(ws->stream, ls, rs, left.index().n_blocks(ci_eff), (value_type*)left.device_data()[ci_eff], dev_l);

        value_type one(1.0), zero(0.);

        cublasSetStream(accelerator::gpu::get_handle(), ws->stream);
        cublasOperation_t cuop[2] = {CUBLAS_OP_N, CUBLAS_OP_T};
        cublasStatus_t stat = cublasDgemm(accelerator::gpu::get_handle(),
                                          cuop[0], cuop[0], M, N, K, &one, dev_l, M, dev_S, K, &zero, ws->mps_buffer, M);
        if (stat != CUBLAS_STATUS_SUCCESS)
        {
            std::cout << "lgemm failed: " << _cudaGetErrorEnum(stat) << std::endl;
            exit(EXIT_FAILURE);
        }

        atomic_add(ws->stream, M*std::size_t(N), ws->mps_buffer, dev_out);
    }

    template <class OtherMatrix>
    void lbtm(
              std::vector<std::vector<value_type>> const & T,
              OtherMatrix & out,
              double alpha
             ) const
    {
        std::vector<value_type> sloc = create_s(T);

        int M = stripe;
        int K = sloc.size() / M;

        OtherMatrix tmp(M,M);
        blas_gemm('N', 'T', M, M, K, value_type(alpha), &sloc[0], stripe, &sloc[0], stripe, value_type(1), &tmp(0,0), M);

        parallel_critical
        out += tmp;
    }

    template <class OtherMatrix>
    void rbtm(
              std::vector<std::vector<value_type>> const & T,
              OtherMatrix & out,
              double alpha
             ) const
    {
        std::vector<value_type> sloc = create_s_r(T);

        int M = stripe;
        int K = nSrows * ls;

        OtherMatrix tmp(M,M);
        blas_gemm('T', 'N', M, M, K, value_type(alpha), &sloc[0], K, &sloc[0], K, value_type(1), &tmp(0,0), M);

        parallel_critical
        out += tmp;
    }

    std::size_t n_tasks() const
    {
        return std::accumulate(suv.begin(), suv.end(), 0, [](std::size_t sum, SUnit const& su) { return sum + su.n_tasks();});
    }

    template <class DefaultMatrix, class OtherMatrix>
    std::size_t n_flops(MPSTensor<DefaultMatrix, SymmGroup> const& mps, BoundaryIndex<OtherMatrix, SymmGroup> const& left) const
    {
        std::size_t ret = 0;
        ret += 2 * rs * ls * left.n_blocks(ci_eff) * num_cols(mps.data()[rb]);

        for (auto const& x : suv)
            ret += 2 * ls * x.ms * x.alpha.size();

        return ret;
    }

    void stage(accelerator::device* dev, WorkSet<value_type>* ws_, value_type* s)
    {
        ws = ws_;
        dev_S = s;
        for (auto& su : suv) su.stage(dev);

        suv_stage.stage(dev, suv);
    }

    std::vector<long int>      & get_offsets()       { return mpo_offsets; }
    std::vector<long int> const& get_offsets() const { return mpo_offsets; }

    index_type get_lb() const { return lb; }
    index_type get_rb() const { return rb; }

    std::size_t get_S_size() const { return nSrows * stripe * std::size_t(sblock); }
    std::size_t get_l_size() const { return nSrows * rs * std::size_t(ls); }

private:
    index_type lb, rb, ls, rs, ci, ci_eff;
    // S is stripe x (sblock * nSrows)
    index_type sblock, nSrows = 0, stripe = 0;

    std::vector<long int> mpo_offsets;

    std::vector<unsigned> sfold;

    // gpu staging data
    std::vector<SUnit> suv;
    SUnitVectorStage suv_stage;
    WorkSet<value_type>* ws;
    value_type* dev_S;

    std::vector<value_type> create_s(std::vector<std::vector<value_type>> const& T) const
    {
        std::vector<value_type> ret(get_S_size());
        for (auto const& x : suv)
        {
            if (!x.alpha.size()) continue;

            alps::numeric::matrix<value_type> buf(x.ms, rs);

            index_type seeker = 0;
            for (index_type b=0; b < x.b2s.size(); ++b)
            {
                memset(&buf(0,0), 0, x.ms * rs * sizeof(value_type));

                for (int ia = seeker; ia < seeker + x.b2s[b]; ++ia)
                    maquis::dmrg::detail::iterator_axpy(&T[x.tidx[2*ia]][x.tidx[2*ia+1] * rs],
                                                        &T[x.tidx[2*ia]][x.tidx[2*ia+1] * rs] + x.ms * rs,
                                                        &buf(0,0), x.alpha[ia]);

                unsigned bb = x.b1[b];
                for (unsigned c = 0; c < rs; ++c)
                    std::copy(&buf(0,c), &buf(0,c) + x.ms, ret.data() + stripe * (bb*rs + c) + x.offset);

                seeker += x.b2s[b];
            }
        }
        return ret;
    }

    std::vector<value_type> create_s_r(std::vector<std::vector<value_type>> const & T) const
    {
        std::vector<value_type> ret(get_S_size());
        for (auto const& x : suv)
        {
            if (!x.alpha.size()) continue;

            alps::numeric::matrix<value_type> buf(ls, x.ms);

            index_type seeker = 0;
            for (index_type b=0; b < x.b2s.size(); ++b)
            {
                memset(&buf(0,0), 0, ls * x.ms * sizeof(value_type));

                for (int ia = seeker; ia < seeker + x.b2s[b]; ++ia)
                    maquis::dmrg::detail::iterator_axpy(&T[x.tidx[2*ia]][x.tidx[2*ia+1] * ls],
                                                        &T[x.tidx[2*ia]][x.tidx[2*ia+1] * ls] + x.ms * ls,
                                                        &buf(0,0), x.alpha[ia]);

                unsigned bb = x.b1[b];
                for (unsigned c = 0; c < x.ms; ++c)
                    std::copy(buf.col(c).first, buf.col(c).second, ret.data() + nSrows*ls * (x.offset+c) + bb*ls);

                seeker += x.b2s[b];
            }
        }
        return ret;
    }

    void create_s_r_gpu(value_type** dev_T) const
    {
        HANDLE_ERROR(cudaMemsetAsync(dev_S, 0, get_S_size() * sizeof(value_type), ws->stream));

        dsaccv_gpu(ws->stream, suv.size(), nSrows, ls, suv_stage.dev_ms, suv_stage.dev_nb1,
                   suv_stage.dev_vb1, suv_stage.dev_vb2s, suv_stage.dev_valpha, suv_stage.dev_vtidx, dev_T, dev_S, suv_stage.dev_offset);
    }

    void compute_mpo_offsets()
    {
        std::vector<long int> cpy = mpo_offsets;
        index_type n_blocks = 0;
        for(auto& b : cpy) if (b) b = n_blocks++;

        nSrows = n_blocks;

        for (auto & x : suv)
            for (index_type b1i = 0; b1i < x.b1.size(); ++b1i)
            {
                index_type b = x.b1[b1i]; // b is the mpo index
                x.b1[b1i] = cpy[b];
            }

        std::size_t block_size = ls * rs; // ALIGN
        index_type cnt = 0;
        for(auto& b : mpo_offsets) if (b) b = block_size * cnt++; else b = -1;
    }
};


template <class Matrix, class SymmGroup>
class MPSBlock : public std::vector<Cohort<Matrix, SymmGroup>>
{
    typedef typename Matrix::value_type value_type;

    template <class T>
    static void tr_tile_v(unsigned nrows, unsigned ncols, size_t cnt, const T* in, T* out)
    {
        std::vector<T> buf(nrows * ncols);
        for (size_t b = 0; b < cnt; ++b)
        {
            size_t offset = b * nrows * ncols;
            std::copy(in + offset, in + offset + nrows*ncols, buf.data());

            for (unsigned i = 0; i < nrows; ++i)
            for (unsigned j = 0; j < ncols; ++j)
                out[offset + ncols*i + j] = buf[nrows*j + i]; 
        }
    }

public:
    typedef Cohort<Matrix, SymmGroup> cohort_type;

    template <class DefaultMatrix, class OtherMatrix>
    std::vector<std::vector<value_type>>
    create_T_left(Boundary<OtherMatrix, SymmGroup> const & left, MPSTensor<DefaultMatrix, SymmGroup> const & mps) const
    {
        std::vector<std::vector<value_type>> ret(t_schedule.size());
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            unsigned mps_offset = boost::get<0>(t_schedule[ti]);
            unsigned ci = boost::get<1>(t_schedule[ti]);
            unsigned ci_eff = boost::get<2>(t_schedule[ti]);
            unsigned lb_ket = boost::get<3>(t_schedule[ti]);

            unsigned bls = left.index().left_size(ci);
            unsigned brs = left.index().right_size(ci);
            unsigned nb  = left.index().n_blocks(ci_eff);

            std::vector<value_type> lbuf;
            if (!left.index().tr(ci))
            {
                lbuf = std::vector<value_type>(bls * brs * nb);
                tr_tile_v(bls, brs, nb, left[ci_eff], lbuf.data());
            }

            const value_type* l_use = (left.index().tr(ci)) ? left[ci_eff] : lbuf.data();
            const value_type* mpsdata = &mps.data()[lb_ket](0, mps_offset);
            ret[ti] = std::vector<value_type>(bls * rs_ket * nb);

            int M = rs_ket;
            int N = bls * nb;
            int K = brs;
            blas_gemm('T', 'N', M, N, K, value_type(1), mpsdata, K, l_use, K, value_type(0), ret[ti].data(), M);

            tr_tile_v(rs_ket, bls, nb, ret[ti].data(), ret[ti].data());

            //int M = bls;
            //int N = rs_ket;
            //int K = brs;

            //const value_type* mpsdata = &mps.data()[lb_ket](0, mps_offset);
            //ret[ti] = std::vector<value_type>(M * size_t(N) * left.index().n_blocks(ci_eff));
            //for (unsigned b = 0; b < left.index().n_blocks(ci_eff); ++b)
            //{
            //    size_t loff = b*M*size_t(K);
            //    size_t ooff = b*M*size_t(N);

            //    if (left.index().tr(ci))
            //        blas_gemm('T', 'N', M, N, K, value_type(1), left[ci_eff] + loff, K,
            //                  mpsdata, K, value_type(0), ret[ti].data()+ooff, M);
            //    else
            //        blas_gemm('N', 'N', M, N, K, value_type(1), left[ci_eff] + loff, M,
            //                  mpsdata, K, value_type(0), ret[ti].data()+ooff, M);
            //}
        }

        return ret;
    }

    //template <class DefaultMatrix, class OtherMatrix, class Pointer>
    //value_type** create_T_left_gpu(Boundary<OtherMatrix, SymmGroup> const & left,
    //                               MPSTensor<DefaultMatrix, SymmGroup> const & mps,
    //                               std::vector<Pointer> const & mps_dev_ptr) const
    //{
    template <class DefaultMatrix, class OtherMatrix>
    std::vector<std::vector<value_type>>
    create_T_left_gpu(Boundary<OtherMatrix, SymmGroup> const & left, MPSTensor<DefaultMatrix, SymmGroup> const & mps) const
    {
        std::vector<std::vector<value_type>> ret(t_schedule.size());

        cublasSetStream(accelerator::gpu::get_handle(), ws->stream);

        value_type* dev_l = gpu_data.dev_rsl;
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            unsigned mps_offset = boost::get<0>(t_schedule[ti]);
            unsigned ci = boost::get<1>(t_schedule[ti]);
            unsigned ci_eff = boost::get<2>(t_schedule[ti]);
            unsigned lb_ket = boost::get<3>(t_schedule[ti]);

            unsigned bls = left.index().left_size(ci);
            unsigned brs = left.index().right_size(ci);

            int nb  = left.index().n_blocks(ci_eff);
            int M = bls;
            int N = rs_ket;
            int K = brs;

            if(gpu_data.t[ti] + M * size_t(N)  > dev_l) {std::cout << "T/L overlap\n"; exit(1); }

            const value_type* mpsdata = (value_type*)mps.device_data()[lb_ket] + mps_offset * K;
            ret[ti] = std::vector<value_type>(M * size_t(N) * nb);

            value_type one(1.0), zero(0.);
            cublasOperation_t cuop[2] = {CUBLAS_OP_N, CUBLAS_OP_T};
            cublasStatus_t stat;
            if (left.index().tr(ci))
                stat =
                cublasDgemmStridedBatched(accelerator::gpu::get_handle(),
                            cuop[1], cuop[0], M, N, K, &one, (value_type*)left.device_data()[ci_eff], K, M*K,
                            mpsdata, K, 0, &zero, gpu_data.t[ti], M, M*N, nb);
            else
                stat =
                cublasDgemmStridedBatched(accelerator::gpu::get_handle(),
                            cuop[0], cuop[0], M, N, K, &one, (value_type*)left.device_data()[ci_eff], M, M*K,
                            mpsdata, K, 0, &zero, gpu_data.t[ti], M, M*N, nb);

            if (stat != CUBLAS_STATUS_SUCCESS) {
                std::cout << "lgemm failed: " << _cudaGetErrorEnum(stat) << std::endl;
                exit(EXIT_FAILURE);
            }

            HANDLE_ERROR(cudaMemcpy(ret[ti].data(), gpu_data.t[ti], M*N*nb * sizeof(value_type),
                         cudaMemcpyDeviceToHost));
        }

        //return gpu_data.dev_t;
        return ret;
    }

    template <class DefaultMatrix, class OtherMatrix>
    std::vector<std::vector<value_type>>
    create_T(Boundary<OtherMatrix, SymmGroup> const & right, MPSTensor<DefaultMatrix, SymmGroup> const & mps) const
    {
        std::vector<std::vector<value_type>> ret(t_schedule.size());
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            unsigned mps_offset = boost::get<0>(t_schedule[ti]);
            unsigned ci = boost::get<1>(t_schedule[ti]);
            unsigned ci_eff = boost::get<2>(t_schedule[ti]);
            unsigned lb_ket = boost::get<3>(t_schedule[ti]);

            unsigned bls = right.index().left_size(ci);
            unsigned brs = right.index().right_size(ci);

            int M = num_rows(mps.data()[lb_ket]);
            int N = right.index().n_blocks(ci_eff) * brs;
            int K = bls;

            //std::vector<value_type> rbuf;
            //if (right.index().tr(ci))
            //{
            //    rbuf = std::vector<value_type>(K * size_t(N));
            //    for (size_t offset = 0; offset < K * size_t(N); offset += brs * bls)
            //    {
            //        for (unsigned c = 0; c < brs; ++c)
            //        for (unsigned r = 0; r < bls; ++r)
            //            rbuf[offset + c*bls + r] = right[ci_eff] + offset + r*brs + c;
            //    }
            //}

            //const value_type* r_use = (right.index().tr(ci)) ? rbuf.data() : right[ci_eff];
            //const value_type* mpsdata = &mps.data()[lb_ket](0, mps_offset);
            //ret[ti] = std::vector<value_type>(M * size_t(N));

            //blas_gemm('N', 'N', M, N, K, value_type(1), mpsdata, M, r_use, K, value_type(0), ret[ti].data(), M);

            const value_type* mpsdata = &mps.data()[lb_ket](0, mps_offset);
            ret[ti] = std::vector<value_type>(M * size_t(N));
            for (unsigned b = 0; b < right.index().n_blocks(ci_eff); ++b)
            {
                int N = brs;
                size_t roff = b*K*N;
                size_t ooff = b*M*N;
                if (right.index().tr(ci))
                    blas_gemm('N', 'T', M, N, K, value_type(1), mpsdata, M,
                              right[ci_eff] + roff, N, value_type(0), ret[ti].data()+ooff, M);
                else
                    blas_gemm('N', 'N', M, N, K, value_type(1), mpsdata, M,
                              right[ci_eff] + roff, K, value_type(0), ret[ti].data()+ooff, M);
            }
        }

        return ret;
    }

    template <class DefaultMatrix, class OtherMatrix, class Pointer>
    value_type** create_T_gpu(Boundary<OtherMatrix, SymmGroup> const & right, MPSTensor<DefaultMatrix, SymmGroup> const & mps,
                              std::vector<Pointer> const & mps_dev_ptr) const
    {
        cublasSetStream(accelerator::gpu::get_handle(), ws->stream);

        value_type* dev_r = gpu_data.dev_rsl;
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            unsigned mps_offset = boost::get<0>(t_schedule[ti]);
            unsigned ci = boost::get<1>(t_schedule[ti]);
            unsigned ci_eff = boost::get<2>(t_schedule[ti]);
            unsigned lb_ket = boost::get<3>(t_schedule[ti]);

            unsigned bls = right.index().left_size(ci);
            unsigned brs = right.index().right_size(ci);

            int M = num_rows(mps.data()[lb_ket]);
            int N = right.index().n_blocks(ci_eff) * brs;
            int K = bls;

            if (right.index().tr(ci))
                transpose_v(ws->stream, brs, bls, right.index().n_blocks(ci_eff), (value_type*)right.device_data()[ci_eff], dev_r);

            const value_type* r_use = (right.index().tr(ci)) ? dev_r : (value_type*)right.device_data()[ci_eff];
            //const value_type* mpsdata = (value_type*)mps.device_data()[lb_ket] + mps_offset * M;
            const value_type* mpsdata = (value_type*)mps_dev_ptr[lb_ket] + mps_offset * M;

            assert( gpu_data.t[ti] + M * size_t(N)  <= dev_r);

            value_type one(1.0), zero(0.);
            cublasOperation_t cuop[2] = {CUBLAS_OP_N, CUBLAS_OP_T};
            cublasDgemm(accelerator::gpu::get_handle(),
                        cuop[0], cuop[0], M, N, K, &one, mpsdata, M, r_use, K, &zero, gpu_data.t[ti], M);
        }

        return gpu_data.dev_t;
    }

    std::size_t max_sl_size() const
    {
        std::size_t ret = 0;
        for (auto& cohort : *this)
        {
            ret = std::max(ret, bit_twiddling::round_up<BUFFER_ALIGNMENT>(cohort.get_S_size()) +
                                bit_twiddling::round_up<BUFFER_ALIGNMENT>(cohort.get_l_size()));
        }
        return ret;
    }

    template <class OtherMatrix>
    std::size_t max_r_size(BoundaryIndex<OtherMatrix, SymmGroup> const& right) const
    {
        std::size_t ret = 0;
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            unsigned ci = boost::get<1>(t_schedule[ti]);
            if (right.tr(ci))
            {
                unsigned ci_eff = boost::get<2>(t_schedule[ti]);
                ret = std::max(ret, right.n_blocks(ci_eff) * right.left_size(ci) * right.right_size(ci));
            }
        }
        return bit_twiddling::round_up<BUFFER_ALIGNMENT>(ret);
    }

    unsigned get_ti(unsigned mps_offset, unsigned ci_virt) const
    {
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
            if (boost::get<0>(t_schedule[ti]) == mps_offset && boost::get<1>(t_schedule[ti]) == ci_virt)
                return ti;

        //throw std::runtime_error("ti not found\n");
        return std::numeric_limits<unsigned>::max();
    }

    template <class DefaultMatrix, class OtherMatrix>
    size_t n_flops(MPSTensor<DefaultMatrix, SymmGroup> const& mps,
                   BoundaryIndex<OtherMatrix, SymmGroup> const& left,
                   BoundaryIndex<OtherMatrix, SymmGroup> const& right) const
    {
        std::size_t ret = 0;
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            unsigned ci = boost::get<1>(t_schedule[ti]);
            unsigned ci_eff = boost::get<2>(t_schedule[ti]);
            unsigned lb_ket = boost::get<3>(t_schedule[ti]);

            unsigned bls = right.left_size(ci);
            unsigned brs = right.right_size(ci);

            ret += 2 * mps.row_dim()[lb_ket].second * right.left_size(ci) * right.right_size(ci) * right.n_blocks(ci_eff);
        }

        for (auto& coh : *this) ret += coh.n_flops(mps, left);

        return ret;
    }

    void stage(accelerator::device* dev, WorkSet<value_type>* ws_)
    {
        ws = ws_;

        gpu_data.t.resize(t_schedule.size());

        value_type* dev_t_seek = ws->buffer;
        for (unsigned ti = 0; ti < t_schedule.size(); ++ti)
        {
            gpu_data.t[ti] = dev_t_seek;
            dev_t_seek += boost::get<4>(t_schedule[ti]);
        }

        gpu_data.dev_rsl = dev_t_seek;
        gpu_data.stage(dev);

        for (auto& coh : *this) coh.stage(dev, ws, gpu_data.dev_rsl);
    }

    unsigned rs_ket;

    struct TSched_type : public
    std::vector<boost::tuple<unsigned, unsigned, unsigned, unsigned, size_t>>
    {
        TSched_type() : buf_size(0) {}
        size_t buf_size;
    } t_schedule;

    bool on_gpu = false;
    int deviceID;

private:
    WorkSet<value_type>* ws;

    struct gpuTransferable // staging data
    {
        std::vector<value_type*> t;
        value_type** dev_t;

        value_type* dev_rsl;

        void stage(accelerator::device* dev) {
            dev_t = (value_type**)dev->stage_vector(t);
        }
    };

    gpuTransferable gpu_data;
};

///////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
class WorkSet
{
public:

    WorkSet(T* t_, T* mps_, int id_) : buffer(t_), mps_buffer(mps_), id(id_), stream(accelerator::gpu::next_stream(id_)) {}

    T* buffer;
    T* mps_buffer;
    int id;
    cudaStream_t stream;
};

///////////////////////////////////////////////////////////////////////////////////////////////

template <class Matrix, class SymmGroup>
struct ScheduleNew : public std::vector<MPSBlock<
            typename maquis::traits::aligned_matrix<Matrix, maquis::aligned_allocator, ALIGNMENT>::type
                                                      , SymmGroup> >
{
    typedef typename maquis::traits::aligned_matrix<Matrix, maquis::aligned_allocator, ALIGNMENT>::type AlignedMatrix;
    typedef std::vector<MPSBlock<AlignedMatrix, SymmGroup> > base;
    typedef MPSBlock<AlignedMatrix, SymmGroup> block_type;
    typedef typename Matrix::value_type value_type;

    ScheduleNew() {}
    ScheduleNew(std::size_t dim) : base(dim), /*mutexes(dim),*/ cpu_time(0) { std::fill(gpu_time, gpu_time + MAX_N_GPUS, 0); }

    //double mflops(double time) const { return total_flops*niter / time / 1e6; }

    void print_stats(double time) const {
        maquis::cout << total_flops*niter / time / 1e6
                     << " CPU: " << cpu_flops*niter / cpu_time / 1e6;
        double gpu_t;
        if (gpu_flops)
        {
            gpu_t = *std::max_element(gpu_time, gpu_time + accelerator::gpu::nGPU());
            maquis::cout << " GPU: " << gpu_flops*niter / gpu_t / 1e6;
        }

        maquis::cout << "  (MFLOPS)" << std::endl;

        if (gpu_flops)
            maquis::cout << "GPU_TIME: "  << gpu_t << std::endl;
    }

    double get_cpu_gpu_ratio()
    {
        if (!gpu_flops) return 0.9;

        double gpu_t = *std::max_element(gpu_time, gpu_time + accelerator::gpu::nGPU());
        double gpu_speed = gpu_flops / gpu_t;

        double cpu_speed = cpu_flops / cpu_time;

        return std::max(1.0 / (cpu_speed/gpu_speed + 1.0), 0.9) ;
    }

    template <class OtherMatrix>
    void compute_workload(MPSTensor<Matrix, SymmGroup> const & mps, BoundaryIndex<OtherMatrix, SymmGroup> const& left,
                          BoundaryIndex<OtherMatrix, SymmGroup> const& right, double cpu_gpu_ratio)
    {
        std::vector<std::size_t> flops_list;
        for (auto& mpsb : *this)
            flops_list.push_back( mpsb.n_flops(mps, left, right) );

        total_flops = std::accumulate(flops_list.begin(), flops_list.end(), 0lu);

        std::vector<size_t> mpsb_sorted = sort_invert(flops_list);

        std::size_t nflops = 0, cut = 0;
        for ( ; cut < mpsb_sorted.size(); ++cut) {
            nflops += flops_list[mpsb_sorted[cut]];
            if ( double(nflops)/total_flops > cpu_gpu_ratio) break; // send at most cpu_gpu_ratio of the workload to the GPU
        }

        for (std::size_t b = 0; b < mpsb_sorted.size(); ++b) {
            std::size_t idx = mpsb_sorted[b];
            if (accelerator::gpu::use_gpu(flops_list[idx]) && b <= cut) {
                (*this)[idx].on_gpu = true;
                (*this)[idx].deviceID = enumeration_gpu.size() % accelerator::gpu::nGPU();      // TODO load balancing
                //(*this)[idx].deviceID = 1;
                gpu_flops += flops_list[idx];
                enumeration_gpu.push_back(idx);
            }
            else {
                std::size_t idx = mpsb_sorted[b];
                cpu_flops += flops_list[idx];
                enumeration.push_back(idx);
            }
        }
    }

    template <class OtherMatrix>
    void stage_gpu(Boundary<OtherMatrix, SymmGroup> const & right, MPSTensor<Matrix, SymmGroup> const & mps)
    {
        accelerator::gpu::reset_buffers();

        std::size_t mps_maxblock = 0;
        for (std::size_t k = 0; k < mps.data().basis().size(); ++k)
            mps_maxblock = bit_twiddling::round_up<BUFFER_ALIGNMENT>(
                            std::max( mps.data().basis().left_size(k) * mps.data().basis().right_size(k), mps_maxblock) );

        std::vector<std::size_t> buffer_sizes;
        for (auto& mpsb : *this)
            buffer_sizes.push_back(mpsb.t_schedule.buf_size +
                                   std::max(mpsb.max_r_size(right.index()), mpsb.max_sl_size()) +
                                   mps_maxblock);

        // Index of MPSBlock with biggest buffer = mpsb_sorted[0]
        std::vector<std::size_t> mpsb_sorted = sort_invert(buffer_sizes);

        pipeline.resize(accelerator::gpu::nGPU());
        for (int d = 0; d < accelerator::gpu::nGPU(); ++d)
        {
            { // resize the GPU pipeline buffer if needed
                std::vector<std::size_t> psz(accelerator::gpu::max_nstreams());
                for (int tn = 0; tn < std::min(accelerator::gpu::max_nstreams(), (int)buffer_sizes.size()); ++tn)
                    psz[tn] = buffer_sizes[mpsb_sorted[tn]] * sizeof(value_type);

                accelerator::gpu::adjust_pipeline_buffer(psz, d);
            }

            for (int tn = 0; tn < std::min(accelerator::gpu::max_nstreams(), (int)enumeration_gpu.size()); ++tn)
            {
                std::size_t idx = mpsb_sorted[tn];

                value_type* buffer = (value_type*)accelerator::gpu::get_pipeline_buffer(buffer_sizes[idx] * sizeof(value_type), d);
                if (buffer)
                    pipeline[d].push_back(WorkSet<value_type>(buffer, buffer + buffer_sizes[idx] - mps_maxblock, d));
                else
                    break;
            }

            int redo = 0;
            do {
                redo = 0;
                try {
                    int counter = 0;
                    for (std::size_t tn = 0; tn < mpsb_sorted.size(); ++tn)
                    {
                        size_t i = mpsb_sorted[tn];
                        auto& mpsb = (*this)[i];
                        if(mpsb.on_gpu && mpsb.deviceID == d)
                            mpsb.stage(accelerator::gpu::get_device(d), &pipeline[d][counter++%pipeline[d].size()]);
                    }
                }
                catch (const std::out_of_range& e) {
                    redo++;
                    accelerator::gpu::reallocate_staging_buffer(d);
                }
            }
            while (redo > 0);
        }

        accelerator::gpu::update_schedule_buffer();
    }

    void sync() const
    {
        for (WorkSet<value_type> const & ws : pipeline)
            cudaStreamSynchronize(ws.stream);
    }

    std::size_t niter;
    std::size_t total_flops=0;
    std::size_t cpu_flops=0, gpu_flops=0;
    mutable double cpu_time, gpu_time[MAX_N_GPUS];

    std::vector<unsigned> enumeration;
    std::vector<unsigned> enumeration_gpu;

    mutable MPSTensorStage<value_type> mps_stage;
    mutable std::vector<std::mutex> mutexes;

    static Timer sh_timer;

private:
    std::vector<std::vector<WorkSet<value_type>>> pipeline;

};

template <class Matrix, class SymmGroup> Timer ScheduleNew<Matrix, SymmGroup>::sh_timer = Timer("SITE_HAMIL");

///////////////////////////////////////////////////////////////////////////////////////////////


template <class Matrix, class SymmGroup>
struct Schedule
{
    //typedef typename maquis::traits::aligned_matrix<Matrix, maquis::aligned_allocator, ALIGNMENT>::type AlignedMatrix;
    typedef ScheduleNew<Matrix, SymmGroup> schedule_t;
}; 

} // namespace common
} // namespace contraction

#endif
