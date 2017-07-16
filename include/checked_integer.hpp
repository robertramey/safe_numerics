#ifndef BOOST_NUMERIC_CHECKED_INTEGER_HPP
#define BOOST_NUMERIC_CHECKED_INTEGER_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// contains operations for doing checked aritmetic on NATIVE
// C++ types.

#include <limits>
#include <type_traits> // is_integral, make_unsigned
#include <algorithm>   // std::max
#include <boost/utility/enable_if.hpp>

#include "checked_result.hpp"
#include "checked_default.hpp"
#include "safe_compare.hpp"
#include "utility.hpp"
#include "exception.hpp"

namespace boost {
namespace numeric {

// utility

template<bool tf>
using bool_type = typename boost::mpl::if_c<tf, std::true_type, std::false_type>::type;

////////////////////////////////////////////////////
// layer 0 - implement safe operations for intrinsic integers
// Note presumption of twos complement integer arithmetic

template<typename R, typename T>
struct checked_unary_operation<R, T,
    typename std::enable_if<
        std::is_integral<R>::value
        && std::is_integral<T>::value
    >::type
>{
    ////////////////////////////////////////////////////
    // safe casting on primitive types

    struct cast_impl_detail {
        constexpr static checked_result<R>
        cast_impl(
            const T & t,
            std::true_type, // R is signed
            std::true_type  // T is signed
        ) noexcept {
            // INT32-C Ensure that operations on signed
            // integers do not overflow
            return
            boost::numeric::safe_compare::greater_than(
                t,
                std::numeric_limits<R>::max()
            ) ?
                checked_result<R>(
                    safe_numerics_error::positive_overflow_error,
                    "converted signed value too large"
                )
            :
            boost::numeric::safe_compare::less_than(
                t,
                std::numeric_limits<R>::min()
            ) ?
                checked_result<R>(
                    safe_numerics_error::negative_overflow_error,
                    "converted signed value too small"
                )
            :
                checked_result<R>(static_cast<R>(t))
            ;
        }
        constexpr static checked_result<R>
        cast_impl(
            const T & t,
            std::true_type,  // R is signed
            std::false_type  // T is unsigned
        ) noexcept {
            // INT30-C Ensure that unsigned integer operations
            // do not wrap
            return
            boost::numeric::safe_compare::greater_than(
                t,
                std::numeric_limits<R>::max()
            ) ?
                checked_result<R>(
                    safe_numerics_error::positive_overflow_error,
                    "converted unsigned value too large"
                )
            :
                checked_result<R>(static_cast<R>(t))
            ;
        }
        constexpr static checked_result<R>
        cast_impl(
            const T & t,
            std::false_type, // R is unsigned
            std::false_type  // T is unsigned
        ) noexcept {
            // INT32-C Ensure that operations on unsigned
            // integers do not overflow
            return
            boost::numeric::safe_compare::greater_than(
                t,
                std::numeric_limits<R>::max()
            ) ?
                checked_result<R>(
                    safe_numerics_error::positive_overflow_error,
                    "converted unsigned value too large"
                )
            :
                checked_result<R>(static_cast<R>(t))
            ;
        }
        constexpr static checked_result<R>
        cast_impl(
            const T & t,
            std::false_type, // R is unsigned
            std::true_type   // T is signed
        ) noexcept {
            return
            boost::numeric::safe_compare::less_than(t, 0) ?
                checked_result<R>(
                    safe_numerics_error::domain_error,
                    "converted negative value to unsigned"
                )
            :
            boost::numeric::safe_compare::greater_than(
                t,
                std::numeric_limits<R>::max()
            ) ?
                checked_result<R>(
                    safe_numerics_error::positive_overflow_error,
                    "converted signed value too large"
                )
            :
                checked_result<R>(static_cast<R>(t))
            ;
        }
    }; // cast_impl_detail

    constexpr static checked_result<R>
    cast(const T & t) noexcept {
        return
            cast_impl_detail::cast_impl(
                t,
                std::is_signed<R>(),
                std::is_signed<T>()
            );
    }
};

template<typename R, typename T>
struct checked_unary_operation<R, T,
    typename std::enable_if<
        std::is_floating_point<R>::value
        && std::is_integral<T>::value
    >::type
>{
    constexpr static checked_result<R>
    cast(T & t) noexcept {
        return static_cast<R>(t);
    }
};

template<typename R, typename T, typename U>
struct checked_binary_operation<R, T, U,
    typename std::enable_if<
        std::is_integral<R>::value
        && std::is_integral<T>::value
        && std::is_integral<U>::value
    >::type
>{
    ////////////////////////////////////////////////////
    // safe addition on primitive types

    struct add_impl_detail {
        // result unsigned
        constexpr static checked_result<R> add(
            const R t,
            const R u,
            std::false_type // R unsigned
        ) noexcept {
            return
                // INT30-C. Ensure that unsigned integer operations do not wrap
                std::numeric_limits<R>::max() - u < t ?
                    checked_result<R>(
                        safe_numerics_error::positive_overflow_error,
                        "addition result too large"
                    )
                :
                    checked_result<R>(t + u)
            ;
        }

        // result signed
        constexpr static checked_result<R> add(
            const R t,
            const R u,
            std::true_type // R signed
        ) noexcept {
            return
                // INT32-C. Ensure that operations on signed integers do not result in overflow
                ((u > 0) && (t > (std::numeric_limits<R>::max() - u))) ?
                    checked_result<R>(
                        safe_numerics_error::positive_overflow_error,
                        "addition result too large"
                    )
                :
                ((u < 0) && (t < (std::numeric_limits<R>::min() - u))) ?
                    checked_result<R>(
                        safe_numerics_error::negative_overflow_error,
                        "addition result too low"
                    )
                :
                    checked_result<R>(t + u)
            ;
        }
    }; // add_impl_detail

    constexpr static checked_result<R>
    add(const T & t, const U & u) noexcept {
        const checked_result<R> rt(checked::cast<R>(t));
        if(rt.exception() )
            return rt;
        const checked_result<R> ru(checked::cast<R>(u));
        if(ru.exception() )
            return ru;
        return add_impl_detail::add(rt, ru, std::is_signed<R>());
    }

    ////////////////////////////////////////////////////
    // safe subtraction on primitive types
    struct subtract_impl_detail {

        // result unsigned
        constexpr static checked_result<R> subtract(
            const R t,
            const R u,
            std::false_type // R is unsigned
        ) noexcept {
            // INT30-C. Ensure that unsigned integer operations do not wrap
            return
                t < u ?
                    checked_result<R>(
                        safe_numerics_error::range_error,
                        "subtraction result cannot be negative"
                    )
                :
                    checked_result<R>(t - u)
            ;
        }

        // result signed
        constexpr static checked_result<R> subtract(
            const R t,
            const R u,
            std::true_type // R is signed
        ) noexcept { // INT32-C
            return
                // INT32-C. Ensure that operations on signed integers do not result in overflow
                ((u > 0) && (t < (std::numeric_limits<R>::min() + u))) ?
                    checked_result<R>(
                        safe_numerics_error::positive_overflow_error,
                        "subtraction result overflows result type"
                    )
                :
                ((u < 0) && (t > (std::numeric_limits<R>::max() + u))) ?
                    checked_result<R>(
                        safe_numerics_error::negative_overflow_error,
                        "subtraction result overflows result type"
                    )
                :
                    checked_result<R>(t - u)
            ;
        }

    }; // subtract_impl_detail

    constexpr static checked_result<R> subtract(const T & t, const U & u) noexcept {
        const checked_result<R> rt(checked::cast<R>(t));
        if(rt.exception() )
            return rt;
        const checked_result<R> ru(checked::cast<R>(u));
        if(ru.exception() )
            return ru;
        return subtract_impl_detail::subtract(t, u, std::is_signed<R>());
    }

    ////////////////////////////////////////////////////
    // safe multiplication on primitive types

    struct multiply_impl_detail {

        // result unsigned
        constexpr static checked_result<R> multiply(
            const R t,
            const R u,
            std::false_type,  // R is unsigned
            std::false_type   // !(sizeof(R) > sizeof(std::uintmax_t) / 2)

        ) noexcept {
            // INT30-C
            // fast method using intermediate result guaranteed not to overflow
            // todo - replace std::uintmax_t with a size double the size of R
            using i_type = std::uintmax_t;
            return
                static_cast<i_type>(t) * static_cast<i_type>(u)
                > std::numeric_limits<R>::max() ?
                    checked_result<R>(
                        safe_numerics_error::positive_overflow_error,
                        "multiplication overflow"
                    )
                :
                    checked_result<R>(t * u)
            ;
        }
        constexpr static checked_result<R> multiply(
            const R t,
            const R u,
            std::false_type,  // R is unsigned
            std::true_type    // (sizeof(R) > sizeof(std::uintmax_t) / 2)

        ) noexcept {
            // INT30-C
            return
                u > 0 && t > std::numeric_limits<R>::max() / u ?
                    checked_result<R>(
                        safe_numerics_error::positive_overflow_error,
                        "multiplication overflow"
                    )
                :
                    checked_result<R>(t * u)
            ;
        }

        // result signed
        constexpr static checked_result<R> multiply(
            const R t,
            const R u,
            std::true_type, // R is signed
            std::false_type // ! (sizeof(R) > (sizeof(std::intmax_t) / 2))

        ) noexcept {
            // INT30-C
            // fast method using intermediate result guaranteed not to overflow
            // todo - replace std::intmax_t with a size double the size of R
            using i_type = std::intmax_t;
            return
                (
                    static_cast<i_type>(t) * static_cast<i_type>(u)
                    > static_cast<i_type>(std::numeric_limits<R>::max())
                ) ?
                    checked_result<R>(
                        safe_numerics_error::positive_overflow_error,
                        "multiplication overflow"
                    )
                :
                (
                    static_cast<i_type>(t) * static_cast<i_type>(u)
                    < static_cast<i_type>(std::numeric_limits<R>::min())
                ) ?
                    checked_result<R>(
                        safe_numerics_error::negative_overflow_error,
                        "multiplication overflow"
                    )
                :
                    checked_result<R>(t * u)
            ;
        }
        constexpr static checked_result<R> multiply(
            const R t,
            const R u,
            std::true_type,   // R is signed
            std::true_type    // (sizeof(R) > (sizeof(std::intmax_t) / 2))
        ) noexcept { // INT32-C
            return t > 0 ?
                u > 0 ?
                    t > std::numeric_limits<R>::max() / u ?
                        checked_result<R>(
                            safe_numerics_error::positive_overflow_error,
                            "multiplication overflow"
                        )
                    :
                        checked_result<R>(t * u)
                : // u <= 0
                    u < std::numeric_limits<R>::min() / t ?
                        checked_result<R>(
                            safe_numerics_error::negative_overflow_error,
                            "multiplication overflow"
                        )
                    :
                        checked_result<R>(t * u)
            : // t <= 0
                u > 0 ?
                    t < std::numeric_limits<R>::min() / u ?
                        checked_result<R>(
                            safe_numerics_error::negative_overflow_error,
                            "multiplication overflow"
                        )
                    :
                        checked_result<R>(t * u)
                : // u <= 0
                    t != 0 && u < std::numeric_limits<R>::max() / t ?
                        checked_result<R>(
                            safe_numerics_error::positive_overflow_error,
                            "multiplication overflow"
                        )
                    :
                        checked_result<R>(t * u)
            ;
        }
    }; // multiply_impl_detail

    constexpr static checked_result<R> multiply(const T & t, const U & u) noexcept {
        checked_result<R> rt(checked::cast<R>(t));
        if(rt.exception() )
            return rt;
        checked_result<R> ru(checked::cast<R>(u));
        if(ru.exception() )
            return ru;
        return multiply_impl_detail::multiply(
            t,
            u,
            std::is_signed<R>(),
            std::integral_constant<
                bool,
                (sizeof(R) > sizeof(std::uintmax_t) / 2)
            >()
        );
    }

    ////////////////////////////////
    // safe division on unsafe types

    struct divide_impl_detail {

        //
        constexpr static checked_result<R> divide(
            const R & t,
            const R & u,
            std::false_type // R is unsigned
        ) noexcept {
            return t / u;
        }

        constexpr static checked_result<R> divide(
            const R & t,
            const R & u,
            std::true_type // R is signed
        ) noexcept {
            return
                (u == -1 && t == std::numeric_limits<R>::min()) ?
                    checked_result<R>(
                        safe_numerics_error::range_error,
                        "result cannot be represented"
                    )
                :
                    checked_result<R>(t / u)
            ;
        }
    }; // divide_impl_detail

    // note that we presume that the size of R >= size of T
    constexpr static checked_result<R> divide(const T & t, const U & u) noexcept {
        if(u == 0){
            return checked_result<R>(
                safe_numerics_error::domain_error,
                "divide by zero"
            );
        }
        checked_result<R> tx = checked::cast<R>(t);
        checked_result<R> ux = checked::cast<R>(u);
        if(tx.exception()
        || ux.exception())
            return checked_result<R>(
                safe_numerics_error::domain_error,
                "failure converting argument types"
            );
        return divide_impl_detail::divide(tx, ux, std::is_signed<R>());
    }

    ////////////////////////////////
    // safe modulus on unsafe types

    // built-in abs isn't constexpr - so fix this here
    template<class Tx>
    constexpr static std::make_unsigned_t<Tx>
    abs(const Tx & t) noexcept {
        return (t < 0 && t != std::numeric_limits<Tx>::min()) ?
            -t
        :
            t
        ;
    }

    constexpr static checked_result<R> modulus(const T & t, const U & u) noexcept {
        if(0 == u)
            return checked_result<R>(
                safe_numerics_error::domain_error,
                "denominator is zero"
            );

        // why to we need abs here? the sign of the modulus is the sign of the
        // dividend. Consider -128 % -1  The result of this operation should be -1
        // but if I use t % u the x86 hardware uses the divide instruction
        // capturing the modulus as a side effect.  When it does this, it
        // invokes the operation -128 / -1 -> 128 which overflows a signed type
        // and provokes a hardware exception.  We can fix this using abs()
        // since -128 % -1 = -128 % 1 = 0
        return t % abs(u);
    }

    ////////////////////////////////
    // safe comparison on unsafe types
    constexpr static checked_result<bool> less_than(
        const T & t,
        const U & u
    ) noexcept {
        const checked_result<R> tx = checked::cast<R>(t);
        if(tx.exception())
            return tx;
        const checked_result<R> ux = checked::cast<R>(u);
        if(ux.exception())
            return ux;
        return static_cast<R>(tx) < static_cast<R>(ux);
    }

    constexpr static checked_result<bool> greater_than(
        const T & t,
        const U & u
    ) noexcept {
        const checked_result<R> tx = checked::cast<R>(t);
        if(tx.exception())
            return tx;
        const checked_result<R> ux = checked::cast<R>(u);
        if(ux.exception())
            return ux;
        return static_cast<R>(tx) > static_cast<R>(ux);
    }

    constexpr static checked_result<bool> equal(
        const T & t,
        const U & u
    ) noexcept {
        checked_result<R> tx = checked::cast<R>(t);
        if(tx.exception())
            return tx;
        checked_result<R> ux = checked::cast<R>(u);
        if(ux.exception())
            return ux;
        return static_cast<R>(tx) == static_cast<R>(ux);
    }

    ///////////////////////////////////
    // shift operations

    struct left_shift_integer_detail {

        #if 0
        // todo - optimize for gcc to exploit builtin
        /* for gcc compilers
        int __builtin_clz (unsigned int x)
              Returns the number of leading 0-bits in x, starting at the
              most significant bit position.  If x is 0, the result is undefined.
        */

        #ifndef __has_feature         // Optional of course.
          #define __has_feature(x) 0  // Compatibility with non-clang compilers.
        #endif

        template<typename T>
        constexpr unsigned int leading_zeros(const T & t){
            if(0 == t)
                return 0;
            #if __has_feature(builtin_clz)
                return  __builtin_clz(t);
            #else
            #endif
        }
        #endif

        // INT34-C C++

        // standard paragraph 5.8 / 2
        // The value of E1 << E2 is E1 left-shifted E2 bit positions;
        // vacated bits are zero-filled.
        constexpr static checked_result<R> left_shift(
            const T & t,
            const U & u,
            std::false_type // T is unsigned
        ) noexcept {
            // the value of the result is E1 x 2^E2, reduced modulo one more than
            // the maximum value representable in the result type.

            // see 5.8 & 1
            // if right operand is
            // greater than or equal to the length in bits of the promoted left operand.
            if(
                safe_compare::greater_than(
                    u,
                    std::numeric_limits<R>::digits - utility::significant_bits(t)
                )
            ){
                // behavior is undefined
                return checked_result<R>(
                   safe_numerics_error::undefined_behavior,
                   "shifting left more bits than available is undefined behavior"
                );
            }
            return t << u;
        }

        constexpr static checked_result<R> left_shift(
            const T & t,
            const U & u,
            std::true_type // T is signed
        ) noexcept {
            // and [E1] has a non-negative value
            if(t >= 0){
                // and E1 x 2^E2 is representable in the corresponding
                // unsigned type of the result type,

                // then that value, converted to the result type,
                // is the resulting value
                return checked::left_shift<R>(
                    static_cast<typename std::make_unsigned<T>::type>(t),
                    u
                );
            }
            // otherwise, the behavior is undefined.
            return checked_result<R>(
               safe_numerics_error::undefined_behavior,
               "shifting a negative value is undefined behavior"
            );
        }

    }; // left_shift_integer_detail

    constexpr static checked_result<R> left_shift(
        const T & t,
        const U & u
    ) noexcept {
        // INT34-C - Do not shift an expression by a negative number of bits

        // standard paragraph 5.8 & 1
        // if the right operand is negative
        if(u == 0){
            return t;
        }
        if(u < 0){
            return checked_result<R>(
               safe_numerics_error::implementation_defined_behavior,
               "shifting negative amount is implementation defined behavior"
            );
        }
        if(u > std::numeric_limits<R>::digits){
            // behavior is undefined
            return checked_result<R>(
               safe_numerics_error::implementation_defined_behavior,
               "shifting more bits than available is implementation defined behavior"
            );
        }
        if(t == 0)
            return checked::cast<R>(t);
        return left_shift_integer_detail::left_shift(t, u, std::is_signed<T>());
    }

// right shift

    struct right_shift_integer_detail {

        // INT34-C C++

        // standard paragraph 5.8 / 3
        // The value of E1 << E2 is E1 left-shifted E2 bit positions;
        // vacated bits are zero-filled.
        constexpr static checked_result<R> right_shift(
            const T & t,
            const U & u,
            std::false_type // T is unsigned
        ) noexcept {
            // the value of the result is the integral part of the
            // quotient of E1/2E2
            return checked::cast<R>(t >> u);
        }

        constexpr static checked_result<R> right_shift(
            const T & t,
            const U & u,
            std::true_type  // T is signed;
        ) noexcept {
        if(t < 0){
            // note that the C++ standard considers this case is "implemenation
            // defined" rather than "undefined".
            return checked_result<R>(
                safe_numerics_error::implementation_defined_behavior,
                "shifting a negative value is implementation defined behavior"
            );
         }

         // the value is the integral part of E1 / 2^E2,
         return checked::cast<R>(t >> u);
        }
    }; // right_shift_integer_detail

constexpr static checked_result<R> right_shift(
    const T & t,
    const U & u
) noexcept {
    // INT34-C - Do not shift an expression by a negative number of bits

    // standard paragraph 5.8 & 1
    // if the right operand is negative
    if(u == 0){
        return t;
    }
    if(u < 0){
        return checked_result<R>(
           safe_numerics_error::implementation_defined_behavior,
           "shifting negative amount is implementation defined behavior"
        );
    }
    if(u > std::numeric_limits<R>::digits){
        // behavior is undefined
        return checked_result<R>(
           safe_numerics_error::implementation_defined_behavior,
           "shifting more bits than available is implementation defined behavior"
        );
    }
    if(t == 0)
        return checked::cast<R>(0);
    return right_shift_integer_detail::right_shift(t, u ,std::is_signed<T>());
}

///////////////////////////////////
// bitwise operations

// INT13-C Note: We don't enforce recommendation as acually written
// as it would break too many programs.  Specifically, we permit signed
// integer operands.

constexpr static checked_result<R> bitwise_or(const T & t, const U & u) noexcept {
    using namespace boost::numeric::utility;
    const unsigned int result_size
        = std::max(significant_bits(t), significant_bits(u));

    if(result_size > bits_type<R>::value){
        return checked_result<R>{
            safe_numerics_error::positive_overflow_error,
            "result type too small to hold bitwise or"
        };
    }
    return t | u;
}

constexpr static checked_result<R> bitwise_xor(const T & t, const U & u) noexcept {
    using namespace boost::numeric::utility;
    const unsigned int result_size
        = std::max(significant_bits(t), significant_bits(u));

    if(result_size > bits_type<R>::value){
        return checked_result<R>{
            safe_numerics_error::positive_overflow_error,
            "result type too small to hold bitwise or"
        };
    }
    return t ^ u;
}

constexpr static checked_result<R> bitwise_and(const T & t, const U & u) noexcept {
    using namespace boost::numeric::utility;
    const unsigned int result_size
        = std::min(significant_bits(t), significant_bits(u));

    if(result_size > bits_type<R>::value){
        return checked_result<R>{
            safe_numerics_error::positive_overflow_error,
            "result type too small to hold bitwise or"
        };
    }
    return t & u;
}

}; // checked_binary_operation

} // numeric
} // boost

#endif // BOOST_NUMERIC_CHECKED_INTEGER_HPP