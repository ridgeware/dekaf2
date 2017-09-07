#include "kbasepipe.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KBasePipe::KBasePipe()
//-----------------------------------------------------------------------------
{} // Default Constructor


//-----------------------------------------------------------------------------
KBasePipe::~KBasePipe()
//-----------------------------------------------------------------------------
{} // Default Destructor

//-----------------------------------------------------------------------------
bool KBasePipe::splitArgs(KString& argString, CharVec& argVector)
//-----------------------------------------------------------------------------
{
	argVector.push_back(&argString[0]);
	for (size_t i = 0; i < argString.size(); ++i)
	{
		if (argString[i] == ' ')
		{
			argString[i] = '\0';
			if ((argString.size() > i + 1) && (argString[i+1] == '"'))
			{
				argString[i+1] = '\0';
				argVector.push_back(&argString[i+2]);
				do
				{
					++i;
				} while (argString[i] != '"');
				argString[i] = '\0'; // null terminate spaced region
			}
			else
			{
				argVector.push_back(&argString[i + 1]);
			}
		}
	}
	argVector.push_back(NULL); // null terminate
	return !argVector.empty();
} // splitArgs

//-----------------------------------------------------------------------------
bool KBasePipe::WaitForFinished(int msecs)
//-----------------------------------------------------------------------------
{
	if (msecs >= 0)
	{
		int counter = 0;
		while (IsRunning())
		{
			usleep(1000);
			++counter;

			if (counter == msecs)
				return false;
		}
		return true;
	}
	return false;
} // WaitForFinished

} // end namespace dekaf2
