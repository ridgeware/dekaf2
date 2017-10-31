

#include "kstream.h"

namespace dekaf2
{

KStream::~KStream()
//-----------------------------------------------------------------------------
{
}

template class KReaderWriter<std::fstream>;
template class KReaderWriter<std::stringstream>;
template class KReaderWriter<asio::ip::tcp::iostream>;

}
