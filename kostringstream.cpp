#include "kostringstream.h"

namespace dekaf2
{

//KOStringStream::KOStringStream()
//{}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream::KOStringStream(KOStringStream&& other)
    : m_sBuf{other.m_sBuf}
    , m_KOStreamBuf{std::move(other.m_KOStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_sBuf.get().clear();

} // move ctor
#endif

//-----------------------------------------------------------------------------
KOStringStream::~KOStringStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream& KOStringStream::operator=(KOStringStream&& other)
//-----------------------------------------------------------------------------
{
	m_sBuf = other.m_sBuf;
	m_KOStreamBuf = std::move(other.m_KOStreamBuf);
	other.m_sBuf.get().empty();
	return *this;
}
#endif

//-----------------------------------------------------------------------------
/// adds more to the KString buffer
bool KOStringStream::addMore(KString& str)
//-----------------------------------------------------------------------------
{
	m_sBuf.get().append(str);
}

//-----------------------------------------------------------------------------
/// Adds a formatted string to the internal buffer
template<class... Args>
bool KOStringStream::addFormatted(Args&&... args)
//-----------------------------------------------------------------------------
{
	return m_sBuf.get().append(kfFormat(this, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
/// this is the custom KString writer
std::streamsize KOStringStream::KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (sTargetBuf)
	{
		const KString* pInBuf = reinterpret_cast<const KString *>(sBuffer);
		KStringRef* pOutBuf = reinterpret_cast<KStringRef *>(sTargetBuf);
		pOutBuf->get().append(*pInBuf);
		//sTargetBuf.get().append(*sBuffer);
	}

	return iWrote;

}

} // end namespace dekaf2
