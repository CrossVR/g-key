#include "../ipc.h"
#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
	IpcMessage msg;
	IpcInit();

	while(true)
	{
		cout << "Message: ";
		cin >> msg.message;
		if(!IpcWrite(&msg, 1000))
		{
			cout << "Failed to write message" << endl;
		}
		cout << "Message written succesfully" << endl;
	}
}