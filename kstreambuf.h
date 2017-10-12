#include <streambuf>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customizable input stream buffer
class KInStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Reader's function's signature:
	/// std::streamsize Reader(void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns read bytes. CustomPointer can be used for anything, to the discretion of the
	/// Reader.
	typedef std::streamsize (*Reader)(void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Reader function, it will be called by std::streambuf on buffer reads
	KInStreamBuf(Reader rcb, void* CustomPointerR = nullptr)
	//-----------------------------------------------------------------------------
	    : m_CallbackR(rcb), m_CustomPointerR(CustomPointerR)
	{
	}
	//-----------------------------------------------------------------------------
	virtual ~KInStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type underflow() override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	Reader m_CallbackR{nullptr};
	void* m_CustomPointerR{nullptr};
	enum { STREAMBUFSIZE = 256 };
	char_type m_buf[STREAMBUFSIZE];

}; // KInStreamBuf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customized output stream buffer
class KOutStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	typedef std::streamsize (*Writer)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Writer function, it will be called by std::streambuf on buffer flushes
	KOutStreamBuf(Writer cb, void* CustomPointer = nullptr)
	//-----------------------------------------------------------------------------
	    : m_Callback(cb), m_CustomPointer(CustomPointer)
	{
	}
	//-----------------------------------------------------------------------------
	virtual ~KOutStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	Writer m_Callback{nullptr};
	void* m_CustomPointer{nullptr};

}; // KOutStreamBuf


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KStreamBuf : public KInStreamBuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	typedef std::streamsize (*Writer)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Reader function, it will be called by std::streambuf on buffer reads
	KStreamBuf(Reader rcb, Writer wcb, void* CustomPointerR = nullptr, void* CustomPointerW = nullptr)
	//-----------------------------------------------------------------------------
	    : KInStreamBuf(rcb, CustomPointerR)
	    , m_CallbackW(wcb), m_CustomPointerW(CustomPointerW)
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	Writer m_CallbackW{nullptr};
	void* m_CustomPointerW{nullptr};

}; // KStreamBuf

}
