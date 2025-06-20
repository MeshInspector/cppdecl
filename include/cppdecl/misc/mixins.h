#pragma once

#include <concepts>

namespace cppdecl
{
    // Returns `M0<Derived,M1<Derived,...Mn<Derived,Base>>>`.
    // `Derived` should inherit from the resulting type.
    // Each mixin must inherit from its second template parameter. The last mixin receives `Base`.
    template <typename Derived, typename Base, template <typename, typename> typename ...M>
    struct CombineMixins
    {
        using type = Base;
    };

    template <typename Derived, typename Base, template <typename, typename> typename M0, template <typename, typename> typename ...M>
    struct CombineMixins<Derived, Base, M0, M...>
    {
        using NextBase = typename CombineMixins<Derived, Base, M...>::type;
        using type = M0<Derived, NextBase>;
        static_assert(std::derived_from<type, NextBase>, "`Mixin<Derived, Base>` must inherit from `Base`.");
    };
}
