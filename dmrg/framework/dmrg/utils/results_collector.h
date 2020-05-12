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

#ifndef UTILS_RESULTS_COLLECTOR_H
#define UTILS_RESULTS_COLLECTOR_H

#include <map>
#include <memory>
#include <any>
#include <utility>

#include "dmrg/utils/storage.h"

namespace detail {
    class collector_impl_base
    {
    public:
        virtual ~collector_impl_base() {}
        virtual void collect(const std::any&) = 0;
        virtual void save(alps::hdf5::archive& ar) const = 0;
        virtual void extract(const std::any&) const = 0;
    };

    template<class T>
    class collector_impl : public collector_impl_base
    {
    public:
        void collect(const std::any& val)
        {
            vals.push_back(std::any_cast<T>(val));
        }
        
        void save(alps::hdf5::archive& ar) const
        {
            std::vector<T> allvalues;
            if (ar.is_data("mean/value"))
                ar["mean/value"] >> allvalues;
            std::copy(vals.begin(), vals.end(), std::back_inserter(allvalues));
            ar["mean/value"] << allvalues;
        }

        void extract(const std::any& output) const
        {
            std::vector<T>* ovalues = std::any_cast<std::vector<T>*>(output);
            std::copy(vals.begin(), vals.end(), std::back_inserter(*ovalues));
        }
        
    private:
        std::vector<T> vals;
    };
}

class collector_proxy {
    typedef std::shared_ptr<detail::collector_impl_base> coll_type;
public:
    collector_proxy(coll_type & collector_)
    : collector(collector_)
    { }
    
    template<class T>
    void operator<<(const T& val)
    {
        if (!collector)
            collector.reset(new detail::collector_impl<T>());
        collector->collect(val);
    }

private:
    coll_type & collector;
};

class collector_proxy_const {
    typedef std::shared_ptr<detail::collector_impl_base> coll_type;
public:
    collector_proxy_const(coll_type collector_)
    : collector(collector_)
    { }

    template<class T>
    void operator>>(std::vector<T>& output) const
    {
        if (!collector) return;
        collector->extract(&output);
    }

private:
    coll_type collector;
};

class results_collector
{
public:
    
    void clear()
    {
        collection.clear();
    }
    
    auto operator[] (std::string name)
    {
        return collector_proxy(collection[name]);
    }
    
    auto operator[] (std::string name) const
    {
        return collector_proxy_const(collection.at(name));
    }

    template <class Archive>
    void save(Archive & ar) const
    {
        for (std::map<std::string, std::shared_ptr< ::detail::collector_impl_base> >::const_iterator
             it = collection.begin(); it != collection.end(); ++it)
        {
            ar[it->first] << *it->second;
        }
    }
    
private:
    std::map<std::string, std::shared_ptr<::detail::collector_impl_base>> collection;
};


inline double minIterationEnergy(const results_collector& collector)
{
    std::vector<double> energies;
    collector["Energy"] >> energies;
    return *std::min_element(energies.begin(), energies.end());
}

#endif
