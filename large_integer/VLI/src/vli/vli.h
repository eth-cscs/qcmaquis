/*
*Very Large Integer Library, License - Version 1.0 - May 3rd, 2012
*
*Timothee Ewart - University of Geneva,
*Andreas Hehn - Swiss Federal Institute of technology Zurich.
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

#ifndef VLI_VLI_CPU_H
#define VLI_VLI_CPU_H
#include "vli/function_hooks/vli_number_cpu_function_hooks.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/cstdint.hpp> // for the type boost::uint64_t
#include <boost/operators.hpp>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <ostream>
#include <sstream>
#include <boost/swap.hpp>


namespace vli {

    template<std::size_t NumBits> class vli;

    template <std::size_t NumBits>
    void swap(vli<NumBits>& vli_a, vli<NumBits>& vli_b){
        boost::swap(vli_a.data_,vli_b.data_);
    }

    template<std::size_t NumBits>
    class vli
        :boost::equality_comparable<vli<NumBits> >, // generate != operator
         boost::less_than_comparable<vli<NumBits> >, // generate <= >= > < whatever the paire VLI/VLI
         boost::less_than_comparable<vli<NumBits>, long int>, // generate <= >= > < whatever the paire VLI/int
         boost::addable<vli<NumBits> >, // generate VLI<nbits> = VLIVLI<nbits> + VLI<VLI<nbits>
         boost::subtractable<vli<NumBits> >, // generate VLI<nbits> = VLIVLI<nbits> - VLI<VLI<nbits>
         boost::multipliable<vli<NumBits> >, //  generate VLI<nbits> = VLIVLI<nbits> * VLI<VLI<nbits>
         boost::left_shiftable<vli<NumBits>, long int>, // enerate VLI<nbits> = VLIVLI<nbits> << int
         boost::right_shiftable<vli<NumBits>, long int>, //enerate VLI<nbits> = VLIVLI<nbits> >> int
         boost::modable<vli<NumBits> >
    {
    public:
        typedef boost::uint64_t      value_type;     // Data type to store parts of the very long integer (usually int) -
        typedef std::size_t          size_type;      // Size type of the very long integers (number of parts)

        static const std::size_t numbits = NumBits;           // Number of bits of the vli
        static const std::size_t numwords = (NumBits+63)/64;  // Number of words (here word = 64 bits) of the very long integer (eg. how many ints)
        // c - constructors, copy-swap, access   
        vli();
        explicit vli(long int num);
        //vli(vli const& r);
#if defined __GNU_MP_VERSION
        // TODO find a better solution for this.
        operator mpz_class() const;
        operator mpq_class() const;
#endif //__GNU_MP_VERSION
        friend void swap<> (vli& vli_a, vli& vli_b);
        //vli& operator= (vli r);
        value_type& operator[](size_type i);
        const value_type& operator[](size_type i) const;
        // c - negative number
        void negate();
        bool is_negative() const;
        // c - basic operator
        vli& operator >>= (long int const a); // bit shift
        vli& operator <<= (long int const a); // bit shift
        vli& operator |=  (vli const& vli_a); // bit shift
        vli& operator += (vli const& a);
        vli& operator += (long int const a);
        vli& operator -= (vli const& vli_a);
        vli& operator -= (long int a);
        vli& operator *= (long int a);
        vli& operator *= (vli const& a); // conserve the total number of bits
        vli& operator /= (vli const& a); // conserve the total number of bits
        vli& operator %= (vli const& a); // conserve the total number of bits

        vli operator -() const;
        bool operator == (vli const& vli_a) const; // need by boost::equality_comparable
        bool operator < (vli const& vli_a) const; // need by less_than_comparable<T>
        bool operator < (long int i) const; // need by less_than_comparable<T,U>
        bool operator > (long int i) const; // need by less_than_comparable<T,U>
        bool is_zero() const;
        void print_raw(std::ostream& os) const;
        void print(std::ostream& os) const;

        std::string get_str() const;
        size_type order_of_magnitude_base10(vli const& value) const;
        std::string get_str_helper_inplace(vli& value, size_type ten_exp) const;
    private:
        value_type data_[numwords];
    };

    /**
     multiply and addition operators, suite ...
     no boost::operators for VLI operator int, because I have specific assembly solver, 
     I do not want VLI tmp(int) and work with the tmp as for > < operators
     */
    template <std::size_t NumBits>
    bool is_zero(vli<NumBits> const& v);

    template <std::size_t NumBits>
    void negate_inplace(vli<NumBits>& v);

    template <std::size_t NumBits>
    const vli<NumBits> operator + (vli<NumBits> vli_a, long int b);

    template <std::size_t NumBits>
    const vli<NumBits> operator + (long int b, vli<NumBits> const& vli_a);

    template <std::size_t NumBits> //extented arithmetic
    const vli<NumBits+64> plus_extend(vli<NumBits> const& vli_a, vli<NumBits> const& vli_b);

    template <std::size_t NumBits>
    const vli<NumBits> operator - (vli<NumBits> vli_a, long int b);

    template <std::size_t NumBits>
    const vli<NumBits> operator * (vli<NumBits> vli_a, long int b);

    template <std::size_t NumBits>
    const vli<NumBits> operator * (long int b, vli<NumBits> const& a);

    template <std::size_t NumBits>
    void multiply_extend(vli<2*NumBits>& vli_res, vli<NumBits> const&  vli_a, vli<NumBits> const& vli_b); // C nt = non truncated

    template <std::size_t NumBits>
    void multiply_add_extend(vli<2*NumBits>& vli_res, vli<NumBits> const&  vli_a, vli<NumBits> const& vli_b); // C
    /**
    stream 
    */
    template <std::size_t NumBits>
    std::ostream& operator<< (std::ostream& os,  vli<NumBits> const& );
}

#endif //VLI_NUMBER_CPU_HPP
