install(
	FILES
	enable-debugview.reg
	enable-full-dumps.reg
	enable-mini-dumps.reg
	enable-log-files.reg
	OpenKneeboard.wprp
	OpenKneeboard-with-d3d11.wprp
	DESTINATION scripts/
)

add_executable(args-to-lines args-to-lines.cpp)
add_executable("true" "true.cpp")
