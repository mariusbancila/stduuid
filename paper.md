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
std::array<uint8_t, 16> arr{{
   0x47, 0x18, 0x38, 0x23, 
   0x25, 0x74, 
   0x4b, 0xfd, 
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43}};
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

### Iterators

Constant and mutable iterators allow direct access to the underlaying `uuid` data. This enables both direct reading and writing of the `uuid` bits.

```
std::array<uint8_t, 16> arr{{
   0x47, 0x18, 0x38, 0x23,
   0x25, 0x74,
   0x4b, 0xfd,
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43}};

uuid id;
assert(id.nil());

std::copy(std::cbegin(arr), std::cend(arr), std::begin(id));
assert(!id.nil());
assert(id.string() == "47183823-2574-4bfd-b411-99ed177d3e43");

size_t i = 0;
for (auto const & b : id)
   assert(arr[i++] == b);
```

### `variant` and `version`

Member functions `variant()` and `version()` allows to check the variant type of the uuid and respetively the version type. These are defined by two strongly typed enums called `uuid_variant` and `uuid_version`.

```
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");
assert(id.version() == uuids::uuid_version::random_number_based);
assert(id.variant() == uuids::uuid_variant::rfc);
```

### Swapping

Both member and non-member swap functions including a `std::swap<>` specialization that enable the swapping of `uuid` values.

```
uuid empty;
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");

assert(empty.nil());
assert(!id.nil());

std::swap(empty, id);

assert(!empty.nil());
assert(id.nil());

empty.swap(id);

assert(empty.nil());
assert(!id.nil());
```

### `operator==` and `operator!=`

Non-member operators == and != are provided in order to test the equality/inequality of two `uuid` values.

```
uuid empty;
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");

assert(empty == empty);
assert(id == id);
assert(empty != id);
```

### `operator<`

Although it does not make sense to check whether a uuid is less or less or equal then another uuid, the overloading of this operator for `uuid` is necessary in order to be able to store `uuid` values in some containers such as `std::set` that by default use the operator to compare keys.

```
std::set<uuids::uuid> ids{
   uuid{},
   uuids::uuid("47183823-2574-4bfd-b411-99ed177d3e43"),
   uuids::make_uuid()
};

assert(ids.size() == 3);
assert(ids.find(uuid{}) != ids.end());
```

### Hashing

A `std::hash<>` specialization for `uuid` is provided in order to enable the use of `uuid`s in associative unordered containers such as `std::unordered_set`.

```
std::unordered_set<uuids::uuid> ids{
   uuid{},
   uuids::uuid("47183823-2574-4bfd-b411-99ed177d3e43"),
   uuids::make_uuid()
};

assert(ids.size() == 3);
assert(ids.find(uuid{}) != ids.end());
```

### Generating new uuids

Non-member `make_uuid` function creates a new uuid by relying on the operating system APIs for this purpose. In practice, all the major operating system APIs (`CoCreateGuid` on Windows, `uuid_generate` on Linux, `CFUUIDCreate` on Max OS) produce version 4 UUIDs of the RFC variant.

```
auto id = make_uuid();
assert(!id.nil());
assert(id.version() == uuids::uuid_version::random_number_based);
assert(id.variant() == uuids::uuid_variant::rfc);
```

## IV. Technical Specifications

### Header
Add a new header called `<uuid>`.

### `uuid_variant` enum

```
namespace std::uuid {
   enum class uuid_variant
   {
      ncs,
      rfc,
      microsoft,
      future
   };
}
```

### `uuid_version` enum

```
namespace std::uuid {
   enum class uuid_version
   {
      none = 0,
      time_based = 1,
      dce_security = 2,
      name_based_md5 = 3,
      random_number_based = 4,
      name_based_sha1 = 5
   };
}
```

### `uuid` class

```
namespace std::uuid {
struct uuid
   {
   public:
      typedef uint8_t         value_type;
      typedef uint8_t&        reference;
      typedef uint8_t const&  const_reference;
      typedef uint8_t*        iterator;
      typedef uint8_t const*  const_iterator;
      typedef std::size_t     size_type;
      typedef std::ptrdiff_t  difference_type;

   public:
      constexpr explicit uuid();
      constexpr explicit uuid(std::array<uint8_t, 16> const & bytes);
      explicit uuid(uint8_t const * const bytes);
      explicit uuid(std::string const & str);
      explicit uuid(std::wstring const & str);

      constexpr uuid_variant variant() const noexcept;
      constexpr uuid_version version() const noexcept;
      constexpr std::size_t size() const noexcept;
      constexpr bool nil() const noexcept;

      void swap(uuid & other) noexcept;
      friend void swap(uuid& lhs, uuid& rhs) noexcept;
      
      iterator begin() noexcept;
      const_iterator begin() const noexcept;
      iterator end() noexcept;
      const_iterator end() const noexcept;

      template<class CharT, 
         class Traits = std::char_traits<CharT>,
         class Alloc = std::allocator<CharT>>
         std::basic_string<CharT, Traits, Alloc> string(Alloc const & a = Alloc()) const;
      std::string string() const;
      std::wstring wstring() const;

   private:
      friend bool operator==(uuid const & lhs, uuid const & rhs) noexcept;
      friend bool operator<(uuid const & lhs, uuid const & rhs) noexcept;
      
      template <class Elem, class Traits>
      friend std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id);
   };
}
```

### non-member functions

```
namespace std::uuid {
   inline bool operator== (uuid const& lhs, uuid const& rhs) noexcept;
   inline bool operator!= (uuid const& lhs, uuid const& rhs) noexcept;
   inline bool operator< (uuid const& lhs, uuid const& rhs) noexcept;
   
   template <class Elem, class Traits>
   std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id);

   uuid make_uuid();
}
```

### Specialization

```
namespace std
{
   template <>
   void swap(uuids::uuid & lhs, uuids::uuid & rhs);

   template <>
   struct hash<uuids::uuid>
   {
      typedef uuids::uuid argument_type;
      typedef std::size_t result_type;

      result_type operator()(argument_type const &uuid) const;
   };
}
```

## V. Limitations

The library does not support creating different versions of UUIDs. Generating version 3 (name-based versiong using MD5 hashing) and version 5 (name-based version using SHA1 hashing) are made impossible by the lack of standard support for MD5 and SHA1 cryptographic hash function. Instead, the library relies on the underlying operating system functionalities for creating UUIDs.

This limitation should be irrelevant for most practical use cases. In the event UUIDs need to be generated with a specific version other than the one provided by the underlying system, users can resort to a 3rd party implementation such as `boost::uuid`.

## VI. References

* [1] Universally unique identifier, https://en.wikipedia.org/wiki/Universally_unique_identifier
* [2] A Universally Unique IDentifier (UUID) URN Namespace, https://tools.ietf.org/html/rfc4122
* [3] Boost Uuid Library http://www.boost.org/doc/libs/1_65_1/libs/uuid/uuid.html
* [4] crossguid - Lightweight cross platform C++ GUID/UUID library, https://github.com/graeme-hill/crossguid
