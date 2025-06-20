#pragma once

#include <cppdecl/declarations/data.h>
#include <cppdecl/declarations/simplify.h>
#include <cppdecl/misc/platform.h>

namespace cppdecl::SimplifyModules
{
    // https://github.com/greg7mdp/parallel-hashmap
    template <typename Derived, typename Base>
    struct Phmap : Base
    {
        [[nodiscard]] CPPDECL_CONSTEXPR bool ShouldAct(SimplifyFlags flags) const
        {
            (void)flags; // We ignore the flags anyway.
            return true;
        }

        CPPDECL_CONSTEXPR void SimplifyQualifiedNameNonRecursively(SimplifyFlags flags, QualifiedName &name)
        {
            Base::SimplifyQualifiedNameNonRecursively(flags, name);

            if (
                name.parts.size() >= 2 &&
                name.parts.at(0).AsSingleWord() == "phmap" &&
                name.parts.at(1).template_args &&
                name.parts.at(1).template_args->args.size() > 0
            )
            {
                //                               1    2                 3                    4                                            5                                            6                 7
                // phmap::flat_hash_set         <int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>                          >
                // phmap::flat_hash_map         <int, float,            phmap::Hash<int>,    phmap::EqualTo<int>,                         std::allocator<std::pair<int const, float>>  >
                // phmap::node_hash_set         <int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>                          >
                // phmap::node_hash_map         <int, float,            phmap::Hash<int>,    phmap::EqualTo<int>,                         std::allocator<std::pair<int const, float>>  >
                // phmap::parallel_flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>,                         4ul,                                         phmap::NullMutex  >
                // phmap::parallel_flat_hash_map<int, float,            phmap::Hash<int>,    phmap::EqualTo<int>,                         std::allocator<std::pair<int const, float>>, 4ul,              phmap::NullMutex>
                // phmap::parallel_node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>,                         4ul,                                         phmap::NullMutex  >
                // phmap::parallel_node_hash_map<int, float,            phmap::Hash<int>,    phmap::EqualTo<int>,                         std::allocator<std::pair<int const, float>>, 4ul,              phmap::NullMutex>
                // phmap::btree_set             <int, phmap::Less<int>, std::allocator<int>  >
                // phmap::btree_map             <int, float,            phmap::Less<int>,    std::allocator<std::pair<int const, float>>>
                // phmap::btree_multiset        <int, phmap::Less<int>, std::allocator<int>  >
                // phmap::btree_multimap        <int, float,            phmap::Less<int>,    std::allocator<std::pair<int const, float>>>

                UnqualifiedName &part = name.parts.at(1);

                const std::string_view container_name = part.AsSingleWord(SingleWordFlags::ignore_type_prefixes | SingleWordFlags::ignore_template_args);

                bool known = false;
                const bool is_flat_hash_set          = !known && container_name == "flat_hash_set"          && (known = true);
                const bool is_flat_hash_map          = !known && container_name == "flat_hash_map"          && (known = true);
                const bool is_node_hash_set          = !known && container_name == "node_hash_set"          && (known = true);
                const bool is_node_hash_map          = !known && container_name == "node_hash_map"          && (known = true);
                const bool is_parallel_flat_hash_set = !known && container_name == "parallel_flat_hash_set" && (known = true);
                const bool is_parallel_flat_hash_map = !known && container_name == "parallel_flat_hash_map" && (known = true);
                const bool is_parallel_node_hash_set = !known && container_name == "parallel_node_hash_set" && (known = true);
                const bool is_parallel_node_hash_map = !known && container_name == "parallel_node_hash_map" && (known = true);
                const bool is_btree_set              = !known && container_name == "btree_set"              && (known = true);
                const bool is_btree_map              = !known && container_name == "btree_map"              && (known = true);
                const bool is_btree_multiset         = !known && container_name == "btree_multiset"         && (known = true);
                const bool is_btree_multimap         = !known && container_name == "btree_multimap"         && (known = true);

                // Can't destroy those variables, even though they are unused, because their initialization sets `known = true`.
                (void)is_flat_hash_set;
                (void)is_node_hash_set;

                if (known)
                {
                    const bool is_map = is_flat_hash_map || is_node_hash_map || is_parallel_flat_hash_map || is_parallel_node_hash_map || is_btree_map || is_btree_multimap;

                    Type *const elem_type = std::get_if<Type>(&part.template_args->args.at(0).var);
                    Type *const mapped_type = is_map ? std::get_if<Type>(&part.template_args->args.at(1).var) : nullptr;

                    if (elem_type && (!is_map || mapped_type))
                    {
                        const bool is_parallel = is_parallel_flat_hash_set || is_parallel_flat_hash_map || is_parallel_node_hash_set || is_parallel_node_hash_map;
                        const bool is_btree = !is_parallel && (is_btree_set || is_btree_map || is_btree_multiset || is_btree_multimap);

                        if (is_parallel)
                        {
                            { // Strip `phmap::NullMutex`.
                                std::size_t expected_targ_list_size = is_map ? 7 : 6;
                                if (expected_targ_list_size == part.template_args->args.size())
                                {
                                    if (auto targ = std::get_if<Type>(&part.template_args->args.back().var))
                                    {
                                        if (
                                            targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                            targ->simple_type.name.parts.size() == 2 &&
                                            targ->simple_type.name.parts.at(0).AsSingleWord() == "phmap" &&
                                            targ->simple_type.name.parts.at(1).AsSingleWord() == "NullMutex"
                                        )
                                        {
                                            part.template_args->args.pop_back();
                                        }
                                    }
                                }
                            }

                            { // Strip `4ul`. Documentation says this number `N` gives you `2^N` submaps.
                                std::size_t expected_targ_list_size = is_map ? 6 : 5;
                                if (expected_targ_list_size == part.template_args->args.size())
                                {
                                    if (auto targ = std::get_if<PseudoExpr>(&part.template_args->args.back().var); targ && targ->tokens.size() == 1)
                                    {
                                        if (auto num = std::get_if<NumericLiteral>(&targ->tokens.front()); num && num->ToInteger() == 4)
                                        {
                                            part.template_args->args.pop_back();
                                        }
                                    }
                                }
                            }
                        }

                        { // Strip allocator.
                            std::size_t expected_targ_list_size = 4 + is_map - is_btree;
                            if (expected_targ_list_size == part.template_args->args.size())
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.back().var);
                                    targ &&
                                    (is_map ? this->GetDerived().IsAllocatorForPair(*targ, *elem_type, *mapped_type) : this->GetDerived().IsAllocatorFor(*targ, *elem_type))
                                )
                                {
                                    part.template_args->args.pop_back();
                                }
                            }
                        }

                        // Strip equality comparator.
                        if (!is_btree)
                        {
                            std::size_t expected_targ_list_size = 3 + is_map;
                            if (expected_targ_list_size == part.template_args->args.size())
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.back().var);
                                    targ &&
                                    targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                    targ->simple_type.name.parts.size() >= 2 &&
                                    targ->simple_type.name.parts.front().AsSingleWord() == "phmap"
                                )
                                {
                                    bool ok = false;

                                    // The normal comparator: `phmap::EqualTo`.
                                    if (
                                        !ok &&
                                        targ->simple_type.name.parts.size() == 2 &&
                                        targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "EqualTo" &&
                                        targ->simple_type.name.parts.at(1).TemplateArgsMatch(*elem_type)
                                    )
                                    {
                                        ok = true;
                                    }

                                    // Some custom comparators.
                                    if (
                                        !ok &&
                                        targ->simple_type.name.parts.size() == 4 &&
                                        targ->simple_type.name.parts.at(1).AsSingleWord() == "priv" &&
                                        targ->simple_type.name.parts.at(3).AsSingleWord() == "Eq"
                                    )
                                    {
                                        // The string comparator: `phmap::priv::StringHashEqT<char>::Eq`.
                                        if (
                                            !ok &&
                                            targ->simple_type.name.parts.at(2).AsSingleWord(SingleWordFlags::ignore_template_args) == "StringHashEqT" &&
                                            targ->simple_type.name.parts.at(2).template_args &&
                                            targ->simple_type.name.parts.at(2).template_args->args.size() == 1 &&
                                            std::holds_alternative<Type>(targ->simple_type.name.parts.at(2).template_args->args.front().var) // Don't check the actual type, because what would we compare it with? It's not always `char`, can be wider too.
                                        )
                                        {
                                            ok = true;
                                        }

                                        // The pointer comparator: `phmap::priv::HashEq<int*, void>::Eq`.
                                        if (
                                            !ok &&
                                            targ->simple_type.name.parts.at(2).AsSingleWord(SingleWordFlags::ignore_template_args) == "HashEq" &&
                                            targ->simple_type.name.parts.at(2).TemplateArgsMatch(*elem_type, cppdecl::Type::FromSingleWord("void"))
                                        )
                                        {
                                            ok = true;
                                        }
                                    }

                                    // On success, remove the comparator.
                                    if (ok)
                                        part.template_args->args.pop_back();
                                }
                            }
                        }

                        // Strip hash functor.
                        if (!is_btree)
                        {
                            std::size_t expected_targ_list_size = 2 + is_map;
                            if (expected_targ_list_size == part.template_args->args.size())
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.back().var);
                                    targ &&
                                    targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                    targ->simple_type.name.parts.size() >= 2 &&
                                    targ->simple_type.name.parts.front().AsSingleWord() == "phmap"
                                )
                                {
                                    bool ok = false;

                                    // The normal hash function: `phmap::Hash`.
                                    if (
                                        !ok &&
                                        targ->simple_type.name.parts.size() == 2 &&
                                        targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "Hash" &&
                                        targ->simple_type.name.parts.at(1).TemplateArgsMatch(*elem_type)
                                    )
                                    {
                                        ok = true;
                                    }

                                    // Some custom hash functions.
                                    if (
                                        !ok &&
                                        targ->simple_type.name.parts.size() == 4 &&
                                        targ->simple_type.name.parts.at(1).AsSingleWord() == "priv" &&
                                        targ->simple_type.name.parts.at(3).AsSingleWord() == "Hash"
                                    )
                                    {
                                        // The string hash function: `phmap::priv::StringHashEqT<char>::Hash`.
                                        if (
                                            !ok &&
                                            targ->simple_type.name.parts.at(2).AsSingleWord(SingleWordFlags::ignore_template_args) == "StringHashEqT" &&
                                            targ->simple_type.name.parts.at(2).template_args &&
                                            targ->simple_type.name.parts.at(2).template_args->args.size() == 1 &&
                                            std::holds_alternative<Type>(targ->simple_type.name.parts.at(2).template_args->args.front().var) // Don't check the actual type, because what would we compare it with? It's not always `char`, can be wider too.
                                        )
                                        {
                                            ok = true;
                                        }

                                        // The pointer hash function: `phmap::priv::HashEq<int*, void>::Hash`.
                                        if (
                                            !ok &&
                                            targ->simple_type.name.parts.at(2).AsSingleWord(SingleWordFlags::ignore_template_args) == "HashEq" &&
                                            targ->simple_type.name.parts.at(2).TemplateArgsMatch(*elem_type, cppdecl::Type::FromSingleWord("void"))
                                        )
                                        {
                                            ok = true;
                                        }
                                    }

                                    // On success, remove the hash function.
                                    if (ok)
                                        part.template_args->args.pop_back();
                                }
                            }
                        }

                        // Strip less comparator.
                        if (is_btree)
                        {
                            std::size_t expected_targ_list_size = 2 + is_map;
                            if (expected_targ_list_size == part.template_args->args.size())
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.back().var);
                                    targ &&
                                    targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                    targ->simple_type.name.parts.size() >= 2 &&
                                    targ->simple_type.name.parts.at(0).AsSingleWord() == "phmap" &&
                                    targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "Less" &&
                                    targ->simple_type.name.parts.at(1).TemplateArgsMatch(*elem_type)
                                )
                                {
                                    part.template_args->args.pop_back();
                                }
                            }
                        }
                    }
                }
            }
        }
    };
}
