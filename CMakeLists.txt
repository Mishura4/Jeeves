cmake_minimum_required(VERSION 3.16)

project(mimiron LANGUAGES CXX)

set(DPP_CORO on)
set(DPP_USE_PCH on)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

add_subdirectory(dep)

FILE(GLOB_RECURSE MIMIRON_SRC src/*.cpp src/*.h)

add_executable(Mimiron ${MIMIRON_SRC})

target_link_libraries(Mimiron PUBLIC dpp sol2::sol2 liblua mysql)

target_compile_options(Mimiron
	PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/wd4251 /utf-8 /permissive- /Zc:preprocessor>
)

target_compile_features(Mimiron
	PUBLIC
		cxx_std_23
)

target_include_directories(Mimiron PRIVATE src)

if (MSVC)
	if (NOT WINDOWS_32_BIT)
		set(DPP_DEP_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/dep/DPP/win32/bin")
	else()
		set(DPP_DEP_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/dep/DPP/win32/32")
	endif()

	add_custom_command(TARGET Mimiron
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${DPP_DEP_DIRECTORY}/libcrypto-1_1-x64.dll"
			"${DPP_DEP_DIRECTORY}/libsodium.dll"
			"${DPP_DEP_DIRECTORY}/libssl-1_1-x64.dll"
			"${DPP_DEP_DIRECTORY}/opus.dll"
			"${DPP_DEP_DIRECTORY}/zlib1.dll"
			$<TARGET_FILE:dpp>
			$<TARGET_FILE_DIR:Mimiron>
	)
endif()
