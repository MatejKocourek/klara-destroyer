# CMakeList.txt: Projekt CMake pro KlaraDestroyer, sem přidejte logiku zdrojového
# kódu a definic specifickou pro projekt.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 23)

project ("KlaraDestroyer")

# Přidejte zdrojový kód ke spustitelnému souboru tohoto projektu.
add_executable (KlaraDestroyer "KlaraDestroyer.cpp" "KlaraDestroyer.h" "static_vector.h")

set_target_properties(KlaraDestroyer PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:/GL>;$<$<CONFIG:Release>:/arch:AVX2>;$<$<CONFIG:Release>:/fp:fast>")
set_target_properties(KlaraDestroyer PROPERTIES LINK_FLAGS_RELEASE "/LTCG")


# TODO: V případě potřeby přidejte testy a cíle instalace.
