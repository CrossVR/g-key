#pragma once

typedef struct sqlite3 sqlite3;

class TS3Settings
{
public:
	TS3Settings(void);
	~TS3Settings(void);

	sqlite3* settings;

	int OpenDatabase(char* path);
	bool GetValueFromData(char* data, char* key, char* buf, int bufSize);

private:

};

