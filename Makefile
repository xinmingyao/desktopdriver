SRC=\
src/desktop_driver.c

TEST=src/desktop_driver.c\
src/test.c

all : clean win

win : desktop_driver.dll dlllib


test.exe: $(TEST)
	gcc -g -Wall  -o $@ $^  -lGdi32
	
desktop_driver.dll : $(SRC)
	gcc -g -Wall -D_GUI --shared -o $@ $^  -lGdi32

clean :
	rm -rf desktop_driver.dll 
dlllib :
	dlltool -d desktop_driver.def -l desktop_driver.lib
