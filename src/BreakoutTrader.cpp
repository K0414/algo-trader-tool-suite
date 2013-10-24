
#include "BreakoutTrader.h"

#include "EPosixClientSocket.h"
#include "EPosixClientSocketPlatform.h"

#include "Contract.h"
#include "Order.h"

#include <math.h>

using namespace std;

const int PING_DEADLINE = 2; // seconds
const int SLEEP_BETWEEN_PINGS = 30; // seconds
// const 1 of type double to avoid casting for division operation (i.e. double/int=double, int/int=int)
const double one = 1;

///////////////////////////////////////////////////////////
// member funcs
BreakoutTrader::BreakoutTrader(std::string symbol, std::string type, std::string exch, std::string ccy, std::string time, std::string isdbg)
	: m_pClient(new EPosixClientSocket(this))
	, m_state(ST_CONNECT)
	, m_sleepDeadline(0)
	, m_orderId(0)
	, m_tickId(0)
	,m_curName("")
	,m_pFout(new ofstream)
	,m_pLogger(new ofstream)
	,m_totalBidVol(0)
	,m_numbBidTicks(0)
	,m_numbAskTicks(0)
	,m_totalAskVol(0)
	,m_totalVol(0)
	,m_numbTicks(0)
{
	m_pLogger->open("logs/rolling_log.log", ios::app);
	if (!*(m_pLogger))
	{
		throw "Cannot open rolling.log\n";
	}
	m_curName = rollLog();
	m_symbol = symbol;
	m_secType = type;
	m_exchange = exch;
	m_currency = ccy;
	m_timePeriod = atoi(time.c_str());
	m_isDbg = atoi(isdbg.c_str());
	
}

BreakoutTrader::~BreakoutTrader()
{
	m_pFout->close();
	m_pLogger->close();
}

string BreakoutTrader::rollLog()
{
	time_t now = ::time(NULL);
	struct tm * timeinfo = localtime ( &now);
	
	ostringstream ostr;
	ostr <<"logs/"<<timeinfo->tm_year<<"_"<<timeinfo->tm_mon<<"_"<<timeinfo->tm_mday;
	string tmp = ostr.str();
	
	const char *fName;
	fName = tmp.c_str();
	
	if (tmp != m_curName)
	{
		// close old file
		if (m_pFout->is_open()) m_pFout->close();
		// open new
		m_pFout->open(fName, ios::app);
		if (!*(m_pFout))
		{
			time_t now = ::time(NULL);
			*m_pLogger<<ctime(&now)<<"Cannot open rolling.log\n";
			throw "Cannot open rolling.log\n";
		}
		else
		{
			*m_pFout<<"DAY,"<<"HOUR,"<<"MIN,"<<"SEC,"<<"TOTAL_BID_SIZE,"<<"CLOSE_BID,"<<"BID_ST_DIV,"<<"TOTAL_ASK_SIZE,"<<"CLOSE_ASK,"<<"ASK_ST_DIV,"
					<<"CLOSE_PRICE,"<<"ST_DIV,"<<endl;
		}
		return tmp;
	}
	
	return m_curName;
}

bool BreakoutTrader::connect(const char *host, unsigned int port, int clientId)
{
	
	// trying to connect
	time_t now = ::time(NULL);
	*m_pLogger<<ctime(&now)<<"Connecting to: "<< (!( host && *host) ? "127.0.0.1" : host)<<" port: " << port<<" clientId: "<<clientId<<endl;

	bool bRes = m_pClient->eConnect( host, port, clientId);

	if (bRes) 
	{
		*m_pLogger<<ctime(&now)<<"Connected to: "<< (!( host && *host) ? "127.0.0.1" : host)<<" port: " << port<<" clientId: "<<clientId<<endl;
		m_PeriodStarted = time(NULL);
	}
	else
		*m_pLogger<<ctime(&now)<<"Cannot connect to: "<< (!( host && *host) ? "127.0.0.1" : host)<<" port: " << port<<" clientId: "<<clientId<<endl;
	
	return bRes;
}

void BreakoutTrader::disconnect() const
{
	m_pClient->eDisconnect();

	time_t now = ::time(NULL);
	*m_pLogger<<ctime(&now)<<"Disconnected"<<endl;
}

bool BreakoutTrader::isConnected() const
{
	return m_pClient->isConnected();
}

void BreakoutTrader::processMessages()
{
	fd_set readSet, writeSet, errorSet;

	struct timeval tval;
	tval.tv_usec = 0;
	tval.tv_sec = 0;
	
	time_t now = time(NULL);
	
	switch (m_state) 
	{
		case ST_REQ_DATA:
			reqData();
			break;
		case ST_REQ_DATA_ACK:
			// can't run for more than 24 hours without VPN
			// m_curName = rollLog();
			if ((now - m_PeriodStarted) > m_timePeriod)
			{
				closeTimePeriod();
			}
			break;
		case ST_PING:
			reqCurrentTime();
			break;
		case ST_PING_ACK:
			if( m_sleepDeadline < now) 
			{
				disconnect();
				return;
			}
			break;
		case ST_IDLE:
			if( m_sleepDeadline < now) 
			{
				m_state = ST_PING;
				return;
			}
			break;
	}

	if( m_sleepDeadline > 0) 
	{
		// initialize timeout with m_sleepDeadline - now
		tval.tv_sec = m_sleepDeadline - now;
	}

	if( m_pClient->fd() >= 0 ) 
	{

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

//////////////////////////////////////////////////////////////////
// methods
void BreakoutTrader::reqCurrentTime()
{
	time_t now = ::time(NULL);
	*m_pLogger<<ctime(&now)<<"Requesting Current Time"<<endl;

	// set ping deadline to "now + n seconds"
	m_sleepDeadline = time( NULL) + PING_DEADLINE;

	m_state = ST_PING_ACK;

	m_pClient->reqCurrentTime();
}

void BreakoutTrader::reqData()
{
	Contract contract;
	contract.symbol = m_symbol;
	contract.secType = m_secType;
	contract.exchange = m_exchange;
	contract.currency = m_currency;

	IBString ticks = "100,101,104,105,106,107,165,221,225,233,236,258,293,294,295,318";
	//IBString whatToShow = "TRADES";

	time_t now = ::time(NULL);
	*m_pLogger<<ctime(&now)<<"Request market data for: "<<m_symbol<<m_secType<<m_exchange<<m_currency<<endl;

	m_state = ST_REQ_DATA_ACK;
	m_pClient->reqMktData(m_tickId, contract, ticks, false);
	//m_pClient->reqMktDepth(m_tickId, contract, 50);
	//m_pClient->reqRealTimeBars(m_tickId, contract, 5, whatToShow, true);
	m_tickId++;
}

void BreakoutTrader::closeTimePeriod()
{
	// roll
	// keep
	Sum<double> sumFunc;
	sumFunc = for_each(m_closePriceLogs.begin(),m_closePriceLogs.end(),Sum<double>());
	double sum = sumFunc.GetSum();
	SqrSum<double> sumSqFunc;
	sumSqFunc = for_each(m_closePriceLogs.begin(),m_closePriceLogs.end(),SqrSum<double>());
	double sqSum = sumSqFunc.GetSum();
	// delete
	Sum<double> sumAskFunc;
	sumAskFunc = for_each(m_closeAskPriceLogs.begin(),m_closeAskPriceLogs.end(),Sum<double>());
	double askSum = sumAskFunc.GetSum();
	SqrSum<double> sumSqAskFunc;
	sumSqAskFunc = for_each(m_closeAskPriceLogs.begin(),m_closeAskPriceLogs.end(),SqrSum<double>());
	double askSqSum = sumSqAskFunc.GetSum();
	
	Sum<double> sumBidFunc;
	sumBidFunc = for_each(m_closeBidPriceLogs.begin(),m_closeBidPriceLogs.end(),Sum<double>());
	double bidSum = sumBidFunc.GetSum();
	SqrSum<double> sumSqBidFunc;
	sumSqBidFunc = for_each(m_closeBidPriceLogs.begin(),m_closeBidPriceLogs.end(),SqrSum<double>());
	double bidSqSum = sumSqBidFunc.GetSum();
	// delete
	if (m_isDbg)
	{
		time_t now = ::time(NULL);
		*m_pLogger<<ctime(&now)<<"End Period received - m_numbAskTicks:"<<m_numbAskTicks<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - m_numbBidTicks:"<<m_numbBidTicks<<endl; 
		*m_pLogger<<ctime(&now)<<"End Period received - askSum:"<<scientific<<askSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - askSqSum:"<<scientific<<askSqSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - bidSum:"<<scientific<<bidSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - bidSqSum:"<<scientific<<bidSqSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<((m_numbAskTicks-1))<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<(1/(m_numbAskTicks-1))<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<(1/(m_numbAskTicks-1))*askSqSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<-(1/(m_numbAskTicks*(m_numbAskTicks-1)))*askSum*askSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<(1/(m_numbAskTicks-1))*askSqSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<(1/(m_numbAskTicks-1))*askSqSum-(1/(m_numbAskTicks*(m_numbAskTicks-1)))*askSum*askSum<<endl;
		*m_pLogger<<ctime(&now)<<"End Period received - AskStDiv:"<<scientific<<sqrt((1/(m_numbAskTicks-1))*askSqSum-(1/(m_numbAskTicks*(m_numbAskTicks-1)))*askSum*askSum)<<endl;
	}
	
	// delete
	m_AskStDiv =sqrt((one/(m_numbAskTicks-1))*askSqSum-(one/(m_numbAskTicks*(m_numbAskTicks-1)))*askSum*askSum);
	m_BidStDiv =sqrt((one/(m_numbBidTicks-1))*bidSqSum-(one/(m_numbBidTicks*(m_numbBidTicks-1)))*bidSum*bidSum);
	// keep
	m_StDiv =sqrt((one/(m_numbTicks-1))*sqSum-(one/(m_numbTicks*(m_numbTicks-1)))*sum*sum);
	
	// log
	time_t now = ::time(NULL);
	struct tm * timeinfo = localtime ( &now);
	// "DAY,"<<"HOUR,"<<"MIN,"<<"SEC,"<<"TOTAL_BID_SIZE,"<<"CLOSE_BID,"<<"BID_ST_DIV,"<<"TOTAL_ASK_SIZE,"<<"CLOSE_ASK,"<<"ASK_ST_DIV,"
	*m_pFout<<timeinfo->tm_mday<<","<<timeinfo->tm_hour<<","<<timeinfo->tm_min<<","<<timeinfo->tm_sec<<","
			<<m_totalBidVol<<","<<m_lastBidPrice<<","<<scientific<<m_BidStDiv<<","<<m_totalAskVol<<","
			<<fixed<<m_lastAskPrice<<","<<scientific<<m_AskStDiv<<","<<m_lastPrice<<","<<m_StDiv<<endl;
	// clear
	// delete
	m_totalBidVol = 0;
	m_totalAskVol = 0;
	m_AskStDiv = 0;
	m_BidStDiv = 0;
	m_numbBidTicks = 0;
	m_numbAskTicks = 0;
	m_PeriodStarted = ::time(NULL);
	m_closeBidPriceLogs.clear();
	m_closeAskPriceLogs.clear();
	// keep
	m_totalVol = 0;
	m_StDiv = 0;
	m_numbTicks = 0;
	m_PeriodStarted = ::time(NULL);
	m_closePriceLogs.clear();
}


///////////////////////////////////////////////////////////////////
// events
void BreakoutTrader::orderStatus( OrderId orderId, const IBString &status, int filled,
	   int remaining, double avgFillPrice, int permId, int parentId,
	   double lastFillPrice, int clientId, const IBString& whyHeld) 
{
}

void BreakoutTrader::nextValidId( OrderId orderId)
{
	m_orderId = orderId;
	m_tickId = orderId;

	m_state = ST_REQ_DATA;
}

void BreakoutTrader::currentTime( long time)
{
	if ( m_state == ST_PING_ACK) {
		time_t t = ( time_t)time;
		struct tm * timeinfo = localtime ( &t);
		*m_pLogger<<"The current date/time is: "<<asctime( timeinfo)<<endl;

		time_t now = ::time(NULL);
		m_sleepDeadline = now + SLEEP_BETWEEN_PINGS;

		m_state = ST_IDLE;
	}
}

void BreakoutTrader::error(const int id, const int errorCode, const IBString errorString)
{
	time_t now = ::time(NULL);
	*m_pLogger<<ctime(&now)<<"Error: "<<errorCode<<" "<<errorString<<endl;
	if( id == -1 && errorCode == 1100) // if "Connectivity between IB and TWS has been lost"
		disconnect();
}

void BreakoutTrader::tickPrice( TickerId tickerId, TickType field, double price, int canAutoExecute) 
{
	// keep
	if ((field == ASK) || (field == BID)) 
	{
		m_numbTicks += 1;
		// StDiv calculus
		if (m_numbTicks > 1) 
		{
			m_prevPrice = m_lastPrice;
			m_lastPrice = price;
			m_closePriceLogs.push_back(log(m_lastPrice/m_prevPrice));
		}
		else
		{
			m_lastPrice = price;
		}
		//if (m_isDbg)
		//{
		//	time_t now = ::time(NULL);
		//	*m_pLogger<<ctime(&now)<<endl;
		//}
		
	}
	// delete
	if (field == ASK) 
	{
		m_numbAskTicks += 1;
		// StDiv calculus
		if (m_numbAskTicks > 1) 
		{
			m_prevAskPrice = m_lastAskPrice;
			m_lastAskPrice = price;
			m_closeAskPriceLogs.push_back(log(m_lastAskPrice/m_prevAskPrice));
		}
		else
		{
			m_lastAskPrice = price;
		}
		//if (m_isDbg)
		//{
		//	time_t now = ::time(NULL);
		//	*m_pLogger<<ctime(&now)<<"TickPrice received - Type: ASK Price: "<<fixed<<price<<endl;
		//}
		
	}
	else if (field == BID)
	{
		m_numbBidTicks += 1;
		// StDiv calculus
		if (m_numbBidTicks > 1) 
		{
			m_prevBidPrice = m_lastBidPrice;
			m_lastBidPrice = price;
			m_closeBidPriceLogs.push_back(log(m_lastBidPrice/m_prevBidPrice));
		}
		else
		{
			m_lastBidPrice = price;
		}
		//if (m_isDbg)
		//{
		//	time_t now = ::time(NULL);
		//	*m_pLogger<<ctime(&now)<<"TickPrice received - Type: BID Price: "<<fixed<<price<<endl;
		//}
	}
	else
	{
		// error
	}
}
void BreakoutTrader::tickSize( TickerId tickerId, TickType field, int size) 
{
	if (field == BID_SIZE) 
	{
		m_totalBidVol += size;
	}
	else if (field == ASK_SIZE)
	{
		m_totalAskVol += size;
	}
	
	if (m_isDbg)
	{
		time_t now = ::time(NULL);
		*m_pLogger<<ctime(&now)<<"TickSize received - Type:"<<field<<" Size: "<<size<<endl;
	}
}

void BreakoutTrader::tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
											 double optPrice, double pvDividend,
											 double gamma, double vega, double theta, double undPrice) {}
void BreakoutTrader::tickGeneric(TickerId tickerId, TickType tickType, double value) {}
void BreakoutTrader::tickString(TickerId tickerId, TickType tickType, const IBString& value) {}
void BreakoutTrader::tickEFP(TickerId tickerId, TickType tickType, double basisPoints, const IBString& formattedBasisPoints,
							   double totalDividends, int holdDays, const IBString& futureExpiry, double dividendImpact, double dividendsToExpiry) {}
void BreakoutTrader::openOrder( OrderId orderId, const Contract&, const Order&, const OrderState& ostate) {}
void BreakoutTrader::openOrderEnd() {}
void BreakoutTrader::winError( const IBString &str, int lastError) {}
void BreakoutTrader::connectionClosed() {}
void BreakoutTrader::updateAccountValue(const IBString& key, const IBString& val,
										  const IBString& currency, const IBString& accountName) {}
void BreakoutTrader::updatePortfolio(const Contract& contract, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const IBString& accountName){}
void BreakoutTrader::updateAccountTime(const IBString& timeStamp) {}
void BreakoutTrader::accountDownloadEnd(const IBString& accountName) {}
void BreakoutTrader::contractDetails( int reqId, const ContractDetails& contractDetails) {}
void BreakoutTrader::bondContractDetails( int reqId, const ContractDetails& contractDetails) {}
void BreakoutTrader::contractDetailsEnd( int reqId) {}
void BreakoutTrader::execDetails( int reqId, const Contract& contract, const Execution& execution) {}
void BreakoutTrader::execDetailsEnd( int reqId) {}

void BreakoutTrader::updateMktDepth(TickerId id, int position, int operation, int side,
									  double price, int size) 
{
	// could be useful
	//cout<<"position: "<<position<<"operation: "<<operation<<"side: "<<side<<"price: "<<price<<"size: "<<size<<endl;
}
void BreakoutTrader::updateMktDepthL2(TickerId id, int position, IBString marketMaker, int operation,
										int side, double price, int size) 
{
	//cout<<"position: "<<position<<"marketMaker: "<<marketMaker<<"side: "<<side<<"price: "<<price<<"size: "<<size<<endl;
}
void BreakoutTrader::updateNewsBulletin(int msgId, int msgType, const IBString& newsMessage, const IBString& originExch) {}
void BreakoutTrader::managedAccounts( const IBString& accountsList) {}
void BreakoutTrader::receiveFA(faDataType pFaDataType, const IBString& cxml) {}
void BreakoutTrader::historicalData(TickerId reqId, const IBString& date, double open, double high,
									  double low, double close, int volume, int barCount, double WAP, int hasGaps) {}
void BreakoutTrader::scannerParameters(const IBString &xml) {}
void BreakoutTrader::scannerData(int reqId, int rank, const ContractDetails &contractDetails,
	   const IBString &distance, const IBString &benchmark, const IBString &projection,
	   const IBString &legsStr) {}
void BreakoutTrader::scannerDataEnd(int reqId) {}
void BreakoutTrader::realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
								   long volume, double wap, int count) 
{
	//cout<<"time: "<<time<<"open: "<<open<<"high: "<<high<<"volume: "<<volume<<"count: "<<count<<endl;
}
void BreakoutTrader::fundamentalData(TickerId reqId, const IBString& data) {}
void BreakoutTrader::deltaNeutralValidation(int reqId, const UnderComp& underComp) {}
void BreakoutTrader::tickSnapshotEnd(int reqId) {}
void BreakoutTrader::marketDataType(TickerId reqId, int marketDataType) {}

