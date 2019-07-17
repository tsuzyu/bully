all:
	g++ main.cpp -o main --std=c++11 -lpthread
clean:
	rm main.o
