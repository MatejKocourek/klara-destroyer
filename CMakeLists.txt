# CMakeList.txt: Projekt CMake pro KlaraDestroyer, sem přidejte logiku zdrojového
# kódu a definic specifickou pro projekt.
#
cmake_minimum_required (VERSION 3.19)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

project ("KlaraDestroyer")

# Přidejte zdrojový kód ke spustitelnému souboru tohoto projektu.
add_executable (KlaraDestroyer "KlaraDestroyer.h" "stack_vector.h" "main.cpp" "stack_string")

set_property(TARGET KlaraDestroyer APPEND PROPERTY ISPC_INSTRUCTION_SETS avx512skx-i32x835)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-O3>;$<$<CONFIG:Release>:-march=native>;$<$<CONFIG:Release>:-fno-stack-protector>")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-Weverything")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-Wno-c++98-compat")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-Wno-c++98-compat-pedantic")

	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-ffast-math")
	#set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-fno-finite-math-only")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-fhonor-infinities")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "-fhonor-nans")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# using GCC
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	# using Intel C++
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set_property(TARGET KlaraDestroyer APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/GL>;$<$<CONFIG:Release>:/arch:AVX512>;$<$<CONFIG:Release>:/fp:fast>;$<$<CONFIG:Release>:/GS->")
	#set_property(TARGET KlaraDestroyer APPEND PROPERTY LINK_FLAGS_RELEASE "/LTCG")
endif()
