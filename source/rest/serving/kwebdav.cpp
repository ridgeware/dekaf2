/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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
*/

#include <dekaf2/rest/serving/kwebdav.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/serving/kwebserverpermissions.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/data/xml/kxml.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/util/id/kuuid.h>
#include <array>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace {

namespace dav {

// property local names (for comparison with parsed request properties)
constexpr KStringViewZ resourcetype     = "resourcetype";
constexpr KStringViewZ displayname      = "displayname";
constexpr KStringViewZ getlastmodified  = "getlastmodified";
constexpr KStringViewZ creationdate     = "creationdate";
constexpr KStringViewZ getcontentlength = "getcontentlength";
constexpr KStringViewZ getcontenttype   = "getcontenttype";
constexpr KStringViewZ getetag          = "getetag";

// XML element local names (for parsing request bodies)
constexpr KStringViewZ propfind         = "propfind";
constexpr KStringViewZ allprop          = "allprop";
constexpr KStringViewZ propname         = "propname";
constexpr KStringViewZ prop             = "prop";
constexpr KStringViewZ propertyupdate   = "propertyupdate";
constexpr KStringViewZ set              = "set";
constexpr KStringViewZ remove           = "remove";
constexpr KStringViewZ lockinfo         = "lockinfo";
constexpr KStringViewZ owner            = "owner";
constexpr KStringViewZ href             = "href";

// Depth header values
constexpr KStringViewZ infinity         = "infinity";

// Lock defaults (opaquelocktoken is the URI scheme from RFC 4918 §6.4)
constexpr KStringViewZ opaquelocktoken  = "opaquelocktoken";
constexpr KStringViewZ second_600       = "Second-600";

// Options response values
constexpr KStringViewZ compliance       = "1, 2";
constexpr KStringViewZ allowed_methods  = "OPTIONS,GET,HEAD,PUT,POST,DELETE,PROPFIND,PROPPATCH,MKCOL,COPY,MOVE,LOCK,UNLOCK";

// Header names not in KHTTPHeader
constexpr KStringViewZ lock_token       = "lock-token";
constexpr KStringViewZ timeout          = "timeout";

// namespace declaration
constexpr KStringViewZ xmlns_D          = "xmlns:D";
constexpr KStringViewZ ns_DAV           = "DAV:";

// status line strings
constexpr KStringViewZ status_200       = "HTTP/1.1 200 OK";
constexpr KStringViewZ status_403       = "HTTP/1.1 403 Forbidden";
constexpr KStringViewZ status_404       = "HTTP/1.1 404 Not Found";
constexpr KStringViewZ status_409       = "HTTP/1.1 409 Conflict";

// D:-prefixed XML element names (for response output)
namespace D {
constexpr KStringViewZ multistatus      = "D:multistatus";
constexpr KStringViewZ response         = "D:response";
constexpr KStringViewZ href             = "D:href";
constexpr KStringViewZ propstat         = "D:propstat";
constexpr KStringViewZ prop             = "D:prop";
constexpr KStringViewZ status           = "D:status";
constexpr KStringViewZ resourcetype     = "D:resourcetype";
constexpr KStringViewZ collection       = "D:collection";
constexpr KStringViewZ displayname      = "D:displayname";
constexpr KStringViewZ getlastmodified  = "D:getlastmodified";
constexpr KStringViewZ creationdate     = "D:creationdate";
constexpr KStringViewZ getcontentlength = "D:getcontentlength";
constexpr KStringViewZ getcontenttype   = "D:getcontenttype";
constexpr KStringViewZ getetag          = "D:getetag";
constexpr KStringViewZ lockdiscovery    = "D:lockdiscovery";
constexpr KStringViewZ activelock       = "D:activelock";
constexpr KStringViewZ locktype         = "D:locktype";
constexpr KStringViewZ write            = "D:write";
constexpr KStringViewZ lockscope        = "D:lockscope";
constexpr KStringViewZ exclusive        = "D:exclusive";
constexpr KStringViewZ depth            = "D:depth";
constexpr KStringViewZ owner            = "D:owner";
constexpr KStringViewZ timeout          = "D:timeout";
constexpr KStringViewZ locktoken        = "D:locktoken";
constexpr KStringViewZ lockroot         = "D:lockroot";
} // D

} // dav

enum class PropfindType : uint8_t { AllProp, PropName, Prop };

struct PropfindRequest
{
	PropfindType         Type { PropfindType::AllProp };
	std::vector<KString> RequestedProps; // local names after stripping namespace prefix
};

/// known DAV properties that our server supports
static constexpr std::array<KStringViewZ, 7> s_KnownProperties
{{
	dav::resourcetype,
	dav::displayname,
	dav::getlastmodified,
	dav::creationdate,
	dav::getcontentlength,
	dav::getcontenttype,
	dav::getetag
}};

//-----------------------------------------------------------------------------
/// strip namespace prefix from an XML element name (e.g., "D:resourcetype" -> "resourcetype")
KStringView StripNamespacePrefix(KStringView sName) noexcept
//-----------------------------------------------------------------------------
{
	auto iPos = sName.find(':');
	return (iPos != KStringView::npos) ? sName.substr(iPos + 1) : sName;
}

//-----------------------------------------------------------------------------
/// check if a property local name is a known DAV property that is applicable
/// for the given resource
bool IsApplicableProperty(KStringView sLocalName, const KFileStat& Stat) noexcept
//-----------------------------------------------------------------------------
{
	for (const auto& sProp : s_KnownProperties)
	{
		if (sProp == sLocalName)
		{
			// getcontentlength and getcontenttype are only defined for files
			if (Stat.IsDirectory() && (sLocalName == dav::getcontentlength || sLocalName == dav::getcontenttype))
			{
				return false;
			}
			return true;
		}
	}
	return false;

} // IsApplicableProperty

//-----------------------------------------------------------------------------
/// add a single DAV property value to a prop node
void AddPropertyValue(KXMLNode& Prop, KStringView sLocalName, const KFileStat& Stat, KStringView sHref, KStringView sContentType)
//-----------------------------------------------------------------------------
{
	if (sLocalName == dav::resourcetype)
	{
		auto ResType = Prop.AddNode(dav::D::resourcetype);
		if (Stat.IsDirectory()) ResType.AddNode(dav::D::collection);
	}
	else if (sLocalName == dav::displayname)
	{
		KStringView sTrimmed = sHref;
		sTrimmed.remove_suffix('/');
		auto sName = kBasename(sTrimmed);
		if (sName.empty()) sName = "/";
		Prop.AddNode(dav::D::displayname).SetValue(sName);
	}
	else if (sLocalName == dav::getlastmodified)
	{
		Prop.AddNode(dav::D::getlastmodified).SetValue(kFormHTTPTimestamp(Stat.ModificationTime()));
	}
	else if (sLocalName == dav::creationdate)
	{
		Prop.AddNode(dav::D::creationdate).SetValue(kFormTimestamp(Stat.ChangeTime(), "{:%Y-%m-%dT%H:%M:%SZ}"));
	}
	else if (sLocalName == dav::getcontentlength)
	{
		Prop.AddNode(dav::D::getcontentlength).SetValue(kFormat("{}", Stat.Size()));
	}
	else if (sLocalName == dav::getcontenttype)
	{
		if (!sContentType.empty())
		{
			Prop.AddNode(dav::D::getcontenttype).SetValue(sContentType);
		}
		else
		{
			Prop.AddNode(dav::D::getcontenttype);
		}
	}
	else if (sLocalName == dav::getetag)
	{
		Prop.AddNode(dav::D::getetag).SetValue(KWebDAV::GenerateETag(Stat));
	}

} // AddPropertyValue

//-----------------------------------------------------------------------------
/// parse the PROPFIND request body to determine what properties are requested
PropfindRequest ParsePropfindBody(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	PropfindRequest Request;

	if (!HTTP.Request.HasContent(HTTP.Request.Method))
	{
		// no body means allprop (RFC 4918 Section 9.1)
		return Request;
	}

	auto sBody = HTTP.Read();

	if (sBody.empty())
	{
		// empty body means allprop (RFC 4918 Section 9.1)
		return Request;
	}

	KXML ReqXML(sBody);

	if (!ReqXML)
	{
		kDebug(1, "failed to parse PROPFIND request body, treating as allprop");
		return Request;
	}

	// find the propfind element (may have any namespace prefix)
	for (const auto& Node : ReqXML)
	{
		if (StripNamespacePrefix(Node.GetName()) == dav::propfind)
		{
			for (const auto& Child : Node)
			{
				auto sChildName = StripNamespacePrefix(Child.GetName());

				if (sChildName == dav::allprop)
				{
					Request.Type = PropfindType::AllProp;
					return Request;
				}
				else if (sChildName == dav::propname)
				{
					Request.Type = PropfindType::PropName;
					return Request;
				}
				else if (sChildName == dav::prop)
				{
					Request.Type = PropfindType::Prop;

					for (const auto& PropChild : Child)
					{
						Request.RequestedProps.emplace_back(StripNamespacePrefix(PropChild.GetName()));
					}

					return Request;
				}
			}
		}
	}

	// fallback: treat as allprop
	return Request;

} // ParsePropfindBody

//-----------------------------------------------------------------------------
/// add one PROPFIND <D:response> entry to the multistatus node
void AddPropfindEntry(KXMLNode& Multistatus, KStringView sHref, const KFileStat& Stat, KStringView sContentType, const PropfindRequest& PFReq)
//-----------------------------------------------------------------------------
{
	auto Response = Multistatus.AddNode(dav::D::response);
	Response.AddNode(dav::D::href).SetValue(sHref);

	if (PFReq.Type == PropfindType::PropName)
	{
		// return just the property names we support (empty elements)
		auto Propstat = Response.AddNode(dav::D::propstat);
		auto Prop     = Propstat.AddNode(dav::D::prop);

		Prop.AddNode(dav::D::resourcetype);
		Prop.AddNode(dav::D::displayname);
		Prop.AddNode(dav::D::getlastmodified);
		Prop.AddNode(dav::D::creationdate);

		if (!Stat.IsDirectory())
		{
			Prop.AddNode(dav::D::getcontentlength);
			Prop.AddNode(dav::D::getcontenttype);
		}

		Prop.AddNode(dav::D::getetag);

		Propstat.AddNode(dav::D::status).SetValue(dav::status_200);
		return;
	}

	if (PFReq.Type == PropfindType::AllProp || PFReq.RequestedProps.empty())
	{
		// return all known properties with values
		auto Propstat = Response.AddNode(dav::D::propstat);
		auto Prop     = Propstat.AddNode(dav::D::prop);

		for (const auto& sProp : s_KnownProperties)
		{
			if (IsApplicableProperty(sProp, Stat))
			{
				AddPropertyValue(Prop, sProp, Stat, sHref, sContentType);
			}
		}

		Propstat.AddNode(dav::D::status).SetValue(dav::status_200);
		return;
	}

	// targeted Prop request: split into 200 OK and 404 Not Found propstats
	KXMLNode FoundProp;
	KXMLNode NotFoundProp;

	for (const auto& sPropName : PFReq.RequestedProps)
	{
		if (IsApplicableProperty(sPropName, Stat))
		{
			if (!FoundProp)
			{
				auto Propstat = Response.AddNode(dav::D::propstat);
				FoundProp     = Propstat.AddNode(dav::D::prop);
				Propstat.AddNode(dav::D::status).SetValue(dav::status_200);
			}
			AddPropertyValue(FoundProp, sPropName, Stat, sHref, sContentType);
		}
		else
		{
			if (!NotFoundProp)
			{
				auto Propstat = Response.AddNode(dav::D::propstat);
				NotFoundProp  = Propstat.AddNode(dav::D::prop);
				Propstat.AddNode(dav::D::status).SetValue(dav::status_404);
			}
			// emit unknown property as empty element (without namespace prefix
			// to avoid namespace declaration issues)
			NotFoundProp.AddNode(sPropName);
		}
	}

} // AddPropfindEntry

//-----------------------------------------------------------------------------
/// maximum depth for recursive PROPFIND to prevent DoS on large directory trees
static constexpr uint32_t MAX_PROPFIND_DEPTH { 5 };

/// parse the Depth header value, capped at MAX_PROPFIND_DEPTH
uint32_t ParseDepth(const KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	const auto& sDepth = HTTP.Request.Headers.Get(KHTTPHeader::DEPTH);

	if (sDepth.empty() || sDepth == dav::infinity)
	{
		return MAX_PROPFIND_DEPTH;
	}

	auto iDepth = sDepth.UInt32();

	if (iDepth > MAX_PROPFIND_DEPTH)
	{
		kDebug(2, "Depth: {} requested, capping at {}", iDepth, MAX_PROPFIND_DEPTH);
		return MAX_PROPFIND_DEPTH;
	}

	return iDepth;

} // ParseDepth

//-----------------------------------------------------------------------------
/// extract the path component from a Destination header URL
KString ParseDestination(const KRESTServer& HTTP, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	const auto& sDestination = HTTP.Request.Headers.Get(KHTTPHeader::DESTINATION);

	if (sDestination.empty())
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing Destination header" };
	}

	// the Destination header contains a full URL, extract the path
	KURL Url(sDestination);
	KString sPath = Url.Path.get();

	if (sPath.empty())
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "invalid Destination header" };
	}

	return sPath;

} // ParseDestination

//-----------------------------------------------------------------------------
/// check the Overwrite header (default is T = true)
bool ParseOverwrite(const KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	const auto& sOverwrite = HTTP.Request.Headers.Get(KHTTPHeader::OVERWRITE);
	return sOverwrite.empty() || sOverwrite == "T";

} // ParseOverwrite

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KString KWebDAV::GenerateETag(const KFileStat& Stat)
//-----------------------------------------------------------------------------
{
	return kFormat("\"{:x}-{:x}-{:x}\"",
	               Stat.Inode(),
	               Stat.ModificationTime().to_time_t(),
	               Stat.Size());

} // GenerateETag

//-----------------------------------------------------------------------------
KString KWebDAV::ResolveFilesystemPath(KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	auto sRelPath = sRequestPath;

	if (!sRelPath.remove_prefix(sRoute))
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("internal error: request path '{}' does not start with route '{}'", sRequestPath, sRoute) };
	}

	if (!kIsSafeURLPath(sRelPath))
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("invalid path: {}", sRelPath) };
	}

	// remove leading slash for concatenation
	sRelPath.remove_prefix('/');

	KString sPath = sDocumentRoot;

	if (!sRelPath.empty())
	{
		sPath += kDirSep;
		sPath += sRelPath;
	}

	return sPath;

} // ResolveFilesystemPath

//-----------------------------------------------------------------------------
void KWebDAV::Propfind(KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	auto PFReq     = ParsePropfindBody(HTTP);
	auto iDepth    = ParseDepth(HTTP);
	auto sFilePath = ResolveFilesystemPath(sDocumentRoot, sRequestPath, sRoute);

	KFileStat Stat(sFilePath);

	if (!Stat.IsFile() && !Stat.IsDirectory())
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("not found: {}", sRequestPath) };
	}

	HTTP.xml.tx.AddXMLDeclaration();

	auto Root = KXMLNode(HTTP.xml.tx).AddNode(dav::D::multistatus);
	Root.AddAttribute(dav::xmlns_D, dav::ns_DAV);

	// add the requested resource itself
	auto sContentType = Stat.IsFile() ? KMIME::CreateByExtension(sFilePath).Serialize() : KString{};
	AddPropfindEntry(Root, sRequestPath, Stat, sContentType, PFReq);

	// if it is a directory and depth > 0, list children
	if (Stat.IsDirectory() && iDepth > 0)
	{
		KDirectory Dir(sFilePath, KFileTypes::ALL, iDepth > 1);

		kDebug(2, "PROPFIND depth {} on '{}' (docroot '{}'), found {} entries",
		       iDepth, sFilePath, sDocumentRoot, Dir.size());

		// On Windows, KDirectory entry paths use native backslashes but
		// sDocumentRoot may use forward slashes - normalize for matching.
		// Do NOT replace backslashes on Unix where \\ is a valid filename char.
#if DEKAF2_IS_WINDOWS
		KString sDocRootNormalized(sDocumentRoot);
		sDocRootNormalized.Replace('\\', '/');
#endif

		for (const auto& Entry : Dir)
		{
#if DEKAF2_IS_WINDOWS
			KString sChildNormalized = Entry.Path();
			sChildNormalized.Replace('\\', '/');
			KStringView sRelChild = sChildNormalized;
#else
			KStringView sRelChild = Entry.Path();
#endif

			// make the child path relative to document root
			if (!sRelChild.remove_prefix(
#if DEKAF2_IS_WINDOWS
				sDocRootNormalized
#else
				sDocumentRoot
#endif
			))
			{
				kDebug(2, "PROPFIND: child path '{}' does not start with docroot '{}', skipping",
				       Entry.Path(), sDocumentRoot);
				continue;
			}

			// build the href: route prefix + relative path
			// (note: sRoute has the trailing /* already stripped by KHTTPAnalyzedPath)
			KString sHref = sRoute;
			sHref += sRelChild;

			// add trailing slash for directories
			if (Entry.Type() == KFileType::DIRECTORY && !sHref.ends_with('/'))
			{
				sHref += '/';
			}

			auto sChildContentType = Entry.Type() == KFileType::FILE
			                       ? KMIME::CreateByExtension(Entry.Path()).Serialize()
			                       : KString{};

			AddPropfindEntry(Root, sHref, Entry.FileStat(), sChildContentType, PFReq);
		}
	}

	HTTP.SetStatus(KHTTPError::H2xx_MULTISTATUS);

} // Propfind

//-----------------------------------------------------------------------------
void KWebDAV::Proppatch(KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	auto sFilePath = ResolveFilesystemPath(sDocumentRoot, sRequestPath, sRoute);

	KFileStat Stat(sFilePath);

	if (!Stat.IsFile() && !Stat.IsDirectory())
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("not found: {}", sRequestPath) };
	}

	auto sBody = HTTP.Read();

	if (sBody.empty())
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "PROPPATCH request body is empty" };
	}

	KXML ReqXML(sBody);

	if (!ReqXML)
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "failed to parse PROPPATCH request body" };
	}

	// collect operations: pair of (property local name, status line)
	std::vector<std::pair<KString, KStringViewZ>> Results;

	// find the propertyupdate element
	for (const auto& Node : ReqXML)
	{
		if (StripNamespacePrefix(Node.GetName()) != dav::propertyupdate)
		{
			continue;
		}

		for (const auto& Action : Node)
		{
			auto sActionName = StripNamespacePrefix(Action.GetName());
			bool bIsSet      = (sActionName == dav::set);
			bool bIsRemove   = (sActionName == dav::remove);

			if (!bIsSet && !bIsRemove)
			{
				continue;
			}

			for (const auto& PropNode : Action)
			{
				if (StripNamespacePrefix(PropNode.GetName()) != dav::prop)
				{
					continue;
				}

				for (const auto& Property : PropNode)
				{
					auto sLocalName = StripNamespacePrefix(Property.GetName());

					if (bIsSet && sLocalName == dav::getlastmodified)
					{
						// actually set the file modification time
						auto tTime = kParseHTTPTimestamp(Property.GetValue());

						if (tTime.ok() && kSetLastMod(sFilePath, tTime))
						{
							Results.emplace_back(sLocalName, dav::status_200);
						}
						else
						{
							Results.emplace_back(sLocalName, dav::status_409);
						}
					}
					else if (sLocalName == dav::resourcetype
					      || sLocalName == dav::getcontentlength
					      || sLocalName == dav::getetag)
					{
						// these are read-only live properties
						Results.emplace_back(sLocalName, dav::status_403);
					}
					else
					{
						// for all other properties (dead properties, creationdate, displayname, etc.)
						// accept the operation but don't persist (no dead property store yet)
						Results.emplace_back(sLocalName, dav::status_200);
					}
				}
			}
		}
	}

	// build the 207 Multi-Status response
	HTTP.xml.tx.AddXMLDeclaration();

	auto Root = KXMLNode(HTTP.xml.tx).AddNode(dav::D::multistatus);
	Root.AddAttribute(dav::xmlns_D, dav::ns_DAV);

	auto Response = Root.AddNode(dav::D::response);
	Response.AddNode(dav::D::href).SetValue(sRequestPath);

	// group results by status
	for (const auto& Result : Results)
	{
		auto Propstat = Response.AddNode(dav::D::propstat);
		auto Prop     = Propstat.AddNode(dav::D::prop);
		Prop.AddNode(kFormat("D:{}", Result.first));
		Propstat.AddNode(dav::D::status).SetValue(Result.second);
	}

	HTTP.SetStatus(KHTTPError::H2xx_MULTISTATUS);

} // Proppatch

//-----------------------------------------------------------------------------
void KWebDAV::Mkcol(KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	auto sFilePath = ResolveFilesystemPath(sDocumentRoot, sRequestPath, sRoute);

	if (kDirExists(sFilePath))
	{
		throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("collection already exists: {}", sRequestPath) };
	}

	if (kFileExists(sFilePath))
	{
		throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("a non-collection resource exists at: {}", sRequestPath) };
	}

	// check parent exists
	KString sParent = kDirname(sFilePath);

	if (!kDirExists(sParent))
	{
		throw KHTTPError { KHTTPError::H4xx_CONFLICT, kFormat("parent collection does not exist for: {}", sRequestPath) };
	}

	if (!kCreateDir(sFilePath, DEKAF2_MODE_CREATE_DIR, false))
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("cannot create collection: {}", sRequestPath) };
	}

	HTTP.SetStatus(201);

} // Mkcol

//-----------------------------------------------------------------------------
void KWebDAV::CopyOrMove(KRESTServer& HTTP,
                          KStringView sDocumentRoot,
                          KStringView sRequestPath,
                          KStringView sRoute,
                          const KWebServerPermissions& Permissions,
                          KStringView sUser,
                          bool bIsMove)
//-----------------------------------------------------------------------------
{
	auto sSourcePath = ResolveFilesystemPath(sDocumentRoot, sRequestPath, sRoute);

	KFileStat SourceStat(sSourcePath);

	if (!SourceStat.IsFile() && !SourceStat.IsDirectory())
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("source not found: {}", sRequestPath) };
	}

	auto sDestRequestPath = ParseDestination(HTTP, sRoute);
	bool bOverwrite       = ParseOverwrite(HTTP);

	// check Write permission on destination path
	if (!Permissions.IsAllowed(sUser, KHTTPMethod::PUT, sDestRequestPath))
	{
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, kFormat("write not permitted for destination: {}", sDestRequestPath) };
	}

	auto sDestPath = ResolveFilesystemPath(sDocumentRoot, sDestRequestPath, sRoute);

	bool bDestExists = kFileExists(sDestPath) || kDirExists(sDestPath);

	if (bDestExists && !bOverwrite)
	{
		throw KHTTPError { KHTTPError::H4xx_PRECONDITION_FAILED, kFormat("destination already exists: {}", sDestRequestPath) };
	}

	bool bSuccess;

	if (bIsMove)
	{
		bSuccess = kRename(sSourcePath, sDestPath);

		if (!bSuccess)
		{
			// kRename fails across filesystems, try kMove
			bSuccess = kMove(sSourcePath, sDestPath);
		}
	}
	else
	{
		bSuccess = kCopy(sSourcePath, sDestPath);
	}

	if (!bSuccess)
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("cannot {} '{}' to '{}'", bIsMove ? "move" : "copy", sRequestPath, sDestRequestPath) };
	}

	HTTP.SetStatus(bDestExists ? 204 : 201);

} // CopyOrMove

//-----------------------------------------------------------------------------
void KWebDAV::Delete(KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	auto sFilePath = ResolveFilesystemPath(sDocumentRoot, sRequestPath, sRoute);

	KFileStat Stat(sFilePath);

	if (Stat.IsFile())
	{
		if (!kRemoveFile(sFilePath))
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("cannot delete file: {}", sRequestPath) };
		}
	}
	else if (Stat.IsDirectory())
	{
		if (!kRemoveDir(sFilePath))
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("cannot delete collection: {}", sRequestPath) };
		}
	}
	else
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("not found: {}", sRequestPath) };
	}

	HTTP.SetStatus(204);

} // Delete

//-----------------------------------------------------------------------------
void KWebDAV::Options(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Set(KHTTPHeader::DAV, dav::compliance);
	HTTP.Response.Headers.Set(KHTTPHeader::ALLOW, dav::allowed_methods);
	HTTP.SetStatus(200);

} // Options

//-----------------------------------------------------------------------------
void KWebDAV::Lock(KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute)
//-----------------------------------------------------------------------------
{
	auto sFilePath = ResolveFilesystemPath(sDocumentRoot, sRequestPath, sRoute);

	// verify the resource or its parent exists
	KFileStat Stat(sFilePath);

	if (!Stat.IsFile() && !Stat.IsDirectory())
	{
		// lock-null resource: check that the parent directory exists
		KString sParent = kDirname(sFilePath);

		if (!kDirExists(sParent))
		{
			throw KHTTPError { KHTTPError::H4xx_CONFLICT, kFormat("parent collection does not exist for: {}", sRequestPath) };
		}

		// create an empty file as a lock-null resource placeholder
		if (!kWriteFile(sFilePath, ""))
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("cannot create lock-null resource: {}", sRequestPath) };
		}
	}

	// parse the lock request body (we accept it but don't deeply validate)
	KString sOwnerHref;
	auto sBody = HTTP.Read();

	if (!sBody.empty())
	{
		KXML ReqXML(sBody);

		if (ReqXML)
		{
			// try to extract the owner href
			for (const auto& Node : ReqXML)
			{
				if (StripNamespacePrefix(Node.GetName()) == dav::lockinfo)
				{
					for (const auto& Child : Node)
					{
						if (StripNamespacePrefix(Child.GetName()) == dav::owner)
						{
							for (const auto& OwnerChild : Child)
							{
								if (StripNamespacePrefix(OwnerChild.GetName()) == dav::href)
								{
									sOwnerHref = OwnerChild.GetValue();
								}
							}
						}
					}
				}
			}
		}
	}

	// generate a unique lock token
	KString sLockToken = kFormat("{}:{}", dav::opaquelocktoken, KUUID().ToString());

	// parse the Timeout header (default: 600 seconds)
	const auto& sTimeout = HTTP.Request.Headers.Get(dav::timeout);
	KString sTimeoutValue = sTimeout.empty() ? KString(dav::second_600) : KString(sTimeout);

	// build the lock discovery response
	HTTP.xml.tx.AddXMLDeclaration();

	auto Root = KXMLNode(HTTP.xml.tx).AddNode(dav::D::prop);
	Root.AddAttribute(dav::xmlns_D, dav::ns_DAV);

	auto LockDiscovery = Root.AddNode(dav::D::lockdiscovery);
	auto ActiveLock    = LockDiscovery.AddNode(dav::D::activelock);

	auto LockType = ActiveLock.AddNode(dav::D::locktype);
	LockType.AddNode(dav::D::write);

	auto LockScope = ActiveLock.AddNode(dav::D::lockscope);
	LockScope.AddNode(dav::D::exclusive);

	ActiveLock.AddNode(dav::D::depth).SetValue(dav::infinity);

	if (!sOwnerHref.empty())
	{
		auto Owner = ActiveLock.AddNode(dav::D::owner);
		Owner.AddNode(dav::D::href).SetValue(sOwnerHref);
	}

	ActiveLock.AddNode(dav::D::timeout).SetValue(sTimeoutValue);

	auto LockTokenNode = ActiveLock.AddNode(dav::D::locktoken);
	LockTokenNode.AddNode(dav::D::href).SetValue(sLockToken);

	auto LockRoot = ActiveLock.AddNode(dav::D::lockroot);
	LockRoot.AddNode(dav::D::href).SetValue(sRequestPath);

	// set the Lock-Token response header
	HTTP.Response.Headers.Set(dav::lock_token, kFormat("<{}>", sLockToken));

	HTTP.SetStatus(200);

} // Lock

//-----------------------------------------------------------------------------
void KWebDAV::Unlock(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// we accept any lock token and return success
	const auto& sLockToken = HTTP.Request.Headers.Get(dav::lock_token);

	if (sLockToken.empty())
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing Lock-Token header" };
	}

	HTTP.SetStatus(204);

} // Unlock

//-----------------------------------------------------------------------------
void KWebDAV::Serve(KRESTServer& HTTP,
                    KStringView  sDocumentRoot,
                    KStringView  sRequestPath,
                    KStringView  sRoute,
                    const KWebServerPermissions& Permissions,
                    KStringView  sUser)
//-----------------------------------------------------------------------------
{
	// advertise DAV Class 2 compliance on all WebDAV responses
	HTTP.Response.Headers.Set(KHTTPHeader::DAV, dav::compliance);

	switch (HTTP.RequestPath.Method)
	{
		case KHTTPMethod::PROPFIND:
			Propfind(HTTP, sDocumentRoot, sRequestPath, sRoute);
			break;

		case KHTTPMethod::PROPPATCH:
			Proppatch(HTTP, sDocumentRoot, sRequestPath, sRoute);
			break;

		case KHTTPMethod::MKCOL:
			Mkcol(HTTP, sDocumentRoot, sRequestPath, sRoute);
			break;

		case KHTTPMethod::COPY:
			CopyOrMove(HTTP, sDocumentRoot, sRequestPath, sRoute, Permissions, sUser, false);
			break;

		case KHTTPMethod::MOVE:
			CopyOrMove(HTTP, sDocumentRoot, sRequestPath, sRoute, Permissions, sUser, true);
			break;

		case KHTTPMethod::DELETE:
			Delete(HTTP, sDocumentRoot, sRequestPath, sRoute);
			break;

		case KHTTPMethod::OPTIONS:
			Options(HTTP);
			break;

		case KHTTPMethod::LOCK:
			Lock(HTTP, sDocumentRoot, sRequestPath, sRoute);
			break;

		case KHTTPMethod::UNLOCK:
			Unlock(HTTP);
			break;

		default:
			throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("method {} not handled by WebDAV", HTTP.RequestPath.Method) };
	}

} // Serve

DEKAF2_NAMESPACE_END
