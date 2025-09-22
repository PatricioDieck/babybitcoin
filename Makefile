all:
	g++ -std=c++17 -I/opt/homebrew/opt/openssl@3/include src/common.cpp src/crypto.cpp -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -o babybitcoin

clean:
	rm -f babybitcoin

install-deps:
	brew install openssl