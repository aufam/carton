#include <boost/preprocessor.hpp>

#define _CPX_REFLECT_FIELD_OP(_, T, elem) , &T::BOOST_PP_TUPLE_ELEM(2, 0, elem)

#define _CPX_REFLECT_TAG_DECL_OP(_, __, elem)                                                                                    \
    static constexpr cpx::TagInfo BOOST_PP_TUPLE_ELEM(2, 0, elem) = BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define _CPX_REFLECT_TAGS_OP(_, __, i, elem) BOOST_PP_COMMA_IF(i) BOOST_PP_TUPLE_ELEM(2, 0, elem)

#define _CPX_REFLECT_T(x) BOOST_PP_TUPLE_ELEM(2, 0, x)
#define _CPX_REFLECT_S(x) BOOST_PP_TUPLE_ELEM(2, 1, x)

// clang-format off
#define CPX_REFLECT(X, SEQ)                                                                                                  \
template <>                                                                                                                  \
struct cpx::_CPX_REFLECT_S(X) Reflect<_CPX_REFLECT_T(X)> : cpx::Fields<cpx::_CPX_REFLECT_S(X) Reflect<_CPX_REFLECT_T(X)>     \
    BOOST_PP_SEQ_FOR_EACH(_CPX_REFLECT_FIELD_OP, _CPX_REFLECT_T(X), SEQ)                   \
> {                                                                                                                          \
    BOOST_PP_SEQ_FOR_EACH(_CPX_REFLECT_TAG_DECL_OP, _, SEQ)                                \
                                                                                                                             \
    static constexpr tags_type tags() {                                                                                      \
        return std::tie(BOOST_PP_SEQ_FOR_EACH_I(_CPX_REFLECT_TAGS_OP, _, SEQ));            \
    }                                                                                                                        \
}

#define CPX_REFLECT_BEGIN(X, SEQ)                                                                                            \
template <>                                                                                                                  \
struct cpx::_CPX_REFLECT_S(X) Reflect<_CPX_REFLECT_T(X)> : cpx::Fields<cpx::_CPX_REFLECT_S(X) Reflect<_CPX_REFLECT_T(X)>     \
    BOOST_PP_SEQ_FOR_EACH_I(_CPX_REFLECT_FIELD_OP, _CPX_REFLECT_T(X), SEQ)                 \
> {                                                                                                                          \
    BOOST_PP_SEQ_FOR_EACH(_CPX_REFLECT_TAG_DECL_OP, _, SEQ)                                \
                                                                                                                             \
    static constexpr tags_type tags() {                                                                                      \
        return std::tie(BOOST_PP_SEQ_FOR_EACH_I(_CPX_REFLECT_TAGS_OP, _, SEQ));            \
    }

#define CPX_REFLECT_END };
// clang-format on
