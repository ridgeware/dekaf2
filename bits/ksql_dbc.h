/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#pragma once

#include "../kdefinitions.h"
#include "../krow.h"
#include "../kstring.h"
#include <cinttypes>

DEKAF2_NAMESPACE_BEGIN

/* DBC file format structures:
 These structures, once established, CAN NOT CHANGE.
 Size and offset of each field must be retained as-is in order to maintain compatibility with existing DBC files.
 The first field, "szLeader[]", is used to identify which format a particular file follows
 */

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DBCFileBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using DBT = detail::KCommonSQLBase::DBT;
	using API = detail::KCommonSQLBase::API;

	virtual ~DBCFileBase();

	virtual bool SetBuffer(KStringView sBuffer) = 0;
	virtual DBT GetDBT() const = 0;
	virtual API GetAPI() const = 0;
	virtual KString GetUsername() const = 0;
	virtual KString GetPassword() const = 0;
	virtual KString GetDatabase() const = 0;
	virtual KString GetHostname() const = 0;
	virtual uint16_t GetDBPort() const = 0;
	virtual uint16_t GetVersion() const = 0;

	virtual bool Save(KStringViewZ sFilename) const = 0;
	virtual void SetDBT(DBT dbt) = 0;
	virtual void SetAPI(API api) = 0;
	virtual bool SetUsername(KStringViewZ sName) = 0;
	virtual bool SetPassword(KStringViewZ sPassword) = 0;
	virtual bool SetDatabase(KStringViewZ sDatabase) = 0;
	virtual bool SetHostname(KStringViewZ sHostname) = 0;
	virtual void SetDBPort(uint16_t iDBPort) = 0;

//----------
protected:
//----------

	static bool IntSetBuffer(KStringView sBuffer, void* pBuffer, std::size_t iExpectedSize);
	static bool IntSave(KStringViewZ sFilename, const void* pBuffer, std::size_t iSize);
	static void decrypt (KStringRef& sString);
	static void encrypt (char* string);
	static int32_t DecodeInt(const uint8_t (&iSourceArray)[4]);
	static DBT DecodeDBT(const uint8_t (&iSourceArray)[4])
	{
		return static_cast<DBT>(DecodeInt(iSourceArray));
	}
	static API DecodeAPI(const uint8_t (&iSourceArray)[4])
	{
		return static_cast<API>(DecodeInt(iSourceArray));
	}
	static void EncodeInt(uint8_t (&cTargetArray)[4], int32_t iSourceValue);
	static void EncodeDBT(uint8_t (&cTargetArray)[4], DBT iDBT)
	{
		EncodeInt(cTargetArray, static_cast<int32_t>(iDBT));
	}
	static void EncodeAPI(uint8_t (&cTargetArray)[4], API iAPI)
	{
		EncodeInt(cTargetArray, static_cast<int32_t>(iAPI));
	}
	static KString GetString(const void* pStr, uint16_t iMaxLen);
	static bool SetString(void* pStr, KStringViewZ sStr, uint16_t iMaxLen);
	static uint16_t DecodeVersion(const void* pStr);
	static void FillBufferWithNoise(void* pBuffer, std::size_t iSize);

}; // DBCFileBase

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DBCFILEv1 : public DBCFileBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	enum
	{
		// DBCFILEv1 used 51 bytes for each authentication field
		// however, older versions of KSQL will truncate the fields when reading them if they are over 49 characters long.
		// Therefore, for compatibility, MAXLEN_CONNECTPARM is 49 characters even though sizeof(szUsername) is still 51 bytes.
		MAXLEN_CONNECTPARM = 49
	};

//----------
public:
//----------

	virtual bool SetBuffer(KStringView sBuffer) override final
	{
		return IntSetBuffer(sBuffer, &data, sizeof(Data));
	}

	virtual bool Save(KStringViewZ sFilename) const override final
	{
		return IntSave(sFilename, &data, sizeof(Data));
	}

	virtual DBT GetDBT() const override final { return static_cast<DBT>(data.iDBType); }
	virtual API GetAPI() const override final { return static_cast<API>(data.iAPISet); }
	virtual KString GetUsername() const override final { return GetString(data.szUsername, MAXLEN_CONNECTPARM); }
	virtual KString GetPassword() const override final { return GetString(data.szPassword, MAXLEN_CONNECTPARM); }
	virtual KString GetDatabase() const override final { return GetString(data.szDatabase, MAXLEN_CONNECTPARM); }
	virtual KString GetHostname() const override final { return GetString(data.szHostname, MAXLEN_CONNECTPARM); }
	virtual uint16_t GetDBPort() const override final { return data.iDBPortNum; }
	virtual uint16_t GetVersion() const override final { return DecodeVersion(data.szLeader); }

	virtual void SetDBT(DBT dbt) override final { data.iDBType = static_cast<int32_t>(dbt); }
	virtual void SetAPI(API api) override final { data.iAPISet = static_cast<int32_t>(api); }
	virtual bool SetUsername(KStringViewZ sName) override final { return SetString(data.szUsername, sName, MAXLEN_CONNECTPARM); }
	virtual bool SetPassword(KStringViewZ sPassword) override final { return SetString(data.szPassword, sPassword, MAXLEN_CONNECTPARM); }
	virtual bool SetDatabase(KStringViewZ sDatabase) override final { return SetString(data.szDatabase, sDatabase, MAXLEN_CONNECTPARM); }
	virtual bool SetHostname(KStringViewZ sHostname) override final { return SetString(data.szHostname, sHostname, MAXLEN_CONNECTPARM); }
	virtual void SetDBPort(uint16_t iDBPort) override final { data.iDBPortNum = iDBPort; }

//----------
private:
//----------

	struct Data
	{
		Data()
		{
			FillBufferWithNoise(this, sizeof(Data));
			strcpy(szLeader, "KSQLDBC1");
		}

		char           szLeader[10]; // <-- length of leader can never change as struct gets rev'ed

		int32_t        iDBType;
		int32_t        iAPISet;
		unsigned char  szUsername[MAXLEN_CONNECTPARM+2]; // order: UPDH
		unsigned char  szPassword[MAXLEN_CONNECTPARM+2];
		unsigned char  szDatabase[MAXLEN_CONNECTPARM+2];
		unsigned char  szHostname[MAXLEN_CONNECTPARM+2];
		int32_t        iDBPortNum;
	};

	Data data;
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DBCFILEv2 : public DBCFileBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	enum
	{
		// DBCFILEv2 used 51 bytes for each authentication field
		// however, older versions of KSQL will truncate the fields when reading them if they are over 49 characters long.
		// Therefore, for compatibility, MAXLEN_CONNECPARM is 49 characters even though sizeof(szUsername) is still 51 bytes.
		MAXLEN_CONNECTPARM = 49
	};

//----------
public:
//----------

	virtual bool SetBuffer(KStringView sBuffer) override final
	{
		return IntSetBuffer(sBuffer, &data, sizeof(Data));
	}

	virtual bool Save(KStringViewZ sFilename) const override final
	{
		return IntSave(sFilename, &data, sizeof(Data));
	}

	virtual DBT GetDBT() const override final { return DecodeDBT(data.iDBType); }
	virtual API GetAPI() const override final { return DecodeAPI(data.iAPISet); }
	virtual KString GetUsername() const override final { return GetString(data.szUsername, MAXLEN_CONNECTPARM); }
	virtual KString GetPassword() const override final { return GetString(data.szPassword, MAXLEN_CONNECTPARM); }
	virtual KString GetDatabase() const override final { return GetString(data.szDatabase, MAXLEN_CONNECTPARM); }
	virtual KString GetHostname() const override final { return GetString(data.szHostname, MAXLEN_CONNECTPARM); }
	virtual uint16_t GetDBPort() const override final { return DecodeInt(data.iDBPortNum); }
	virtual uint16_t GetVersion() const override final { return DecodeVersion(data.szLeader); }

	virtual void SetDBT(DBT dbt) override final { EncodeDBT(data.iDBType, dbt); }
	virtual void SetAPI(API api) override final { EncodeAPI(data.iAPISet, api); }
	virtual bool SetUsername(KStringViewZ sName) override final { return SetString(data.szUsername, sName, MAXLEN_CONNECTPARM); }
	virtual bool SetPassword(KStringViewZ sPassword) override final { return SetString(data.szPassword, sPassword, MAXLEN_CONNECTPARM); }
	virtual bool SetDatabase(KStringViewZ sDatabase) override final { return SetString(data.szDatabase, sDatabase, MAXLEN_CONNECTPARM); }
	virtual bool SetHostname(KStringViewZ sHostname) override final { return SetString(data.szHostname, sHostname, MAXLEN_CONNECTPARM); }
	virtual void SetDBPort(uint16_t iDBPort) override final { EncodeInt(data.iDBPortNum, iDBPort); }

//----------
private:
//----------

	struct Data
	{
		Data()
		{
			FillBufferWithNoise(this, sizeof(Data));
			strcpy(szLeader, "KSQLDBC2");
		}

		char          szLeader[10]; // <-- length of leader can never change as struct gets rev'ed
		uint8_t       iDBType[4];
		uint8_t       iAPISet[4];
		unsigned char szUsername[MAXLEN_CONNECTPARM+2]; // order: UPDH
		unsigned char szPassword[MAXLEN_CONNECTPARM+2];
		unsigned char szDatabase[MAXLEN_CONNECTPARM+2];
		unsigned char szHostname[MAXLEN_CONNECTPARM+2];
		uint8_t       iDBPortNum[4];
	};

	Data data;
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// DBCFilev3 was introduced to increase the maximum length of the connection parameters
class DBCFILEv3 : public DBCFileBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	enum
	{
		MAXLEN_CONNECTPARM = 127
	};

//----------
public:
//----------

	virtual bool SetBuffer(KStringView sBuffer) override final
	{
		return IntSetBuffer(sBuffer, &data, sizeof(Data));
	}

	virtual bool Save(KStringViewZ sFilename) const override final
	{
		return IntSave(sFilename, &data, sizeof(Data));
	}

	virtual DBT GetDBT() const override final { return DecodeDBT(data.iDBType); }
	virtual API GetAPI() const override final { return DecodeAPI(data.iAPISet); }
	virtual KString GetUsername() const override final { return GetString(data.szUsername, MAXLEN_CONNECTPARM); }
	virtual KString GetPassword() const override final { return GetString(data.szPassword, MAXLEN_CONNECTPARM); }
	virtual KString GetDatabase() const override final { return GetString(data.szDatabase, MAXLEN_CONNECTPARM); }
	virtual KString GetHostname() const override final { return GetString(data.szHostname, MAXLEN_CONNECTPARM); }
	virtual uint16_t GetDBPort() const override final { return DecodeInt(data.iDBPortNum); }
	virtual uint16_t GetVersion() const override final { return DecodeVersion(data.szLeader); }

	virtual void SetDBT(DBT dbt) override final { EncodeDBT(data.iDBType, dbt); }
	virtual void SetAPI(API api) override final { EncodeAPI(data.iAPISet, api); }
	virtual bool SetUsername(KStringViewZ sName) override final { return SetString(data.szUsername, sName, MAXLEN_CONNECTPARM); }
	virtual bool SetPassword(KStringViewZ sPassword) override final { return SetString(data.szPassword, sPassword, MAXLEN_CONNECTPARM); }
	virtual bool SetDatabase(KStringViewZ sDatabase) override final { return SetString(data.szDatabase, sDatabase, MAXLEN_CONNECTPARM); }
	virtual bool SetHostname(KStringViewZ sHostname) override final { return SetString(data.szHostname, sHostname, MAXLEN_CONNECTPARM); }
	virtual void SetDBPort(uint16_t iDBPort) override final { EncodeInt(data.iDBPortNum, iDBPort); }

//----------
private:
//----------

	struct Data
	{
		Data()
		{
			FillBufferWithNoise(this, sizeof(Data));
			strcpy(szLeader, "KSQLDBC3");
		}

		char          szLeader[10]; // <-- length of leader can never change as struct gets rev'ed
		uint8_t       iDBType[4];
		uint8_t       iAPISet[4];
		unsigned char szUsername[MAXLEN_CONNECTPARM+1]; // order: UPDH
		unsigned char szPassword[MAXLEN_CONNECTPARM+1];
		unsigned char szDatabase[MAXLEN_CONNECTPARM+1];
		unsigned char szHostname[MAXLEN_CONNECTPARM+1];
		uint8_t       iDBPortNum[4];
	};

	Data data;
};

DEKAF2_NAMESPACE_END
