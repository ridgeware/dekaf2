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

#include "kwebdav.h"
#include "krestserver.h"
#include "kwebserverpermissions.h"
#include "khttperror.h"
#include "kxml.h"
#include "kmime.h"
#include "klog.h"
#include "kformat.h"
#include "kurl.h"
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

// XML element local names (for parsing PROPFIND request body)
constexpr KStringViewZ propfind         = "propfind";
constexpr KStringViewZ allprop          = "allprop";
constexpr KStringViewZ propname         = "propname";
constexpr KStringViewZ prop             = "prop";

// Depth header values
constexpr KStringViewZ infinity         = "infinity";

// namespace declaration
constexpr KStringViewZ xmlns_D          = "xmlns:D";
constexpr KStringViewZ ns_DAV           = "DAV:";

// status line strings
constexpr KStringViewZ status_200       = "HTTP/1.1 200 OK";
constexpr KStringViewZ status_404       = "HTTP/1.1 404 Not Found";

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

	if (!HTTP.Request.HasContent(true))
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
/// parse the Depth header value, returns 0, 1, or a large value for infinity
uint32_t ParseDepth(const KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	const auto& sDepth = HTTP.Request.Headers.Get(KHTTPHeader::DEPTH);

	if (sDepth.empty() || sDepth == dav::infinity)
	{
		return std::numeric_limits<uint32_t>::max();
	}

	if (sDepth == "0") return 0;
	if (sDepth == "1") return 1;

	kDebug(1, "invalid Depth header value: {}", sDepth);

	return 0;

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

		for (const auto& Entry : Dir)
		{
			const auto& sChildPath = Entry.Path();
			// make the child path relative to document root
			KStringView sRelChild = sChildPath;

			if (!sRelChild.remove_prefix(sDocumentRoot))
			{
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
			                       ? KMIME::CreateByExtension(sChildPath).Serialize()
			                       : KString{};

			AddPropfindEntry(Root, sHref, Entry.FileStat(), sChildContentType, PFReq);
		}
	}

	HTTP.SetStatus(KHTTPError::H2xx_MULTISTATUS);

} // Propfind

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
	HTTP.Response.Headers.Set(KHTTPHeader::DAV, "1");
	HTTP.Response.Headers.Set(KHTTPHeader::ALLOW, "OPTIONS,GET,HEAD,PUT,POST,DELETE,PROPFIND,MKCOL,COPY,MOVE");
	HTTP.SetStatus(200);

} // Options

//-----------------------------------------------------------------------------
void KWebDAV::Serve(KRESTServer& HTTP,
                    KStringView  sDocumentRoot,
                    KStringView  sRequestPath,
                    KStringView  sRoute,
                    const KWebServerPermissions& Permissions,
                    KStringView  sUser)
//-----------------------------------------------------------------------------
{
	switch (HTTP.RequestPath.Method)
	{
		case KHTTPMethod::PROPFIND:
			Propfind(HTTP, sDocumentRoot, sRequestPath, sRoute);
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

		default:
			throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("method {} not handled by WebDAV", HTTP.RequestPath.Method) };
	}

} // Serve

DEKAF2_NAMESPACE_END
