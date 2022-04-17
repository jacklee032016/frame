

#ifndef __JSON_READER_HPP__
#define __JSON_READER_HPP__

#if !defined(JSON_IS_AMALGAMATION)
#include "jsonFeatures.hpp"
#include "jsonValue.hpp"
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <deque>
#include <iosfwd>
#include <istream>
#include <stack>
#include <string>

// Disable warning C4251: <data member>: <type> needs to have dll-interface to be used by...
#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#pragma pack(push, 8)

namespace Json
{

//#include "iostream"

#define	JSON_TRACE()	std::cout << "::" << __FUNCTION__ << "():Line." << __LINE__<< ": "<< std::endl;

//do{std::cout << "File " << __FILE__ << " Line." << __LINE__ << ":" << std::endl; }while(0)

/** Interface for reading JSON from a char array.
 */
class JSON_API CharReader
{
	public:
		virtual ~CharReader() = default;
		
	  /** \brief Read a Value from a <a HREF="http://www.json.org">JSON</a>
	   * document. The document must be a UTF-8 encoded string containing the
	   * document to read.
	   *
	   * \param      beginDoc Pointer on the beginning of the UTF-8 encoded string
	   *                      of the document to read.
	   * \param      endDoc   Pointer on the end of the UTF-8 encoded string of the
	   *                      document to read. Must be >= beginDoc.
	   * \param[out] root     Contains the root value of the document if it was
	   *                      successfully parsed.
	   * \param[out] errs     Formatted error messages (if not NULL) a user
	   *                      friendly string that lists errors in the parsed
	   *                      document.
	   * \return \c true if the document was successfully parsed, \c false if an
	   * error occurred.
	   */
		virtual bool parse(char const* beginDoc, char const* endDoc, Value* root, String* errs) = 0;

		class JSON_API Factory
		{
			public:
				virtual ~Factory() = default;

				/** \brief Allocate a CharReader via operator new().
				* \throw std::exception if something goes wrong (e.g. invalid settings)	*/
				virtual CharReader* newCharReader() const = 0;
		};
};


/** \brief Build a CharReader implementation.
 *
 * Usage:
 *   \code
 *   using namespace Json;
 *   CharReaderBuilder builder;
 *   builder["collectComments"] = false;
 *   Value value;
 *   String errs;
 *   bool ok = parseFromStream(builder, std::cin, &value, &errs);
 *   \endcode
 */
class JSON_API CharReaderBuilder : public CharReader::Factory
{
	public:
		// Note: We use a Json::Value so that we can add data-members to this class without a major version bump.

		/** Configuration of this builder.
		* These are case-sensitive.
		* Available settings (case-sensitive):
		* - `"collectComments": false or true`
		*   - true to collect comment and allow writing them back during
		*     serialization, false to discard comments.  This parameter is ignored
		*     if allowComments is false.
		* - `"allowComments": false or true`
		*   - true if comments are allowed.
		* - `"strictRoot": false or true`
		*   - true if root must be either an array or an object value
		* - `"allowDroppedNullPlaceholders": false or true`
		*   - true if dropped null placeholders are allowed. (See
		*     StreamWriterBuilder.)
		* - `"allowNumericKeys": false or true`
		*   - true if numeric object keys are allowed.
		* - `"allowSingleQuotes": false or true`
		*   - true if '' are allowed for strings (both keys and values)
		* - `"stackLimit": integer`
		*   - Exceeding stackLimit (recursive depth of `readValue()`) will cause an
		*     exception.
		*   - This is a security issue (seg-faults caused by deeply nested JSON), so
		*     the default is low.
		* - `"failIfExtra": false or true`
		*   - If true, `parse()` returns false when extra non-whitespace trails the
		*     JSON value in the input string.
		* - `"rejectDupKeys": false or true`
		*   - If true, `parse()` returns false when a key is duplicated within an
		*     object.
		* - `"allowSpecialFloats": false or true`
		*   - If true, special float values (NaNs and infinities) are allowed and
		*     their values are lossfree restorable.
		*
		* You can examine 'settings_` yourself to see the defaults. You can also
		* write and read them just like any JSON Value.
		* \sa setDefaults()
		*/
	Json::Value settings_;

	CharReaderBuilder();
	~CharReaderBuilder() override;

	CharReader* newCharReader() const override;

	/** \return true if 'settings' are legal and consistent;
	*   otherwise, indicate bad settings via 'invalid'.
	*/
	bool validate(Json::Value* invalid) const;

	/** A simple way to update a specific setting.*/
	Value& operator[](const String& key);

	/** Called by ctor, but you can use this to reset settings_.
	* \pre 'settings' != NULL (but Json::null is fine)
	* \remark Defaults:
	* \snippet src/lib_json/json_reader.cpp CharReaderBuilderDefaults
	*/
	static void setDefaults(Json::Value* settings);
	/** Same as old Features::strictMode().
	* \pre 'settings' != NULL (but Json::null is fine)
	* \remark Defaults:
	* \snippet src/lib_json/json_reader.cpp CharReaderBuilderStrictMode
	*/
	static void strictMode(Json::Value* settings);
};


/** Consume entire stream and use its begin/end.
* Someday we might have a real StreamReader, but for now this
* is convenient.
*/
bool JSON_API parseFromStream(CharReader::Factory const&, IStream&, Value* root, String* errs);

/** \brief Read from 'sin' into 'root'.
*
* Always keep comments from the input JSON.
*
* This can be used to read a file into a particular sub-object.
* For example:
*   \code
*   Json::Value root;
*   cin >> root["dir"]["file"];
*   cout << root;
*   \endcode
* Result:
* \verbatim
* {
* "dir": {
*    "file": {
*    // The input stream JSON would be nested here.
*    }
* }
* }
* \endverbatim
* \throw std::exception on parse error.
* \see Json::operator<<()
*/
JSON_API IStream& operator>>(IStream&, Value&);

}

#pragma pack(pop)

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#endif

