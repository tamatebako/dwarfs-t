2026-01-29T17:44:04.0918383Z [44/358] Building CXX object CMakeFiles\dwarfs_common.dir\src\logger.cpp.obj
2026-01-29T17:44:04.1271132Z FAILED: [code=2] CMakeFiles/dwarfs_common.dir/src/logger.cpp.obj
2026-01-29T17:44:04.1649211Z C:\Strawberry\c\bin\ccache.exe D:\a\dwarfs\vs-install\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\cl.exe  /nologo /TP -DBOOST_CHRONO_DYN_LINK -DBOOST_CHRONO_NO_LIB -DBOOST_CONTAINER_DYN_LINK -DBOOST_CONTAINER_NO_LIB -DBOOST_CONTEXT_DYN_LINK="" -DBOOST_CONTEXT_EXPORT=EXPORT -DBOOST_CONTEXT_NO_LIB="" -DBOOST_DATE_TIME_DYN_LINK -DBOOST_DATE_TIME_NO_LIB -DBOOST_FILESYSTEM_DYN_LINK=1 -DBOOST_FILESYSTEM_NO_LIB -DBOOST_PROCESS_DYN_LINK -DBOOST_PROGRAM_OPTIONS_DYN_LINK -DBOOST_PROGRAM_OPTIONS_NO_LIB -DBROTLI_SHARED_COMPILATION -DDWARFS_COMPILER_ID="\"MSVC 19.42.34444.0\"" -DDWARFS_HAVE_FLATBUFFERS -DDWARFS_HAVE_FLATBUFFERS=1 -DDWARFS_HAVE_LEGACY_THRIFT=1 -DDWARFS_SYSTEM_ID="\"Windows [AMD64]\"" -DFMT_SHARED -DNOMINMAX -DWINVER=0x0601 -DXXH_EXPORT -D_WIN32_WINNT=0x0601 -ID:\a\dwarfs\dwarfs\build-x64-windows-static-production\include -ID:\a\dwarfs\dwarfs\include -ID:\a\dwarfs\dwarfs\build-x64-windows-static-production\include\dwarfs\gen-flatbuffers\..\.. -ID:\a\dwarfs\dwarfs\ricepp\include -external:ID:\a\dwarfs\dwarfs\fsst -external:ID:\a\dwarfs\dwarfs\build-x64-windows-static-production\vcpkg_installed\x64-windows\include -external:W0 /DWIN32 /D_WINDOWS /EHsc /O2 /Ob2 /DNDEBUG -std:c++20 -MT /Zc:__cplusplus /utf-8 /wd4267 /wd4244 /wd5219 /W4 /w14254 /w14263 /w14265 /w14287 /we4289 /w14905 /w14906 /w14928 /wd4456 /WX /showIncludes /FoCMakeFiles\dwarfs_common.dir\src\logger.cpp.obj /FdCMakeFiles\dwarfs_common.dir\dwarfs_common.pdb /FS -c D:\a\dwarfs\dwarfs\src\logger.cpp
2026-01-29T17:44:04.1990499Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(55): error C2143: syntax error: missing '}' before 'constant'
2026-01-29T17:44:04.2322968Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(55): error C2059: syntax error: 'constant'
2026-01-29T17:44:04.2743271Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(61): error C2143: syntax error: missing ';' before '}'
2026-01-29T17:44:04.3297817Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(61): error C2238: unexpected token(s) preceding ';'
2026-01-29T17:44:04.3635965Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(63): error C2065: 'level_type': undeclared identifier
2026-01-29T17:44:04.3962068Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(63): error C2146: syntax error: missing ')' before identifier 'level'
2026-01-29T17:44:04.4297069Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(65): error C2588: '::~logger': illegal global destructor
2026-01-29T17:44:04.4618464Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(65): error C2575: 'logger': only member functions and bases can be virtual
2026-01-29T17:44:04.4951474Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(65): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:04.5318072Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(65): error C2610: 'int dwarfs::logger(void)': is not a special member function or comparison operator which can be defaulted
2026-01-29T17:44:04.5655277Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(68): error C2182: 'write': this use of 'void' is not valid
2026-01-29T17:44:04.5992780Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(68): error C2433: 'write': 'virtual' not permitted on data declarations
2026-01-29T17:44:04.6319405Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(68): error C2065: 'level_type': undeclared identifier
2026-01-29T17:44:04.6689372Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(68): error C2146: syntax error: missing ')' before identifier 'level'
2026-01-29T17:44:04.7029076Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): error C2433: 'level_type': 'virtual' not permitted on data declarations
2026-01-29T17:44:04.7354731Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:04.7693764Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): error C2146: syntax error: missing ';' before identifier 'threshold'
2026-01-29T17:44:04.8073672Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): error C2270: 'threshold': modifiers not allowed on nonmember functions
2026-01-29T17:44:04.8407310Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:04.8927530Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): error C2059: syntax error: 'constant'
2026-01-29T17:44:04.9582304Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(71): error C2270: 'policy_name': modifiers not allowed on nonmember functions
2026-01-29T17:44:05.0250956Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(71): error C2065: 'policy_name_': undeclared identifier
2026-01-29T17:44:05.0942813Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(73): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:05.1921351Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(73): error C2370: 'dwarfs::level_type': redefinition; different storage class
2026-01-29T17:44:05.2460768Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(69): note: see declaration of 'dwarfs::level_type'
2026-01-29T17:44:05.2796802Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(73): error C2146: syntax error: missing ';' before identifier 'parse_level'
2026-01-29T17:44:05.2800234Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(73): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:05.3137008Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(74): error C2146: syntax error: missing ')' before identifier 'level'
2026-01-29T17:44:05.3483482Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(78): error C2059: syntax error: 'protected'
2026-01-29T17:44:05.3815536Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(82): error C3861: 'policy_name_': identifier not found
2026-01-29T17:44:05.4152267Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(82): error C2065: 'policy_name_': undeclared identifier
2026-01-29T17:44:05.4652394Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(85): error C2059: syntax error: 'private'
2026-01-29T17:44:05.4980428Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(89): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:05.5061478Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(89): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:05.5377763Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(89): error C2805: binary 'operator <<' has too few parameters
2026-01-29T17:44:05.5677438Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(90): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:05.5757951Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(90): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:05.6034527Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(90): error C2805: binary 'operator >>' has too few parameters
2026-01-29T17:44:05.6356077Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(93): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:05.6420885Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(93): error C3646: 'threshold': unknown override specifier
2026-01-29T17:44:05.7017396Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(93): error C2059: syntax error: '{'
2026-01-29T17:44:05.7680170Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(93): error C2334: unexpected token(s) preceding '{'; skipping apparent function body
2026-01-29T17:44:05.8360432Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(97): error C2504: 'logger': base class undefined
2026-01-29T17:44:05.9057790Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(101): error C2065: 'terminal': undeclared identifier
2026-01-29T17:44:05.9715158Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(101): error C2059: syntax error: 'const'
2026-01-29T17:44:06.0659072Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(104): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:06.1420504Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(106): error C3646: 'threshold': unknown override specifier
2026-01-29T17:44:06.2058881Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(106): error C2059: syntax error: '('
2026-01-29T17:44:06.2732968Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(106): error C2238: unexpected token(s) preceding ';'
2026-01-29T17:44:06.3407066Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(108): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:06.4063267Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(119): error C3646: 'log_threshold': unknown override specifier
2026-01-29T17:44:06.4734340Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(119): error C2059: syntax error: '('
2026-01-29T17:44:06.5520295Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(119): error C2334: unexpected token(s) preceding '{'; skipping apparent function body
2026-01-29T17:44:06.6016288Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(120): error C2143: syntax error: missing ';' before 'const'
2026-01-29T17:44:06.6580653Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(120): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:06.7161593Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(120): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:06.7658651Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(125): error C2065: 'level_type': undeclared identifier
2026-01-29T17:44:06.8418013Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(125): error C2923: 'std::atomic': 'level_type' is not a valid template type argument for parameter '_Ty'
2026-01-29T17:44:06.9087243Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(125): note: see declaration of 'level_type'
2026-01-29T17:44:06.9838459Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(125): error C2955: 'std::atomic': use of class template requires template argument list
2026-01-29T17:44:07.1073223Z D:\a\dwarfs\vs-install\VC\Tools\MSVC\14.42.34433\include\atomic(2113): note: see declaration of 'std::atomic'
2026-01-29T17:44:07.2076548Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(129): error C2065: 'terminal': undeclared identifier
2026-01-29T17:44:07.2852408Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(129): error C2059: syntax error: 'const'
2026-01-29T17:44:07.3779157Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(104): error C3668: 'stream_logger::write': method with override specifier 'override' did not override any base class methods
2026-01-29T17:44:07.4531046Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(132): error C2504: 'logger': base class undefined
2026-01-29T17:44:07.5211401Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(136): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:07.5868521Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(137): error C3646: 'threshold': unknown override specifier
2026-01-29T17:44:07.6522873Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(137): error C2059: syntax error: '('
2026-01-29T17:44:07.7175813Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(137): error C2334: unexpected token(s) preceding '{'; skipping apparent function body
2026-01-29T17:44:07.7841725Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(136): error C3668: 'null_logger::write': method with override specifier 'override' did not override any base class methods
2026-01-29T17:44:07.8518184Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(142): error C2061: syntax error: identifier 'logger'
2026-01-29T17:44:07.9339313Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(159): error C2143: syntax error: missing ';' before '&'
2026-01-29T17:44:08.0124365Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(159): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:08.0775133Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(159): error C2238: unexpected token(s) preceding ';'
2026-01-29T17:44:08.1445393Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(161): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:08.2262060Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(161): error C2143: syntax error: missing ';' before 'const'
2026-01-29T17:44:08.2755494Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(161): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:08.3338665Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(161): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:08.3893461Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(162): error C2143: syntax error: missing ';' before 'const'
2026-01-29T17:44:08.4397088Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(162): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:08.4992601Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(162): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int
2026-01-29T17:44:08.5543564Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(143): error C2065: 'lgr': undeclared identifier
2026-01-29T17:44:08.6021693Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(144): error C2065: 'level': undeclared identifier
2026-01-29T17:44:08.6260553Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(145): error C2065: 'loc': undeclared identifier
2026-01-29T17:44:08.6519110Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(143): error C2614: 'level_log_entry': illegal member initialization: 'lgr_' is not a base or member
2026-01-29T17:44:08.6827523Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(149): error C2065: 'lgr_': undeclared identifier
2026-01-29T17:44:08.6854039Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(167): error C2061: syntax error: identifier 'logger'
2026-01-29T17:44:08.7190486Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(192): error C2061: syntax error: identifier 'logger'
2026-01-29T17:44:08.7530261Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(193): error C2061: syntax error: identifier 'logger'
2026-01-29T17:44:08.7652239Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(193): error C2535: 'no_log_entry::no_log_entry(void)': member function already defined or declared
2026-01-29T17:44:08.7888411Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(192): note: see declaration of 'no_log_entry::no_log_entry'
2026-01-29T17:44:08.7928319Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(221): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:08.7980870Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(221): note: the template instantiation context (the oldest one first) is
2026-01-29T17:44:08.8340424Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(213): note: while compiling class template 'MinimumLogLevelPolicy'
2026-01-29T17:44:08.8403153Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(221): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:08.8440946Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(222): error C3861: 'level': identifier not found
2026-01-29T17:44:08.8889264Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(222): error C2065: 'level': undeclared identifier
2026-01-29T17:44:08.9397500Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(229): error C2061: syntax error: identifier 'logger'
2026-01-29T17:44:08.9792520Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(229): note: the template instantiation context (the oldest one first) is
2026-01-29T17:44:09.0209622Z D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(227): note: while compiling class template 'log_proxy'
2026-01-29T17:44:09.0785029Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(233): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:09.1218435Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(233): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:09.1765926Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(237): error C2653: 'logger': is not a class or namespace name
2026-01-29T17:44:09.2194893Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(237): error C2061: syntax error: identifier 'level_type'
2026-01-29T17:44:09.2746043Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(241): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.2749737Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(245): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.3181323Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(250): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.3730117Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(255): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.4219902Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(260): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.4714082Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(265): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.5088896Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(270): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.5527674Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(275): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.6023994Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(280): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.6427075Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(285): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.7193864Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(290): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.7863993Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(295): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.8361078Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(300): error C2061: syntax error: identifier 'source_location'
2026-01-29T17:44:09.8856092Z ##[error]D:\a\dwarfs\dwarfs\include\dwarfs/logger.h(300): fatal error C1003: error count exceeds 100; stopping compilation
2026-01-29T17:44:09.9191365Z [45/358] Building CXX object CMakeFiles\dwarfs_common.dir\src\os_access_generic.cpp.obj
2026-01-29T17:44:09.9445175Z [46/358] Building CXX object CMakeFiles\dwarfs_common.dir\src\pcm_sample_transformer.cpp.obj
2026-01-29T17:44:10.3147315Z [47/358] Building CXX object CMakeFiles\dwarfs_common.dir\src\file_util.cpp.obj
2026-01-29T17:44:13.1275832Z [48/358] Building CXX object CMakeFiles\dwarfs_common.dir\src\performance_monitor.cpp.obj
2026-01-29T17:44:16.2760965Z [49/358] Building CXX object CMakeFiles\dwarfs_common.dir\src\option_map.cpp.obj
2026-01-29T17:44:16.2765024Z ninja: build stopped: subcommand failed.
