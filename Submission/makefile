sv_node: sv_node.o read_write.o utilities.o
	@g++ -o sv_node sv_node.o read_write.o utilities.o -g -lnsl -lsocket -lresolv -Wall -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib -lcrypto

sv_node.o: sv_node.cpp
	@g++ -c sv_node.cpp -Wall -g -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib

read_write.o: read_write.cpp
	@g++ -c read_write.cpp -Wall -g -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib

utilities.o: utilities.cpp
	@g++ -c utilities.cpp -Wall -g -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib

clean:
	@rm -rf *o sv_node sv_node.o read_write read_write.o utilities utilities.o