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

// kssod_ui.h
//
// HTML rendering for the kssod sample (the html:: / html::ui:: "view" layer).
// kssod.cpp owns the OpenID Connect protocol, routing and authentication and
// calls these renderers; this split keeps the two concerns - OIDC vs. KWebObjects
// - readable on their own. The renderers receive plain data (already-validated
// values, the signed-in user name, store references) and never touch sessions or
// tokens themselves.

#pragma once

#include "kssod_store.h"

#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace dekaf2;

/// the self-service state the account page needs beyond the OIDC claims
struct AccountState
{
	bool        bHasTotp       { false };
	std::size_t iBackupCodes   { 0 };
	bool        bEmailVerified { false };
	bool        bEmailOtp      { false }; ///< email used as the second factor
	bool        bSmtp          { false }; ///< an email relay is configured
};

// --- end-user pages ---
void RenderLanding   (KRESTServer& HTTP);
void RenderLogin     (KRESTServer& HTTP, KStringView sError, bool bShowRecovery = false, uint16_t iStatus = 200);
void RenderTwoFactor (KRESTServer& HTTP, KStringView sPending, KStringView sMethod,
                      KStringView sMsg, bool bError, uint16_t iStatus = 200);
void RenderAccount   (KRESTServer& HTTP, KStringView sUser, bool bAdmin, const KJSON& Claims,
                      KStringView sMsg, bool bError, const AccountState& St, uint16_t iStatus = 200);
void Render2FASetup  (KRESTServer& HTTP, KStringView sUser, bool bAdmin,
                      KStringView sSecret, KStringView sError, uint16_t iStatus = 200);
void RenderBackupCodes(KRESTServer& HTTP, KStringView sUser, bool bAdmin, const std::vector<KString>& Codes);
void RenderInfo      (KRESTServer& HTTP, KStringView sTitle, KStringView sMessage,
                      KStringView sLinkURL = {}, KStringView sLinkText = {}, uint16_t iStatus = 200);
void RenderForgot    (KRESTServer& HTTP, KStringView sMsg, bool bError, uint16_t iStatus = 200);
void RenderReset     (KRESTServer& HTTP, KStringView sToken, KStringView sError, uint16_t iStatus = 200);

// --- admin pages ---
void RenderForbidden (KRESTServer& HTTP, KStringView sUser);
void RenderAdminHome (KRESTServer& HTTP, KStringView sUser);
void RenderSettings  (KRESTServer& HTTP, KStringView sUser, const KssodSettingsStore::Smtp& Smtp,
                      KStringView sMsg, bool bError, uint16_t iStatus = 200);
void RenderUsers     (KRESTServer& HTTP, KStringView sUser, KssodUserStore& Users,
                      KStringView sMsg = {}, bool bError = false, uint16_t iStatus = 200,
                      const KJSON& Prefill = KJSON::object());
void RenderUserDeleteConfirm(KRESTServer& HTTP, KStringView sAdmin, KStringView sTargetUser, KssodUserStore& Users);
void RenderClients   (KRESTServer& HTTP, KStringView sUser, KssodClientStore& Clients,
                      KStringView sMsg = {}, bool bError = false, uint16_t iStatus = 200,
                      const KJSON& Prefill = KJSON::object());
void RenderClientEdit(KRESTServer& HTTP, KStringView sUser, KStringView sClientID,
                      const KJSON& Values, KStringView sMsg = {}, bool bError = false, uint16_t iStatus = 200);
void RenderClientDeleteConfirm(KRESTServer& HTTP, KStringView sAdmin, KStringView sClientID, KssodUserStore& Users);
void RenderClientAccess(KRESTServer& HTTP, KStringView sUser, KStringView sClientID,
                        KssodUserStore& Users, KStringView sMsg = {}, bool bError = false, uint16_t iStatus = 200,
                        const KJSON& Prefill = KJSON::object());
void RenderAccessOverview(KRESTServer& HTTP, KStringView sAdmin, KssodUserStore& Users, KssodClientStore& Clients);
void RenderUserAccess(KRESTServer& HTTP, KStringView sAdmin, KStringView sTargetUser,
                      KssodUserStore& Users, KssodClientStore& Clients,
                      KStringView sMsg = {}, bool bError = false, uint16_t iStatus = 200);
