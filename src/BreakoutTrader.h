#ifndef BreakoutTrader_h__INCLUDED
#define BreakoutTrader_h__INCLUDED

#include "EWrapper.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <limits>
#include <boost/shared_ptr.hpp>

using namespace std;

class EPosixClientSocket;

enum State 
{
	ST_CONNECT,
	ST_REQ_DATA,
	ST_REQ_DATA_ACK,
	ST_PING,
	ST_PING_ACK,
	ST_IDLE
};
// sum functor
template <typename T> 
class Sum 
{ 
	T m_Sum;
  public:
	Sum() 
	{ 
		m_Sum = 0; 
	}
	T GetSum() const
	{
		return m_Sum;
	}
	
	void operator() (const T& el) 
	{ 
		m_Sum += el;
	} 
};
// sum of squares functor
template <typename T> 
class SqrSum 
{ 
	T m_Sum;
  public:
	SqrSum() 
	{ 
		m_Sum = 0; 
	}
	T GetSum() const
	{
		return m_Sum;
	}
	void operator() (const T& el) 
	{ 
		m_Sum += (el*el);
	} 
};

class BreakoutTrader : public EWrapper
{
public:

	BreakoutTrader(std::string symbol, std::string type, std::string exch, std::string ccy, std::string time, std::string isdbg);
	~BreakoutTrader();

	void processMessages();

	bool connect(const char * host, unsigned int port, int clientId = 0);
	void disconnect() const;
	bool isConnected() const;

private:
	string rollLog();
	void reqCurrentTime();
	void reqData();
	void closeTimePeriod();

public:
	// events
	void tickPrice(TickerId tickerId, TickType field, double price, int canAutoExecute);
	void tickSize(TickerId tickerId, TickType field, int size);
	void tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
		double optPrice, double pvDividend, double gamma, double vega, double theta, double undPrice);
	void tickGeneric(TickerId tickerId, TickType tickType, double value);
	void tickString(TickerId tickerId, TickType tickType, const IBString& value);
	void tickEFP(TickerId tickerId, TickType tickType, double basisPoints, const IBString& formattedBasisPoints,
		double totalDividends, int holdDays, const IBString& futureExpiry, double dividendImpact, double dividendsToExpiry);
	void orderStatus(OrderId orderId, const IBString &status, int filled,
		int remaining, double avgFillPrice, int permId, int parentId,
		double lastFillPrice, int clientId, const IBString& whyHeld);
	void openOrder(OrderId orderId, const Contract&, const Order&, const OrderState&);
	void openOrderEnd();
	void winError(const IBString &str, int lastError);
	void connectionClosed();
	void updateAccountValue(const IBString& key, const IBString& val,
		const IBString& currency, const IBString& accountName);
	void updatePortfolio(const Contract& contract, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const IBString& accountName);
	void updateAccountTime(const IBString& timeStamp);
	void accountDownloadEnd(const IBString& accountName);
	void nextValidId(OrderId orderId);
	void contractDetails(int reqId, const ContractDetails& contractDetails);
	void bondContractDetails(int reqId, const ContractDetails& contractDetails);
	void contractDetailsEnd(int reqId);
	void execDetails(int reqId, const Contract& contract, const Execution& execution);
	void execDetailsEnd(int reqId);
	void error(const int id, const int errorCode, const IBString errorString);
	void updateMktDepth(TickerId id, int position, int operation, int side,
		double price, int size);
	void updateMktDepthL2(TickerId id, int position, IBString marketMaker, int operation,
		int side, double price, int size);
	void updateNewsBulletin(int msgId, int msgType, const IBString& newsMessage, const IBString& originExch);
	void managedAccounts(const IBString& accountsList);
	void receiveFA(faDataType pFaDataType, const IBString& cxml);
	void historicalData(TickerId reqId, const IBString& date, double open, double high,
		double low, double close, int volume, int barCount, double WAP, int hasGaps);
	void scannerParameters(const IBString &xml);
	void scannerData(int reqId, int rank, const ContractDetails &contractDetails,
		const IBString &distance, const IBString &benchmark, const IBString &projection,
		const IBString &legsStr);
	void scannerDataEnd(int reqId);
	void realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
		long volume, double wap, int count);
	void currentTime(long time);
	void fundamentalData(TickerId reqId, const IBString& data);
	void deltaNeutralValidation(int reqId, const UnderComp& underComp);
	void tickSnapshotEnd(int reqId);
	void marketDataType(TickerId reqId, int marketDataType);

private:

	boost::shared_ptr<EPosixClientSocket> m_pClient;
	State m_state;
	time_t m_sleepDeadline;
	int m_timePeriod;
	time_t m_PeriodStarted;

	OrderId m_orderId;
	OrderId m_tickId;
	
	string m_symbol;
	string m_secType;
	string m_exchange;
	string m_currency;
	
	string m_curName;
	boost::shared_ptr<ofstream> m_pFout;
	boost::shared_ptr<ofstream> m_pLogger;
	
	bool m_isDbg;
	// delete
	double m_lastBidPrice;
	double m_prevBidPrice;
	double m_lastAskPrice;
	double m_prevAskPrice;
	unsigned int m_totalBidVol;
	int m_numbBidTicks;
	int m_numbAskTicks;
	unsigned int m_totalAskVol;
	double m_AskStDiv;
	double m_BidStDiv;
	vector<double> m_closeAskPriceLogs;
	vector<double> m_closeBidPriceLogs;
	// keep
	double m_lastPrice;
	double m_prevPrice;
	unsigned int m_totalVol;
	int m_numbTicks;
	double m_StDiv;
	vector<double> m_closePriceLogs;
	//
};

#endif

