
add_subdirectory(DPP)
add_subdirectory(sol2)
add_subdirectory(pfr)

set(OSMANIP_TESTS off)
add_subdirectory(termcolor)

project(lua LANGUAGES C)

find_package(MySQL REQUIRED COMPONENTS lib)

add_library(mysql INTERFACE)

target_link_libraries(mysql
  INTERFACE
    MySQL::MySQL)

add_library(lapi OBJECT
	lua/lapi.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lapi.h
	lua/llimits.h
	lua/lstate.h

	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldebug.h
	lua/ldo.h
	lua/lfunc.h
	lua/lgc.h
	lua/lstring.h

	lua/ltable.h
	lua/lundump.h
	lua/lvm.h)

add_library(lauxlib OBJECT
	lua/lauxlib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h)

add_library(lbaselib OBJECT
	lua/lbaselib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(lcode OBJECT
	lua/lcode.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lcode.h
	lua/llex.h
	lua/lobject.h

	lua/llimits.h
	lua/lzio.h
	lua/lmem.h
	lua/lopcodes.h
	lua/lparser.h
	lua/ldebug.h
	lua/lstate.h
	lua/ltm.h

	lua/ldo.h
	lua/lgc.h
	lua/lstring.h
	lua/ltable.h
	lua/lvm.h)

add_library(lcorolib OBJECT
	lua/lcorolib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(lctype OBJECT
	lua/lctype.c
	lua/lprefix.h
	lua/lctype.h
	lua/lua.h
	lua/luaconf.h
	lua/llimits.h)

add_library(ldblib OBJECT
	lua/ldblib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(ldebug OBJECT
	lua/ldebug.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lapi.h
	lua/llimits.h
	lua/lstate.h

	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/lcode.h
	lua/llex.h
	lua/lopcodes.h
	lua/lparser.h

	lua/ldebug.h
	lua/ldo.h
	lua/lfunc.h
	lua/lstring.h
	lua/lgc.h
	lua/ltable.h
	lua/lvm.h)

add_library(ldo OBJECT
	lua/ldo.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lapi.h
	lua/llimits.h
	lua/lstate.h

	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldebug.h
	lua/ldo.h
	lua/lfunc.h
	lua/lgc.h
	lua/lopcodes.h

	lua/lparser.h
	lua/lstring.h
	lua/ltable.h
	lua/lundump.h
	lua/lvm.h)

add_library(ldump OBJECT
	lua/ldump.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lobject.h
	lua/llimits.h
	lua/lstate.h

	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/lundump.h)

add_library(lfunc OBJECT
	lua/lfunc.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h

	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lfunc.h
	lua/lgc.h)

add_library(lgc OBJECT
	lua/lgc.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h

	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lfunc.h
	lua/lgc.h
	lua/lstring.h
	lua/ltable.h)

add_library(linit OBJECT
	lua/linit.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lualib.h
	lua/lauxlib.h)

add_library(liolib OBJECT
	lua/liolib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(llex OBJECT
	lua/llex.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lctype.h
	lua/llimits.h
	lua/ldebug.h

	lua/lstate.h
	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lgc.h
	lua/llex.h
	lua/lparser.h

	lua/lstring.h
	lua/ltable.h)

add_library(lmathlib OBJECT
	lua/lmathlib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(lmem OBJECT
	lua/lmem.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h

	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lgc.h)

add_library(loadlib OBJECT
	lua/loadlib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(lobject OBJECT
	lua/lobject.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lctype.h
	lua/llimits.h

	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lstring.h
	lua/lgc.h

	lua/lvm.h)

add_library(lopcodes OBJECT
	lua/lopcodes.c
	lua/lprefix.h
	lua/lopcodes.h
	lua/llimits.h
	lua/lua.h
	lua/luaconf.h)
add_library(loslib OBJECT
	lua/loslib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)
add_library(lparser OBJECT
	lua/lparser.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lcode.h
	lua/llex.h
	lua/lobject.h

	lua/llimits.h
	lua/lzio.h
	lua/lmem.h
	lua/lopcodes.h
	lua/lparser.h
	lua/ldebug.h
	lua/lstate.h
	lua/ltm.h

	lua/ldo.h
	lua/lfunc.h
	lua/lstring.h
	lua/lgc.h
	lua/ltable.h)

add_library(lstate OBJECT
	lua/lstate.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lapi.h
	lua/llimits.h
	lua/lstate.h

	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldebug.h
	lua/ldo.h
	lua/lfunc.h
	lua/lgc.h
	lua/llex.h

	lua/lstring.h
	lua/ltable.h)

add_library(lstring OBJECT
	lua/lstring.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h

	lua/lobject.h
	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lstring.h
	lua/lgc.h)

add_library(lstrlib OBJECT
	lua/lstrlib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(ltable OBJECT
	lua/ltable.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h
	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lgc.h
	lua/lstring.h
	lua/ltable.h
	lua/lvm.h)

add_library(ltablib OBJECT
	lua/ltablib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(ltests OBJECT
	lua/ltests.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lapi.h
	lua/llimits.h
	lua/lstate.h

	lua/lobject.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/lauxlib.h
	lua/lcode.h
	lua/llex.h
	lua/lopcodes.h

	lua/lparser.h
	lua/lctype.h
	lua/ldebug.h
	lua/ldo.h
	lua/lfunc.h
	lua/lopnames.h
	lua/lstring.h
	lua/lgc.h

	lua/ltable.h
	lua/lualib.h)

add_library(ltm OBJECT
	lua/ltm.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h

	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lgc.h
	lua/lstring.h
	lua/ltable.h
	lua/lvm.h)

add_library(luao OBJECT
	lua/lua.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(lundump OBJECT
	lua/lundump.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h

	lua/lobject.h
	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lfunc.h
	lua/lstring.h
	lua/lgc.h

	lua/lundump.h)

add_library(lutf8lib OBJECT
	lua/lutf8lib.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/lauxlib.h
	lua/lualib.h)

add_library(lvm OBJECT
	lua/lvm.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/ldebug.h
	lua/lstate.h
	lua/lobject.h

	lua/llimits.h
	lua/ltm.h
	lua/lzio.h
	lua/lmem.h
	lua/ldo.h
	lua/lfunc.h
	lua/lgc.h
	lua/lopcodes.h
	lua/lstring.h

	lua/ltable.h
	lua/lvm.h
	lua/ljumptab.h)

add_library(lzio OBJECT
	lua/lzio.c
	lua/lprefix.h
	lua/lua.h
	lua/luaconf.h
	lua/llimits.h
	lua/lmem.h
	lua/lstate.h

	lua/lobject.h
	lua/ltm.h
	lua/lzio.h)

set(CORE_T	liblua)
set(CORE_O	lapi lcode lctype ldebug ldo ldump lfunc lgc llex
	lmem lobject lopcodes lparser lstate lstring ltable
	ltm lundump lvm lzio ltests)
set(AUX_O	lauxlib)
set(LIB_O	lbaselib ldblib liolib lmathlib loslib ltablib lstrlib
	lutf8lib loadlib lcorolib linit)

set(LUA_T	lua)
set(LUA_O	luao)

if(WIN32)
	# add_compile_definitions(LUA_BUILD_AS_DLL)
endif()

add_library(${CORE_T} STATIC)

if(UNIX)
	target_compile_definitions(${CORE_T} PUBLIC LUA_USE_LINUX)
endif()

target_include_directories(${CORE_T} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lua/)

target_link_libraries(${CORE_T} PRIVATE ${CORE_O} ${AUX_O} ${LIB_O})

add_executable(${LUA_T})

target_link_libraries(${LUA_T} ${LUA_O} ${CORE_T})
