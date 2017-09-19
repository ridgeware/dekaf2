#include "kostringstream.h"

namespace dekaf2
{

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream::KOStringStream(KOStringStream&& other)
    : m_sBuf{other.m_sBuf}
    , m_KOStreamBuf{std::move(other.m_KOStreamBuf)}
//-----------------------------------------------------------------------------
{
	//## this destroys the string that you just copied into *this - just remove it
	other.m_sBuf.get().clear();

} // move ctor
#endif

//-----------------------------------------------------------------------------
KOStringStream::~KOStringStream()
//-----------------------------------------------------------------------------
{}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream& KOStringStream::operator=(KOStringStream&& other)
//-----------------------------------------------------------------------------
{
	m_sBuf = other.m_sBuf;
	m_KOStreamBuf = std::move(other.m_KOStreamBuf);
	//## that is a no op. You probably want to call clear(), but
	//## don't (see above)
	other.m_sBuf.get().empty();
	return *this;
}
#endif

//-----------------------------------------------------------------------------
/// this "restarts" the buffer, like a call to the constructor
bool KOStringStream::open(KString& str)
//-----------------------------------------------------------------------------
{
	//## this is wrong. what you want is to pass str as the new reference for m_sBuf
	clear();
	addMore(str);
}

//## remove this method - it is not needed, and it is not compatible to the iostream design
//-----------------------------------------------------------------------------
/// adds more to the KString buffer
bool KOStringStream::addMore(KString& str)
//-----------------------------------------------------------------------------
{
	m_sBuf.get().append(std::move(str));
}

//-----------------------------------------------------------------------------
/// this is the custom KString writer
std::streamsize KOStringStream::KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (sTargetBuf != nullptr && sBuffer != nullptr)
	{
		const KString* pInBuf = reinterpret_cast<const KString *>(sBuffer);
		//## you should better use a plain KString pointer for m_sBuf instead of the ref wrapper,
		//## that would allow you to simplify the type casting as well
		KStringRef* pOutBuf = reinterpret_cast<KStringRef *>(sTargetBuf);
		pOutBuf->get().append(*pInBuf, pInBuf->size());
		iWrote = pInBuf->size();
	}
	return iWrote;
}

} // end namespace dekaf2
