SRC=\
src/desktop_driver.c


all : clean win

win : desktop_driver.dll dlllib



desktop_driver.dll : $(SRC)
	gcc -g -Wall -D_GUI --shared -o $@ $^  -lGdi32

clean :
	rm -rf desktop_driver.dll 
dlllib :
	dlltool -d desktop_driver.def -l desktop_driver.lib
