/*
 * Ambient, License - Version 1.0 - May 3rd, 2012
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef AMBIENT_MODELS_VELVET_TRANSFORMABLE
#define AMBIENT_MODELS_VELVET_TRANSFORMABLE

namespace ambient { namespace models { namespace velvet {

    class transformable {
    public:
        union numeric_union { 
            bool b; 
            double d; 
            std::complex<double> c; 
            operator bool& ();
            operator double& ();
            operator std::complex<double>& ();
            void operator = (bool value);
            void operator = (double value);
            void operator = (std::complex<double> value);
            numeric_union(){ }
        };
        void* operator new  (size_t, void*);
        void operator delete (void*, void*);
        virtual numeric_union eval() const = 0;
        virtual transformable& operator += (transformable& r) = 0;

        const transformable* l;
        const transformable* r;
        mutable numeric_union v;
        void* generator;
    };

    template <typename T>
    class transformable_value : public transformable {
    public:
        transformable_value(T value);
        virtual transformable::numeric_union eval() const;
        virtual transformable& operator += (transformable& r);
    };

    template <typename T, typename FP, FP OP>
    class transformable_expr : public transformable {
    public:
        transformable_expr(const transformable* l);
        transformable_expr(const transformable* l, const transformable* r);
        virtual transformable::numeric_union eval() const;
        virtual transformable& operator += (transformable& r); 
    };

} } }

#endif
