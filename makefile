CXX=g++
CXXFLAGS=-DIB_USE_STD_STRING -Wall -Wno-switch
BASE_SRC_DIR=src
RELEASE_DIR=bin
SHARED_DIR=shared
TEST_DIR=tests
BOOST_DIR=../boost_1_49_0
INCLUDES=-I${SHARED_DIR} -I${BASE_SRC_DIR} -I${BOOST_DIR}
TARGET=BreakoutTrader


$(TARGET):
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(RELEASE_DIR)/EClientSocketBase.o -c $(SHARED_DIR)/EClientSocketBase.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(RELEASE_DIR)/EPosixClientSocket.o -c $(SHARED_DIR)/EPosixClientSocket.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(RELEASE_DIR)/BreakoutTrader.o -c $(BASE_SRC_DIR)/BreakoutTrader.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(RELEASE_DIR)/main.o -c $(BASE_SRC_DIR)/main.cpp 
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(RELEASE_DIR)/$(TARGET) $(RELEASE_DIR)/EClientSocketBase.o $(RELEASE_DIR)/EPosixClientSocket.o $(RELEASE_DIR)/BreakoutTrader.o $(RELEASE_DIR)/main.o

clean:
	rm -f $(RELEASE_DIR)/$(TARGET) $(RELEASE_DIR)/*.o
	
unittest:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TEST_DIR)/EClientSocketBase.o -c $(SHARED_DIR)/EClientSocketBase.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TEST_DIR)/EPosixClientSocket.o -c $(SHARED_DIR)/EPosixClientSocket.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TEST_DIR)/UnitTest.o -c $(TEST_DIR)/UnitTest.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TEST_DIR)/UnitTest $(TEST_DIR)/EClientSocketBase.o $(TEST_DIR)/EPosixClientSocket.o $(TEST_DIR)/UnitTest.o 