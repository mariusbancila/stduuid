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
* `operator==` and `operator!=` for UUIDs comparison for equality/inequality
* `operator<` for comparing whether one UUIDs is less than another. Although this operation does not make much logical sense, it is necessary in order to store UUIDs in a std::set.
* `operator<<` to write a UUID to an output string using the standard repre
* `std::swap<>` specialization for `uuid`
* `std::hash<>` specialization for `uuid`, necessary for storing UUIDs in unordered associative containers, such as `std::unordered_set`

This project is currently under development and should be ignored until further notice.

## Using the library
The following is a list of examples for using the library:
* Creating a nil UUID
```
uuid empty;
assert(empty.nil());
assert(empty.size() == 16);
```
* Creating a new UUID
```
uuid const guid = uuids::make_uuid();
assert(!guid.nil());
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

assert(empty.nil());
assert(!guid.nil());

std::swap(empty, guid);

assert(!empty.nil());
assert(guid.nil());

empty.swap(guid);

assert(empty.nil());
assert(!guid.nil());
```
* Converting to string
```
uuid empty;
assert(empty.string() == "00000000-0000-0000-0000-000000000000");
assert(empty.wstring() == L"00000000-0000-0000-0000-000000000000");
```
* Using with an orderered associative container
```
std::set<uuids::uuid> ids{
   uuid{},
   uuids::make_uuid(),
   uuids::make_uuid(),
   uuids::make_uuid(),
   uuids::make_uuid()
};

assert(ids.size() == 5);
assert(ids.find(uuid{}) != ids.end());
```
* Using in an unordered associative container
```
std::unordered_set<uuids::uuid> ids{
   uuid{},
   uuids::make_uuid(),
   uuids::make_uuid(),
   uuids::make_uuid(),
   uuids::make_uuid()
};

assert(ids.size() == 5);
assert(ids.find(uuid{}) != ids.end());
```
* Hashing UUIDs
```
auto h1 = std::hash<std::string>{};
auto h2 = std::hash<uuid>{};
assert(h1(str) == h2(guid));
```

## Limitations
The library can only create new uuids using the underlaying operating system resources. 

An alternative to this library could be the [boost::uuid](http://www.boost.org/doc/libs/1_65_1/libs/uuid/) library. This has a similar model, but supports creating all variant of uuids, including md5 and sha1 name based, time based, and random number based values.

## Support
The library is supported on all major operating systems: Windows, Linux and Mac OS.

## Testing
A testing project is available in the sources. To build and execute the tests do the following:
* Clone or download this repository
* Create a `build` directory in the root directory of the sources
* Run the command `cmake ..` from the `build` directory; if you do not have CMake you must install it first.
* Build the project created in the previous step
* Run the executable.
