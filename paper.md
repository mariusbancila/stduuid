| | |
| --- | --- |
| Doc. No.: | P0959 |
| Date: | 2018-02-12 |
| Reply to: | Marius Bancila |
| Audience: | Library WG |
| Title: | A Proposal for a Universally Unique Identifier Library |

# A Proposal for a Universally Unique Identifier Library

## I. Motivation

Universally unique identifiers (*uuid*), also known as Globally Unique Identifiers (*guid*s), are commonly used in many types of applications to uniquely identify data. A standard uuid library would benefit developers that currently have to either use operating system specific APIs for creating new uuids or resort to 3rd party libraries, such as *boost::uuid*.

UUIDs are 128-bit numbers that are for most practical purposes unique, without depending on a central registration authority for ensuring their uniqueness. Although the probability of UUID duplication exists, it is negligible. According to Wikipedia, "*for there to be a one in a billion chance of duplication, 103 trillion version 4 UUIDs must be generated.*" UUID is an Internet Engineering Task Force standard described by RFC 4122.

The library proposed on this paper is a light one: it enables developers to generate random and name-based UUIDs, serialize and deserialize UUIDs to and from strings, validate UUIDs and other common operations.

## II. Impact On the Standard

This proposal is a pure library extension. It does not require changes to any standard classes, functions or headers. It does not require any changes in the core language, and it has been implemented in standard C++ as per ISO/IEC 14882:2017. The implementation is available at https://github.com/mariusbancila/stduuid.

## III. Design Decisions

The proposed library, that should be available in a new header called `<uuid>` in the namespace `std`, provides:
* a class called `uuid` that represents a universally unique identifier
* strongly type enums `uuid_variant` and `uuid_version` to represent the possible variant and version types of a UUID
* function objects that generate UUIDs, called generators: `basic_uuid_random_generator<T>`, `uuid_random_generator`, `uuid_name_generator`
* string conversion functions `std::to_string()`, `std::to_wstring()` as well as an overloaded `operator<<` for `std::basic_ostream`
* comparison operators `==`, `!=`, `<`
* `std::swap()` overload for `uuid`
* `std::hash<>` specialization for `uuid`

### Default constructor

Creates a nil UUID that has all the bits set to 0 (i.e. `00000000-0000-0000-0000-000000000000`).

```
uuid empty;
auto empty = uuid{};
```

### string_view constructors

Conversion constructors from `std::string_view` and `std::wstring_view` allow to create `uuid` instances from various strings. 

The input argument must have the form `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` where `x` is a hexadecimal digit. Should the argument be of a different format a nil UUID would be created. 

```
uuid id1("47183823-2574-4bfd-b411-99ed177d3e43");

std::wstring str=L"47183823-2574-4bfd-b411-99ed177d3e43";
uuid id2(str);
```

### Iterators constructors

The conversion constructor that takes two forward iterators constructs an `uuid` with the content of the range \[first, last). It requires the range to have exactly 16 elements, otherwise the result is a nil `uuid`. This constructor follows the conventions of other containers in the standard library.

```
std::array<uuid::value_type, 16> arr{{
   0x47, 0x18, 0x38, 0x23,
   0x25, 0x74,
   0x4b, 0xfd,
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
}};
uuid id(std::begin(arr), std::end(arr));
```

```
uuid::value_type arr[16] = {
   0x47, 0x18, 0x38, 0x23,
   0x25, 0x74,
   0x4b, 0xfd,
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 };
uuid id(std::begin(arr), std::end(arr));
```      

### Size

Member function `size()` indicates the number of bytes in the UUID. Because this is a fixed size structure this function always returns 16.
```
uuid id;
assert(id.size() == 16);
```

### Nil

A nil UUID is a special UUID that has all the bits set to 0. Its canonical textual representation is `00000000-0000-0000-0000-000000000000`. Member function `nil()` indicates whether the `uuid` has all the bits set to 0. A nil uuid is created by the default constructor or by the string conversion constructors when failing to parse the input argument.
```
uuid id;
assert(id.nil());
```

### Iterators

Constant and mutable iterators allow direct access to the underlaying `uuid` data. This enables both direct reading and writing of the `uuid` bits. The `uuid` class has both const and non-const `begin()` and `end()` members to return constant and mutable iterators to the first and the one-past-last element of the UUID data. 

```
std::array<uuid::value_type, 16> arr{{
   0x47, 0x18, 0x38, 0x23,
   0x25, 0x74,
   0x4b, 0xfd,
   0xb4, 0x11,
   0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
}};

uuid id;
assert(id.nil());

std::copy(std::cbegin(arr), std::cend(arr), std::begin(id));
assert(!id.nil());
assert(id.string() == "47183823-2574-4bfd-b411-99ed177d3e43");

size_t i = 0;
for (auto const & b : id)
   assert(arr[i++] == b);
```

Because the internal representation may not be a straightforward array of bytes and may have arbitrary endianness iterators are not defined as pointers.

### `variant` and `version`

Member functions `variant()` and `version()` allow checingk the variant type of the uuid and respetively the version type. These are defined by two strongly typed enums called `uuid_variant` and `uuid_version`.

```
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");
assert(id.version() == uuid_version::random_number_based);
assert(id.variant() == uuid_variant::rfc);
```

### Swapping

Both member and non-member `swap()` functions are available to perform the swapping of `uuid` values.

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

### String conversion

Non-member functions `to_string()` and `to_wstring()` return a string with the UUID formatted to the canonical textual representation `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`, where `x` is a lower case hexadecimal digit.

```
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");
assert(to_string(id) == "47183823-2574-4bfd-b411-99ed177d3e43");
assert(to_wstring(id) == L"47183823-2574-4bfd-b411-99ed177d3e43");
```

### `operator==` and `operator!=`

Non-member `operators ==` and `operator !=` are provided in order to test the equality/inequality of two `uuid` values.

```
uuid empty;
uuid id("47183823-2574-4bfd-b411-99ed177d3e43");

assert(empty == empty);
assert(id == id);
assert(empty != id);
```

### `operator<`

Although it does not make sense to check whether a UUID is less or less or equal then another UUID, the overloading of this operator for `uuid` is necessary in order to be able to store `uuid` values in containers such as `std::set` that by default use `operator <` to compare keys.

```
std::set<std::uuid> ids{
   uuid{},
   uuid("47183823-2574-4bfd-b411-99ed177d3e43")
};

assert(ids.size() == 2);
assert(ids.find(uuid{}) != ids.end());
```

### Hashing

A `std::hash<>` specialization for `uuid` is provided in order to enable the use of `uuid`s in associative unordered containers such as `std::unordered_set`.

```
std::unordered_set<uuid> ids{
   uuid{},
   uuid("47183823-2574-4bfd-b411-99ed177d3e43"),
};

assert(ids.size() == 2);
assert(ids.find(uuid{}) != ids.end());
```

### Generating new uuids
Several function objects, called generators, are provided in order to create different versions of UUIDs.

Examples for generating new UUIDs with the `basic_uuid_random_generator` class:
```
{
  basic_uuid_random_generator<std::mt19937> dgen;
  auto id1 = dgen();
  assert(!id1.nil());
  assert(id1.size() == 16);
  assert(id1.version() == uuid_version::random_number_based);
  assert(id1.variant() == uuid_variant::rfc);
}
{
  basic_uuid_random_generator<std::ranlux48_base> dgen;
  auto id1 = dgen();
  assert(!id1.nil());
  assert(id1.size() == 16);
  assert(id1.version() == uuid_version::random_number_based);
  assert(id1.variant() == uuid_variant::rfc);
}
{
  std::random_device rd;
  std::ranlux48_base generator(rd());

  basic_uuid_random_generator<std::ranlux48_base> dgen(&generator);
  auto id1 = dgen();
  assert(!id1.nil());
  assert(id1.size() == 16);
  assert(id1.version() == uuid_version::random_number_based);
  assert(id1.variant() == uuid_variant::rfc);
}
{
  std::random_device rd;
  auto generator = std::make_unique<std::mt19937>(rd());

  basic_uuid_random_generator<std::mt19937> dgen(generator.get());
  auto id1 = dgen();
  assert(!id1.nil());
  assert(id1.size() == 16);
  assert(id1.version() == uuid_version::random_number_based);
  assert(id1.variant() == uuid_variant::rfc);      
}
```
Examples for generating new UUIDs with the `uuid_random_generator` type alias:
```
uuid_random_generator dgen;
auto id1 = dgen();
assert(!id1.nil());
assert(id1.size() == 16);
assert(id1.version() == uuid_version::random_number_based);
assert(id1.variant() == uuid_variant::rfc);
```      
Examples for genearting new UUIDs with the `uuid_name_generator` class:
```
uuid_name_generator dgen(uuid{"415ccc2b-f5cf-4ec1-b544-45132a518cc8");
auto id1 = dgen("john");
assert(!id1.nil());
assert(id1.size() == 16);
assert(id1.version() == uuid_version::name_based_sha1);
assert(id1.variant() == uuid_variant::rfc);

auto id2 = dgen("jane");
assert(!id2.nil());
assert(id2.size() == 16);
assert(id2.version() == uuid_version::name_based_sha1);
assert(id2.variant() == uuid_variant::rfc);

auto id3 = dgen("jane");
assert(!id3.nil());
assert(id3.size() == 16);
assert(id3.version() == uuid_version::name_based_sha1);
assert(id3.variant() == uuid_variant::rfc);

auto id4 = dgen(L"jane");
assert(!id4.nil());
assert(id4.size() == 16);
assert(id4.version() == uuid_version::name_based_sha1);
assert(id4.variant() == uuid_variant::rfc);

assert(id1 != id2);
assert(id2 == id3);
assert(id3 != id4);
```

## IV. Technical Specifications

### Header
Add a new header called `<uuid>`.

### `uuid_variant` enum

```
namespace std {
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
namespace std {
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
namespace std {
struct uuid
{
public:
  typedef uint8_t               value_type;
  typedef uint8_t&              reference;
  typedef uint8_t const&        const_reference;
  typedef std::size_t           size_type;
  typedef std::ptrdiff_t        difference_type;

  typedef /*implementation-defined*/ iterator;
  typedef /*implementation-defined*/ const_iterator;
    
  static constexpr size_t state_size = 16;

public:
  constexpr uuid() noexcept;

  template<typename ForwardIterator>
  explicit uuid(ForwardIterator first, ForwardIterator last);

  explicit uuid(std::string_view str);
  explicit uuid(std::wstring_view str);

  constexpr uuid_variant variant() const noexcept;
  constexpr uuid_version version() const noexcept;
  constexpr std::size_t size() const noexcept;
  constexpr bool nil() const noexcept;

  void swap(uuid & other) noexcept;

  iterator begin() noexcept;
  const_iterator begin() const noexcept;
  iterator end() noexcept;
  const_iterator end() const noexcept;

private:
  friend bool operator==(uuid const & lhs, uuid const & rhs) noexcept;
  friend bool operator<(uuid const & lhs, uuid const & rhs) noexcept;

  template <class Elem, class Traits>
  friend std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id);
};
```

### non-member functions

```
namespace std {
   inline bool operator== (uuid const& lhs, uuid const& rhs) noexcept;
   inline bool operator!= (uuid const& lhs, uuid const& rhs) noexcept;
   inline bool operator< (uuid const& lhs, uuid const& rhs) noexcept;
   
   inline void swap(uuids::uuid & lhs, uuids::uuid & rhs);
   
   template <class Elem, class Traits>
   std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id);

   std::string to_string(uuid const & id);
   std::wstring to_wstring(uuid const & id);
}
```

### Generators
`basic_uuid_random_generator<T>` is a class template for generating random or pseudo-random UUIDs (version 4, i.e. `uuid_version::random_number_based`). The type template parameter represents a function object that implements both the [`RandomNumberEngine`](http://en.cppreference.com/w/cpp/concept/UniformRandomBitGenerator) and [`UniformRandomBitGenerator`](http://en.cppreference.com/w/cpp/concept/RandomNumberEngine) concepts. `basic_uuid_random_generator` can be either default constructed or constructed with a reference or pointer to a an objects that satisfies the `UniformRandomNumberGenerator` requirements.
```
template <typename UniformRandomNumberGenerator>
class basic_uuid_random_generator 
{
public:
  typedef uuid result_type;

  basic_uuid_random_generator();
  explicit basic_uuid_random_generator(UniformRandomNumberGenerator& gen);
  explicit basic_uuid_random_generator(UniformRandomNumberGenerator* gen);

  uuid operator()();
};
```
A type alias `uuid_random_generator` is provided for convenience as `std::mt19937` is probably the preferred choice of a pesudo-random number generator engine in most cases.
```
using uuid_random_generator = basic_uuid_random_generator<std::mt19937>;
```
`uuid_name_generator` is a function object that generates new UUIDs from a name. It has to be initialized with another UUID and has overloaded `operator()` for both `std::string_view` and `std::wstring_view`.
```
class uuid_name_generator
{
public:
  typedef uuid result_type;

  explicit uuid_name_generator(uuid const& namespace_uuid) noexcept;
  
  uuid operator()(std::string_view name);
  uuid operator()(std::wstring_view name);
};
```

### Specialization
The template specializations of `std::hash` for the `uuid` class allow users to obtain hashes of UUIDs. 
```
namespace std {
   template <>
   struct hash<uuid>
   {
      typedef uuid argument_type;
      typedef std::size_t result_type;

      result_type operator()(argument_type const &uuid) const;
   };
}
```

## V. References

* [1] Universally unique identifier, https://en.wikipedia.org/wiki/Universally_unique_identifier
* [2] A Universally Unique IDentifier (UUID) URN Namespace, https://tools.ietf.org/html/rfc4122
* [3] Boost Uuid Library http://www.boost.org/doc/libs/1_65_1/libs/uuid/uuid.html
* [4] crossguid - Lightweight cross platform C++ GUID/UUID library, https://github.com/graeme-hill/crossguid
