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
	return *this;
}
#endif

//-----------------------------------------------------------------------------
/// this "restarts" the buffer, like a call to the constructor
bool KOStringStream::open(KString& str)
//-----------------------------------------------------------------------------
{
	*m_sBuf = str;
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
		KString* pOutBuf = reinterpret_cast<KString *>(sTargetBuf);
		*pOutBuf += *pInBuf;
		iWrote = pInBuf->size();
	}
	return iWrote;
}

} // end namespace dekaf2
