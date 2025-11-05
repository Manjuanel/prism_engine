CFLAGS = -std=c++20 -O2
LDFLAGS = -lglfw -ldl -lvulkan -lpthread -lX11 -lXxf86vm -lXrandr -lXi

compile: prism.cpp
	g++ $(CFLAGS) -o prism prism.cpp $(LDFLAGS)

.PHONY: test clean

test: compile
	./prism

clean:
	rm -f prism
