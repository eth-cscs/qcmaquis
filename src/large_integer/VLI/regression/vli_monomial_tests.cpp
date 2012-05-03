/*
*Very Large Integer Library, License - Version 1.0 - May 3rd, 2012
*
*Timothee Ewart - University of Geneva, 
*Andreas Hehn - Swiss Federal Institute of technology Zurich 
*
*Permission is hereby granted, free of charge, to any person or organization
*obtaining a copy of the software and accompanying documentation covered by
*this license (the "Software") to use, reproduce, display, distribute,
*execute, and transmit the Software, and to prepare derivative works of the
*Software, and to permit third-parties to whom the Software is furnished to
*do so, all subject to the following:
*
*The copyright notices in the Software and this entire statement, including
*the above license grant, this restriction and the following disclaimer,
*must be included in all copies of the Software, in whole or in part, and
*all derivative works of the Software, unless such copies or derivative
*works are solely in the form of machine-executable object code generated by
*a source language processor.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
*SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
*FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
*ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*DEALINGS IN THE SOFTWARE.
*/

#define BOOST_TEST_MODULE vli_cpu
#include <boost/test/unit_test.hpp>
#include <gmpxx.h>

#include "vli/vli_cpu.h"
#include "vli/vli_traits.hpp"
#include "vli/polynomial/monomial.hpp"

#include "regression/vli_test.hpp"

using vli::vli_cpu;
using vli::max_int_value;
using vli::monomial;

using vli::test::fill_random;

typedef vli::test::vli_cpu_type_list vli_types;
typedef vli::test::vli_cpu_type_extented_list vli_extented_type;

BOOST_AUTO_TEST_CASE_TEMPLATE ( constructor, Vli, vli_extented_type )
{
    monomial<Vli> ma;
    monomial<Vli> mb(0,0);
    BOOST_CHECK_EQUAL(ma,mb);

    monomial<Vli> mc(1,2);
    monomial<Vli> md(2,1);

    BOOST_CHECK_EQUAL(mc == md, false);
}

BOOST_AUTO_TEST_CASE_TEMPLATE ( copy_constructor, Vli, vli_extented_type )
{
    monomial<Vli> ma;
    monomial<Vli> mb(8,5);
    monomial<Vli> mc(mb);
    BOOST_CHECK_EQUAL(mb,mc);
    BOOST_CHECK_EQUAL(ma == mc,false);

    mc.j_exp_ = 1;
    BOOST_CHECK_EQUAL(mb == mc,false);
}

BOOST_AUTO_TEST_CASE_TEMPLATE ( multiply_by_vli, Vli, vli_extented_type)
{
    Vli a;
    fill_random(a,Vli::size-1);
    Vli a_orig(a);
    monomial<Vli> ma(2,3);
    monomial<Vli> ma_orig(ma);

    monomial<Vli> mb = ma * a;
    monomial<Vli> mc = a * ma;
    BOOST_CHECK_EQUAL(ma, ma_orig);

    ma *= a;
    BOOST_CHECK_EQUAL(ma,mb);
    BOOST_CHECK_EQUAL(ma,mc);

    BOOST_CHECK_EQUAL(ma == ma_orig, false);
    BOOST_CHECK_EQUAL(ma.coeff_,a);

    BOOST_CHECK_EQUAL(a, a_orig);
}

BOOST_AUTO_TEST_CASE_TEMPLATE ( multiply_by_int, Vli, vli_extented_type)
{
    int a = 10;
    int a_orig(a);
    monomial<Vli> ma(5,7);
    monomial<Vli> ma_orig(ma);

    monomial<Vli> mb = ma * a;
    monomial<Vli> mc = a* ma;
    BOOST_CHECK_EQUAL(ma, ma_orig);

    ma *= a;
    BOOST_CHECK_EQUAL(ma,mb);
    BOOST_CHECK_EQUAL(ma,mc);

    BOOST_CHECK_EQUAL(ma == ma_orig, false);
    BOOST_CHECK_EQUAL(ma.coeff_,Vli(a));
    
    BOOST_CHECK_EQUAL(a, a_orig);
}
