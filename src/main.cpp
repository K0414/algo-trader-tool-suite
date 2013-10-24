#include <iostream>
#include "BreakoutTrader.h"

using namespace std;
const unsigned MAX_ATTEMPTS = 50;
const unsigned SLEEP_TIME = 10;

// usage: symbol, type, exchange, currency, time period (number of seconds)
// "EUR" "CASH" "IDEALPRO" "USD" 300 1
int main(int argc, char** argv)
{
	try
	{
		cout<<"Start of BreakoutTrader with "<<argc<<" params"<<endl;
		if (argc < 6) 
		{
			throw "Usage: symbol, type, exchange, currency, time period, is debug.\n For example: EUR CASH IDEALPRO USD 300 1";
		}
		
		const char* host = "";
		unsigned int port = 4001;
		int clientId = 0;
	
		unsigned attempt = 0;
		cout<<"Create BreakoutTrader\n"<<endl;
		string symbol = argv[1];
		string type = argv[2];
		string exch = argv[3];
		string ccy = argv[4];
		string period = argv[5];
		string isDbg = argv[6];
		BreakoutTrader client(symbol,type,exch,ccy,period, isDbg);
		for (;;) 
		{
			++attempt;
			cout<<"Attempt "<<attempt<<" of "<<MAX_ATTEMPTS<<"\n"<<endl;
	
			client.connect( host, port, clientId);
	
			while( client.isConnected()) {
				client.processMessages();
			}
	
			if( attempt >= MAX_ATTEMPTS) {
				break;
			}
	
			cout<<"Sleeping before next attempt for "<<SLEEP_TIME<<"\n"<<endl;
			sleep( SLEEP_TIME);
		}
	
		cout<<"End of BreakoutTrader\n"<<endl;
	}
    catch(const char* e)
    {
        cout << "Exception: " << endl;
        cout << e << endl;
        return 1;
    }

	return 0;
}

