#include <iostream>
#include "UnitTest.h"

#include "EPosixClientSocket.h"
#include "EPosixClientSocketPlatform.h"

#include "Contract.h"
#include "Order.h"

using namespace std;

const int PING_DEADLINE = 2; // seconds
const int SLEEP_BETWEEN_PINGS = 30; // seconds

///////////////////////////////////////////////////////////
// member funcs
UnitTest::UnitTest()
: m_pClient(new EPosixClientSocket(this))
, m_state(ST_CONNECT)
, m_sleepDeadline(0)
, m_buySignal(0)
{
}

UnitTest::~UnitTest()
{
}

bool UnitTest::connect(const char *host, unsigned int port, int clientId)
{
	// trying to connect
	cout<<"Connecting to %s:%d clientId:%d\n"<< (!( host && *host) ? "127.0.0.1" : host)<< port<<clientId<<endl;

	bool bRes = m_pClient->eConnect( host, port, clientId);

	if (bRes) {
		cout<<"Connected to %s:%d clientId:%d\n"<<(!( host && *host) ? "127.0.0.1" : host)<<port<<clientId<<endl;
	}
	else
		cout<<"Cannot connect to %s:%d clientId:%d\n"<<(!( host && *host) ? "127.0.0.1" : host)<<port<<clientId<<endl;

	return bRes;
}

void UnitTest::disconnect() const
{
	m_pClient->eDisconnect();

	cout<<"Disconnected\n"<<endl;
}

bool UnitTest::isConnected() const
{
	return m_pClient->isConnected();
}


//////////////////////////////////////////////////////////////////
// methods
void UnitTest::processMessages()
{
	fd_set readSet, writeSet, errorSet;

	struct timeval tval;
	tval.tv_usec = 0;
	tval.tv_sec = 0;
	
	time_t now = time(NULL);

	switch (m_state) {
		case ST_REQ_DATA:
			requestData();
			break;
		case ST_REQ_DATA_ACK:
			break;
		case ST_PLACEORDER:
			placeOrder();
			break;
		case ST_PLACEORDER_ACK:
			break;
		case ST_PLACETAIL:
			placeTail();
			break;
		case ST_PLACETAIL_ACK:
			break;
		case ST_CANCELORDER:
			cancelOrder();
			break;
		case ST_CANCELORDER_ACK:
			break;
		case ST_PING:
			reqCurrentTime();
			break;
		case ST_PING_ACK:
			if( m_sleepDeadline < now) {
				disconnect();
				return;
			}
			break;
		case ST_IDLE:
			if( m_sleepDeadline < now) {
				m_state = ST_PING;
				return;
			}
			break;
	}

	if( m_sleepDeadline > 0) {
		// initialize timeout with m_sleepDeadline - now
		tval.tv_sec = m_sleepDeadline - now;
	}

	if( m_pClient->fd() >= 0 ) {

		FD_ZERO( &readSet);
		errorSet = writeSet = readSet;

		FD_SET( m_pClient->fd(), &readSet);

		if( !m_pClient->isOutBufferEmpty())
			FD_SET( m_pClient->fd(), &writeSet);

		FD_CLR( m_pClient->fd(), &errorSet);

		int ret = select( m_pClient->fd() + 1, &readSet, &writeSet, &errorSet, &tval);

		if( ret == 0) { // timeout
			return;
		}

		if( ret < 0) {	// error
			disconnect();
			return;
		}

		if( m_pClient->fd() < 0)
			return;

		if( FD_ISSET( m_pClient->fd(), &errorSet)) {
			// error on socket
			m_pClient->onError();
		}

		if( m_pClient->fd() < 0)
			return;

		if( FD_ISSET( m_pClient->fd(), &writeSet)) {
			// socket is ready for writing
			m_pClient->onSend();
		}

		if( m_pClient->fd() < 0)
			return;

		if( FD_ISSET( m_pClient->fd(), &readSet)) {
			// socket is ready for reading
			m_pClient->onReceive();
		}
	}
}


void UnitTest::requestData()
{
	Contract contract;
	contract.symbol = "EUR";
	contract.secType = "CASH";
	contract.exchange = "IDEALPRO";
	contract.currency = "USD";

	IBString ticks = "100,101,104,105,106,107,165,221,225,233,236,258,293,294,295,318";

	cout<<"Request FX market data"<<endl;

	m_state = ST_REQ_DATA_ACK;
	m_pClient->reqMktData(m_tickId, contract, ticks, false);
	
	m_tickId++;
}
void UnitTest::placeTail()
{
	
	Contract contract;
	Order order;

	contract.symbol = "EUR";
	contract.secType = "CASH";
	contract.exchange = "IDEALPRO";
	contract.currency = "USD";

	order.action = "SELL";
	order.totalQuantity = 20000;
	order.orderType = "MKT";

	cout<<"Placing Tail Order"<<m_orderId<<order.action.c_str()<<order.totalQuantity<< contract.symbol.c_str()<<order.lmtPrice<<endl;

	m_state = ST_PLACETAIL_ACK;

	m_pClient->placeOrder( m_orderId, contract, order);
	
	m_orderId++;	
}

void UnitTest::reqCurrentTime()
{
	cout<<"Requesting Current Time\n"<<endl;

	// set ping deadline to "now + n seconds"
	m_sleepDeadline = time( NULL) + PING_DEADLINE;

	m_state = ST_PING_ACK;

	m_pClient->reqCurrentTime();
}

void UnitTest::placeOrder()
{
	Contract contract;
	Order order;

	contract.symbol = "EUR";
	contract.secType = "CASH";
	contract.exchange = "IDEALPRO";
	contract.currency = "USD";

	order.action = "BUY";
	order.totalQuantity = 20000;
	order.orderType = "LMT";
	order.lmtPrice = m_lastPrice;

	cout<<"Placing Order"<<m_orderId<<order.action.c_str()<<order.totalQuantity<< contract.symbol.c_str()<<order.lmtPrice<<endl;

	m_state = ST_PLACEORDER_ACK;
	m_pendingOrder = m_orderId;
	m_pClient->placeOrder( m_orderId, contract, order);
	
	m_orderId++;
}

void UnitTest::cancelOrder()
{
	cout<<"Cancelling Order %ld\n"<<m_orderId<<endl;

	m_state = ST_CANCELORDER_ACK;

	m_pClient->cancelOrder( m_orderId);
}

///////////////////////////////////////////////////////////////////
// events
void UnitTest::orderStatus( OrderId orderId, const IBString &status, int filled,
	   int remaining, double avgFillPrice, int permId, int parentId,
	   double lastFillPrice, int clientId, const IBString& whyHeld)

{
	/*
	if( orderId == m_orderId) {
		if( m_state == ST_PLACEORDER_ACK && (status == "PreSubmitted" || status == "Submitted"))
			m_state = ST_CANCELORDER;

		if( m_state == ST_CANCELORDER_ACK && status == "Cancelled")
			m_state = ST_PING;

		cout<<"Order: id=%ld, status=%s\n"<<orderId<<status.c_str()<<endl;
	}
	*/
	if (orderId == m_pendingOrder)
	{
		if( m_state == ST_PLACEORDER_ACK && status == "Filled" )
			m_state = ST_PLACETAIL;
	}
	cout<<"Order:"<<orderId<< " " <<status.c_str()<<endl;
}

void UnitTest::nextValidId( OrderId orderId)
{
	m_orderId = orderId;
	m_tickId = orderId;
	
	m_state = ST_REQ_DATA;
}

void UnitTest::currentTime( long time)
{
	if ( m_state == ST_PING_ACK) {
		time_t t = ( time_t)time;
		struct tm * timeinfo = localtime ( &t);
		cout<<"The current date/time is: %s"<<asctime( timeinfo)<<endl;

		time_t now = ::time(NULL);
		m_sleepDeadline = now + SLEEP_BETWEEN_PINGS;

		m_state = ST_IDLE;
	}
}

void UnitTest::error(const int id, const int errorCode, const IBString errorString)
{
//	printf( "Error id=%d, errorCode=%d, msg=%s\n", id, errorCode, errorString.c_str());
	cout<<"Error: "<<errorCode<<" "<<errorString<<endl;
	if( id == -1 && errorCode == 1100) // if "Connectivity between IB and TWS has been lost"
		disconnect();
}
void UnitTest::tickPrice( TickerId tickerId, TickType field, double price, int canAutoExecute) 
{
	m_buySignal++;
	if (m_buySignal == 10) m_state = ST_PLACEORDER;
	if (field == ASK) m_lastPrice = price;
	cout<<"got tickPrice: "<<tickerId<<" tick type: "<<field<<" price: "<<price<<endl;	
}
void UnitTest::tickSize( TickerId tickerId, TickType field, int size) 
{
	//cout<<"got tickSize: "<<tickerId<<" tick type: "<<field<<" size: "<<size<<endl;		
}
void UnitTest::tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
											 double optPrice, double pvDividend,
											 double gamma, double vega, double theta, double undPrice) 
{
	cout<<"got tickOptionComputation"<<endl;	
}
void UnitTest::tickGeneric(TickerId tickerId, TickType tickType, double value) 
{
	cout<<"got tickGeneric"<<endl;	
}
void UnitTest::tickString(TickerId tickerId, TickType tickType, const IBString& value) 
{
	cout<<"got tickString"<<endl;	
}
void UnitTest::tickEFP(TickerId tickerId, TickType tickType, double basisPoints, const IBString& formattedBasisPoints,
							   double totalDividends, int holdDays, const IBString& futureExpiry, double dividendImpact, double dividendsToExpiry) 
{
	cout<<"got tickEFP"<<endl;	
}
void UnitTest::openOrder( OrderId orderId, const Contract&, const Order&, const OrderState& ostate) {}
void UnitTest::openOrderEnd() {}
void UnitTest::winError( const IBString &str, int lastError) {}
void UnitTest::connectionClosed() {}
void UnitTest::updateAccountValue(const IBString& key, const IBString& val,
										  const IBString& currency, const IBString& accountName) {}
void UnitTest::updatePortfolio(const Contract& contract, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const IBString& accountName){}
void UnitTest::updateAccountTime(const IBString& timeStamp) {}
void UnitTest::accountDownloadEnd(const IBString& accountName) {}
void UnitTest::contractDetails( int reqId, const ContractDetails& contractDetails) {}
void UnitTest::bondContractDetails( int reqId, const ContractDetails& contractDetails) {}
void UnitTest::contractDetailsEnd( int reqId) {}
void UnitTest::execDetails( int reqId, const Contract& contract, const Execution& execution) {}
void UnitTest::execDetailsEnd( int reqId) {}

void UnitTest::updateMktDepth(TickerId id, int position, int operation, int side,
									  double price, int size) {}
void UnitTest::updateMktDepthL2(TickerId id, int position, IBString marketMaker, int operation,
										int side, double price, int size) {}
void UnitTest::updateNewsBulletin(int msgId, int msgType, const IBString& newsMessage, const IBString& originExch) {}
void UnitTest::managedAccounts( const IBString& accountsList) {}
void UnitTest::receiveFA(faDataType pFaDataType, const IBString& cxml) {}
void UnitTest::historicalData(TickerId reqId, const IBString& date, double open, double high,
									  double low, double close, int volume, int barCount, double WAP, int hasGaps) {}
void UnitTest::scannerParameters(const IBString &xml) {}
void UnitTest::scannerData(int reqId, int rank, const ContractDetails &contractDetails,
	   const IBString &distance, const IBString &benchmark, const IBString &projection,
	   const IBString &legsStr) {}
void UnitTest::scannerDataEnd(int reqId) {}
void UnitTest::realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
								   long volume, double wap, int count) {}
void UnitTest::fundamentalData(TickerId reqId, const IBString& data) {}
void UnitTest::deltaNeutralValidation(int reqId, const UnderComp& underComp) {}
void UnitTest::tickSnapshotEnd(int reqId) {}
void UnitTest::marketDataType(TickerId reqId, int marketDataType) {}

int main(int argc, char** argv)
{
	const char* host = argc > 1 ? argv[1] : "";
	unsigned int port = 7496;
	int clientId = 0;

	cout<<"Start of POSIX Socket Client Test\n"<<endl;


		UnitTest client;

		client.connect( host, port, clientId);

		while( client.isConnected()) 
		{
			client.processMessages();
		}
		//client.placeOrder();
		//client.requestData();
		//client.placeCombo();
	
	cout<<"End of POSIX Socket Client Test\n"<<endl;
}


