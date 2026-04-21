/*
 * ApiTags.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting
{

class TagSerializable {};
class TagCopyable {};
class TagRawPointer {};
class TagSharedPointer {};

template<typename Derived>
class ApiTag
{
public:
    using ScriptingApiName = Derived;
};

/// Tag for classes that are accessible via scripting and support serialization to/from scripting values
/// Scripts can create new instances of this class by constructing correctly structured table
/// However, since this class is always represented as a table with its data members, it can't have any callable methods
/// Empty (null/nil) values are not permitted for this type
template<typename Derived>
class ApiSerializable : public ApiTag<Derived>, public TagSerializable
{};

/// Tag for simple classes that can be passed as a copy into scripting system
/// Scripts can not create new instances of this class
/// Empty (null/nil) values are not permitted for this type
template<typename Derived>
class ApiCopyable : public ApiTag<Derived>, public TagCopyable
{};

/// Tag for classes that can be passed as a raw pointer into scripting system
/// Scripts can not create new instances of this class
/// WARNING: may result in garbage pointer if script saves this pointer past the object lifetime!
template<typename Derived>
class ApiRawPointer : public ApiTag<Derived>, public TagRawPointer
{};

/// Tag for classes that can be passed as a shared pointer into scripting system
/// Scripts can not create new instances of this class
/// Object lifetime may be extended till scripting garbage collection
template<typename Derived>
class ApiSharedPointer : public ApiTag<Derived>, public TagSharedPointer
{};

}

VCMI_LIB_NAMESPACE_END
