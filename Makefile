# It's a small project, so we will hardcode most of the things

ALL_OBJ=main.o

parquet_test: $(ALL_OBJ)
	g++ $(ALL_OBJ) -O2 -std=c++14 -larrow -lparquet -o parquet_test


main.o: main.cpp
	g++ main.cpp -O2 -c -std=c++14 -o main.o

clean:
	rm *.o
	rm parquet_test

