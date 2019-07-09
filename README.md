# Posit-Multiplier_FloPoCo
Operator of a parametrized Posit multiplier for the FloPoCo tool (<http://flopoco.gforge.inria.fr/>).

## Instalation in FloPoCo version 4.1.2
- Edit `CMakeLists.txt` adding `src/PositMult`
- Edit `src/FloPoCo.hpp` adding `#include "Posit/PositMult.hpp"`
- Edit `src/main.cpp` performing similar action
- Compile and fix

## New features of this template
In contrast with previous posit implementations, any posit configuration multiplier can be synthetized. This means, not only for _es > 0_, but even lenght of 0 for the exponent field can be assigned.
