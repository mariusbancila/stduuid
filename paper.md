# A Proposal for a Universally Unique Identifier Library

## I. Motivation

Universally unique identifiers (*uuid*), also known as Globally Unique Identifiers (*guid*s), are commonly used in many types of applications to uniquely identify data. A standard uuid library would benefit developers that currently have to either use operating system specific APIs for creating new uuids or resort to 3rd party libraries, such as *boost::uuid*.

UUIDs are 128-bit numbers that are for most practical purposes unique, without depending on a central registration authority for ensuring their uniqueness. Although the probability of UUID duplication exists, it is negligible. According to Wikipedia, "*for there to be a one in a billion chance of duplication, 103 trillion version 4 UUIDs must be generated.*" UUID is an Internet Engineering Task Force standard described by RFC 4122.

The library proposed on this paper is a lite one: it enables developers to generate UUIDs based on the operation system facilities, serialize and deserialize UUIDs to and frmo strings, validate UUIDs and other common ensuing operations.

## II. Impact On the Standard

This proposal is a pure library extension. It does not require changes to any standard classes, functions or headers. It does not require any changes in the core language, and it has been implemented in standard C++ as per ISO/IEC 14882:2017.

## III. Design Decisions

The proposed library, that should be available in a new header called `<uuid>` in the namespace `std::uuids`, provides:
* a class called `uuid` that represents a universally unique identifier
* strongly type enums `uuid_variant` and `uuid_version` to represent the possible variant and version types of a UUID
* a `make_uuid` function to generate a new UUID
* comparison operators `==`, `!=`, `<`
* `std::swap<>` specialization for `uuid`
* `std::hash<>` specialization for `uuid`

### Default constructor

Creates a nil UUID that has all the bits set to 0 (i.e. 00000000-0000-0000-0000-000000000000).

```
uuid empty;
auto empty = uuid{};
```

### String constructors

Conversion constructors from `std::string` and `std::wstring` allow to create `uuid` instances from a string. 

The input argument must have the form `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` where `x` is a hexadecimal digit. Should the argument be of a different format a nil UUID would be created. 

```
uuid id1("47183823-2574-4bfd-b411-99ed177d3e43");

std::wstring str=L"47183823-2574-4bfd-b411-99ed177d3e43";
uuid id2(str);
```

### Array constructors

The conversion constructor from `std::array<uint8_t, 16>` enables the construction of a `uuid` from an array of bytes.

```
std::array<uint8_t, 16> arr{
   0x47, 0x18, 0x38, 0x23, 
   0x25, 0x74, 
   0x4b, 0xfd, 
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43};
uuid id(arr);
```

A conversion constructor from `uint8_t*` also exists to construct `uuid`s from C-like arrays. This requires the input buffer to have at least 16 bytes. The behavior when the buffer does not have 16 bytes is undefined.
```
uint8_t arr[16] = {
   0x47, 0x18, 0x38, 0x23,
   0x25, 0x74,
   0x4b, 0xfd,
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43};
uuid id(arr);
```      

### Capacity

Member function `size()` indicates the number of bytes in the UUID. Because this is a fixed size structure this function always returns 16.

Member function `nil()` indicates whether the `uuid` has all the bits set to 0. A nil uuid is created by the default constructor or by the string conversion constructors when failing to parse the input argument.

```
uuid id;
assert(id.nil());
assert(id.size() == 16);
```

### String conversion

Member functions `string` and `wstring` return a string with the UUID formatted to the canonical textual representation `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`, where `x` is a lower case hexadecimal digit.

```
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");
assert(id.string() == "47183823-2574-4bfd-b411-99ed177d3e43");
assert(id.wstring() == L"47183823-2574-4bfd-b411-99ed177d3e43");
```         

## IV. Technical Specifications

## V. Limitations

## VI. References

* [1] Universally unique identifier, https://en.wikipedia.org/wiki/Universally_unique_identifier
* [2] A Universally Unique IDentifier (UUID) URN Namespace, https://tools.ietf.org/html/rfc4122
* [3] Boost Uuid Library http://www.boost.org/doc/libs/1_65_1/libs/uuid/uuid.html
* [4] crossguid - Lightweight cross platform C++ GUID/UUID library, https://github.com/graeme-hill/crossguid
