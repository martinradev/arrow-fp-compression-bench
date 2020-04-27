all: search_space network_comparison

search_space: search_space.cpp
	g++ search_space.cpp -O3 -o search_space

network_comparison: network_comparison.cpp
	g++ network_comparison.cpp -march=sandybridge -O3 -o network_comparison

clean:
	rm search_space network_comparison
