---
description: safety checks when refactoring state representations or type changes
---

## When refactoring a variable's type (e.g. bool/int to enum, raw pointer to smart pointer, int to class):

1. List ALL initialization sites for the old variable
2. List ALL guard conditions / comparisons that check the variable
3. Trace the control flow from initialization through first use of each guard
4. Verify that the new type's initial value passes through all guards identically to the old type's initial value
5. Pay special attention to tri-state / lazy initialization patterns where the initial value is "unknown" and gets resolved later through a probe or callback
6. Check for implicit conversions that the old type allowed but the new type does not (e.g. `if (!uint8_var)` vs `if (enum_var == State::No)`)

## When changing link dependencies in CMake:

1. Check both shared and static build targets
2. Verify the installed/exported build, not just the local build
3. Use correct visibility: PRIVATE for shared libs (symbols resolved at link time), PUBLIC for static libs (dependency must propagate to consumers)

## When moving defaulted special member functions (constructor, destructor, move/copy):

1. If a class has a `unique_ptr<ForwardDeclaredType>` member, the destructor, move constructor, and move assignment must be defined in the .cpp where the type is complete
2. `= default` in the header forces instantiation in every including TU — move it to the .cpp
3. Verify that changing from "explicitly defaulted at first declaration" to "user-provided" does not affect other implicit special member generation (check Rule of Five)
