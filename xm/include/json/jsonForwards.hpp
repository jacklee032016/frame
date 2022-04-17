
#ifndef __JSON_FORWARDS_HPP__
#define __JSON_FORWARDS_HPP__

#if !defined(JSON_IS_AMALGAMATION)
#include "jsonConfig.hpp"
#endif // if !defined(JSON_IS_AMALGAMATION)

namespace Json {

// jsonWriter.hpp
class StreamWriter;
class StreamWriterBuilder;
class Writer;
class FastWriter;
class StyledWriter;
class StyledStreamWriter;

// jsonReader.hpp
class Reader;
class CharReader;
class CharReaderBuilder;

// json_features.h
class Features;

// jsonValue.hpp
typedef unsigned int ArrayIndex;
class StaticString;
class Path;
class PathArgument;
class Value;
class ValueIteratorBase;
class ValueIterator;
class ValueConstIterator;

} // namespace Json

#endif // JSON_FORWARDS_H_INCLUDED
