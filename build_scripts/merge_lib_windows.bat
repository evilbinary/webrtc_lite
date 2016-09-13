mkdir out_win\lib
del /Q out_win\lib\*
for /r out_win\Release %%i in (*.lib) do copy %%i out_win\lib\
call "%VS140COMNTOOLS%\vsvars32.bat"
LIB out_win\lib\*.lib /out:out_win\lib\libwebrtc.lib.temp
del /Q out_win\lib\*.lib
move out_win\lib\libwebrtc.lib.temp out_win\lib\libwebrtc.lib