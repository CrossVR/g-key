#include "../ipc.h"
#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
	IpcMessage msg;
	IpcInit();

	while(true)
	{
		cin >> msg.message;
		if(!IpcWrite(&msg, 1000))
		{
			cout << "Failed to write message";
		}
		cout << "Message written succesfully";
	}
}