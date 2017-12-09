# stduuid
A C++ cross-platform implementation **for universally unique identifiers**, simply know as either UUID or GUID (mostly on Windows). A UUID is a 128-bit number used to uniquely identify information in computer systems, such as database table keys, COM interfaces, classes and type libraries, and many others.

For information about UUID/GUIDs see:
* [Universally unique identifier](https://en.wikipedia.org/wiki/Universally_unique_identifier)
* [A Universally Unique IDentifier (UUID) URN Namespace](https://www.ietf.org/rfc/rfc4122.txt)

## Library overview
The library defines a namespace `uuids` with the following types and functions:
* `uuid` is a class representing a UUID; this can be default constructed (a nil UUID), constructed from an array of bytes, or from a string.
sentation (in lowercase)
* `uuid_variant` is a strongly type enum representing the type of a UUID
* `uuid_version` is a strongly type enum representing the version of a UUID
* `make_uuid` is a parameterless function that creates a new UUID using the typical platform-specific method to create one (`CoCreateGuid` on Windows, `uuid_generate` on Linux, `CFUUIDCreate` on Mac).
* `operator==` and `operator!=` for UUIDs comparison
* `operator<<` to write a UUID to an output string using the standard repre

This project is currently under development and should be ignored until further notice.

## Using the library
The following is a list of examples for using the library:
* Creating a nil UUID
```
uuid empty;
assert(empty.is_nil());
assert(empty.size() == 16);
```
* Creating a new UUID
```
uuid const guid = uuids::make_uuid();
assert(!guid.is_nil());
assert(guid.size() == 16);
assert(guid.version() == uuids::uuid_version::random_number_based);
assert(guid.variant() == uuids::uuid_variant::rfc);
```
* Create a UUID from a string
```
using namespace std::string_literals;

auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
uuid guid(str);
assert(guid.string() == str);
```
or
```
auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
uuid guid(str);
assert(guid.wstring() == str);      
```
* Comparing UUIDS
```
uuid empty;
uuid guid = uuids::make_uuid();

assert(empty == empty);
assert(guid == guid);
assert(empty != guid);
```
* Swapping UUIDS
```
uuid empty;
uuid guid = uuids::make_uuid();

assert(empty.is_nil());
assert(!guid.is_nil());

std::swap(empty, guid);

assert(!empty.is_nil());
assert(guid.is_nil());

empty.swap(guid);

assert(empty.is_nil());
assert(!guid.is_nil());
```
* Convert to string
```
uuid empty;
assert(empty.string() == "00000000-0000-0000-0000-000000000000");
assert(empty.wstring() == L"00000000-0000-0000-0000-000000000000");
```
