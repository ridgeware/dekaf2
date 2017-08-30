
#pragma once

/// @file ksystem.h
/// general system utilities for dekaf2

#include <cstdlib>
#include "kstring.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p sDefault if not found.
const char* kGetEnv (const char* sEnvVar, const char* sDefault = "")
//-----------------------------------------------------------------------------
{
	const char* sValue = ::getenv(sEnvVar);
	if (sValue)
	{
		return (sValue);
	}
	else
	{
		return (sDefault);
	}

} // kGetEnv

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p sDefault if not found.
const char* kGetEnv (const KString& sEnvVar, const char* sDefault = "")
//-----------------------------------------------------------------------------
{
	return kGetEnv(sEnvVar.c_str(), sDefault);
}

//-----------------------------------------------------------------------------
/// Set environment variable.
void kSetEnv (const char* sEnvVar, const char* sValue)
//-----------------------------------------------------------------------------
{
	if (!::setenv(sEnvVar, sValue, true))
	{
		kWarning("cannot set {} = {}", sEnvVar, sValue);
	}
}

//-----------------------------------------------------------------------------
/// Set environment variable.
void kSetEnv (const KString& sEnvVar, const KString& sValue)
//-----------------------------------------------------------------------------
{
	kSetEnv(sEnvVar.c_str(), sValue.c_str());
}

//-----------------------------------------------------------------------------
/// Unset environment variable.
void kUnsetEnv (const char* sEnvVar)
//-----------------------------------------------------------------------------
{
	if (!::unsetenv(sEnvVar))
	{
		kWarning("cannot unset {}", sEnvVar);
	}
}

//-----------------------------------------------------------------------------
/// Unset environment variable.
void kUnsetEnv (const KString& sEnvVar)
//-----------------------------------------------------------------------------
{
	kUnsetEnv(sEnvVar.c_str());
}

} // end of namespace dekaf2

