#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <array>
#include <string_view>
#include <iterator>
#include <random>
#include <memory>

#ifdef _WIN32
#include <objbase.h>
#elif defined(__linux__) || defined(__unix__)
#include <uuid/uuid.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CFUUID.h>
#endif

namespace uuids
{
   // UUID format https://tools.ietf.org/html/rfc4122
   // Field	                     NDR Data Type	   Octet #	Note
   // --------------------------------------------------------------------------------------------------------------------------
   // time_low	                  unsigned long	   0 - 3	   The low field of the timestamp.
   // time_mid	                  unsigned short	   4 - 5	   The middle field of the timestamp.
   // time_hi_and_version	      unsigned short	   6 - 7	   The high field of the timestamp multiplexed with the version number.
   // clock_seq_hi_and_reserved	unsigned small	   8	      The high field of the clock sequence multiplexed with the variant.
   // clock_seq_low	            unsigned small	   9	      The low field of the clock sequence.
   // node	                     character	      10 - 15	The spatially unique node identifier.
   // --------------------------------------------------------------------------------------------------------------------------
   // 0                   1                   2                   3
   //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |                          time_low                             |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |       time_mid                |         time_hi_and_version   |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |                         node (2-5)                            |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   // indicated by a bit pattern in octet 8, marked with N in xxxxxxxx-xxxx-xxxx-Nxxx-xxxxxxxxxxxx
   enum class uuid_variant
   {
      // NCS backward compatibility (with the obsolete Apollo Network Computing System 1.5 UUID format)
      // N bit pattern: 0xxx
      // > the first 6 octets of the UUID are a 48-bit timestamp (the number of 4 microsecond units of time since 1 Jan 1980 UTC);
      // > the next 2 octets are reserved;
      // > the next octet is the "address family"; 
      // > the final 7 octets are a 56-bit host ID in the form specified by the address family
      ncs,
      
      // RFC 4122/DCE 1.1 
      // N bit pattern: 10xx
      // > big-endian byte order
      rfc,
      
      // Microsoft Corporation backward compatibility
      // N bit pattern: 110x
      // > little endian byte order
      // > formely used in the Component Object Model (COM) library      
      microsoft,
      
      // reserved for possible future definition
      // N bit pattern: 111x      
      reserved
   };

   // indicated by a bit pattern in octet 6, marked with M in xxxxxxxx-xxxx-Mxxx-xxxx-xxxxxxxxxxxx
   enum class uuid_version
   {
      none = 0, // only possible for nil or invalid uuids
      time_based = 1,  // The time-based version specified in RFC 4122
      dce_security = 2,  // DCE Security version, with embedded POSIX UIDs.
      name_based_md5 = 3,  // The name-based version specified in RFS 4122 with MD5 hashing
      random_number_based = 4,  // The randomly or pseudo-randomly generated version specified in RFS 4122
      name_based_sha1 = 5   // The name-based version specified in RFS 4122 with SHA1 hashing
   };

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
      constexpr uuid() noexcept {}

      template<typename ForwardIterator>
      explicit uuid(ForwardIterator first, ForwardIterator last)
      {
         if (std::distance(first, last) == 16)
            std::copy(first, last, std::begin(data));
      }

      explicit uuid(std::string_view str);
      explicit uuid(std::wstring_view str);

      constexpr uuid_variant variant() const noexcept
      {
         if ((data[8] & 0x80) == 0x00)
            return uuid_variant::ncs;
         else if ((data[8] & 0xC0) == 0x80)
            return uuid_variant::rfc;
         else if ((data[8] & 0xE0) == 0xC0)
            return uuid_variant::microsoft;
         else
            return uuid_variant::reserved;
      }

      constexpr uuid_version version() const noexcept
      {
         if ((data[6] & 0xF0) == 0x10)
            return uuid_version::time_based;
         else if ((data[6] & 0xF0) == 0x20)
            return uuid_version::dce_security;
         else if ((data[6] & 0xF0) == 0x30)
            return uuid_version::name_based_md5;
         else if ((data[6] & 0xF0) == 0x40)
            return uuid_version::random_number_based;
         else if ((data[6] & 0xF0) == 0x50)
            return uuid_version::name_based_sha1;
         else
            return uuid_version::none;
      }

      constexpr std::size_t size() const noexcept { return 16; }

      constexpr bool nil() const noexcept
      {
         for (size_t i = 0; i < data.size(); ++i) if (data[i] != 0) return false;
         return true;
      }

      void swap(uuid & other) noexcept
      {
         data.swap(other.data);
      }

      friend void swap(uuid& lhs, uuid& rhs) noexcept
      {
         std::swap(lhs.data, rhs.data);
      }

      iterator begin() noexcept { return &data[0]; }
      const_iterator begin() const noexcept { return &data[0]; }
      iterator end() noexcept { return &data[0] + size(); }
      const_iterator end() const noexcept { return &data[0] + size(); }

   private:
      std::array<uint8_t, 16> data{ { 0 } };

      friend bool operator==(uuid const & lhs, uuid const & rhs) noexcept;
      friend bool operator<(uuid const & lhs, uuid const & rhs) noexcept;

      template <typename TChar>
      void create(TChar const * const str, size_t const size);

      template <class Elem, class Traits>
      friend std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id);
   };

   inline bool operator== (uuid const& lhs, uuid const& rhs) noexcept
   {
      return lhs.data == rhs.data;
   }

   inline bool operator!= (uuid const& lhs, uuid const& rhs) noexcept
   {
      return !(lhs == rhs);
   }

   inline bool operator< (uuid const& lhs, uuid const& rhs) noexcept
   {
      return lhs.data < rhs.data;
   }

   template <class Elem, class Traits>
   std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id)
   {
      return s << std::hex << std::setfill(static_cast<Elem>('0'))
         << std::setw(2) << (int)id.data[0]
         << std::setw(2) << (int)id.data[1]
         << std::setw(2) << (int)id.data[2]
         << std::setw(2) << (int)id.data[3]
         << '-'
         << std::setw(2) << (int)id.data[4]
         << std::setw(2) << (int)id.data[5]
         << '-'
         << std::setw(2) << (int)id.data[6]
         << std::setw(2) << (int)id.data[7]
         << '-'
         << std::setw(2) << (int)id.data[8]
         << std::setw(2) << (int)id.data[9]
         << '-'
         << std::setw(2) << (int)id.data[10]
         << std::setw(2) << (int)id.data[11]
         << std::setw(2) << (int)id.data[12]
         << std::setw(2) << (int)id.data[13]
         << std::setw(2) << (int)id.data[14]
         << std::setw(2) << (int)id.data[15];
   }

   std::string to_string(uuid const & id)
   {
      std::stringstream sstr;
      sstr << id;
      return sstr.str();
   }

   std::wstring to_wstring(uuid const & id)
   {
      std::wstringstream sstr;
      sstr << id;
      return sstr.str();
   }

   class uuid_default_generator
   {
   public:
      typedef uuid result_type;

      uuid operator()()
      {
#ifdef _WIN32

         GUID newId;
         ::CoCreateGuid(&newId);

         std::array<uint8_t, 16> bytes =
         { {
               (unsigned char)((newId.Data1 >> 24) & 0xFF),
               (unsigned char)((newId.Data1 >> 16) & 0xFF),
               (unsigned char)((newId.Data1 >> 8) & 0xFF),
               (unsigned char)((newId.Data1) & 0xFF),

               (unsigned char)((newId.Data2 >> 8) & 0xFF),
               (unsigned char)((newId.Data2) & 0xFF),

               (unsigned char)((newId.Data3 >> 8) & 0xFF),
               (unsigned char)((newId.Data3) & 0xFF),

               newId.Data4[0],
               newId.Data4[1],
               newId.Data4[2],
               newId.Data4[3],
               newId.Data4[4],
               newId.Data4[5],
               newId.Data4[6],
               newId.Data4[7]
            } };

         return uuid{ std::begin(bytes), std::end(bytes) };

#elif defined(__linux__) || defined(__unix__)

         uuid_t id;
         uuid_generate(id);

         std::array<uint8_t, 16> bytes =
         { {
               id[0],
               id[1],
               id[2],
               id[3],
               id[4],
               id[5],
               id[6],
               id[7],
               id[8],
               id[9],
               id[10],
               id[11],
               id[12],
               id[13],
               id[14],
               id[15]
            } };

         return uuid{ std::begin(bytes), std::end(bytes) };

#elif defined(__APPLE__)
         auto newId = CFUUIDCreate(NULL);
         auto bytes = CFUUIDGetUUIDBytes(newId);
         CFRelease(newId);

         std::array<uint8_t, 16> bytes =
         { {
               bytes.byte0,
               bytes.byte1,
               bytes.byte2,
               bytes.byte3,
               bytes.byte4,
               bytes.byte5,
               bytes.byte6,
               bytes.byte7,
               bytes.byte8,
               bytes.byte9,
               bytes.byte10,
               bytes.byte11,
               bytes.byte12,
               bytes.byte13,
               bytes.byte14,
               bytes.byte15
            } };
         return uuid{ std::begin(bytes), std::end(bytes) };
#elif
         return uuid{};
#endif
      }
   };

   template <typename UniformRandomNumberGenerator>
   class basic_uuid_random_generator 
   {
   public:
      typedef uuid result_type;

      basic_uuid_random_generator()
         :generator(new UniformRandomNumberGenerator)
      {
         std::random_device rd;
         generator->seed(rd());
      }

      explicit basic_uuid_random_generator(UniformRandomNumberGenerator& gen) :
         generator(&gen, [](auto) {}) {}
      explicit basic_uuid_random_generator(UniformRandomNumberGenerator* gen) :
         generator(gen, [](auto) {}) {}

      uuid operator()()
      {
         uint8_t bytes[16];
         for (int i = 0; i < 16; i += 4)
            *reinterpret_cast<uint32_t*>(bytes + i) = distribution(*generator);

         // variant must be 10xxxxxx
         bytes[8] &= 0xBF;
         bytes[8] |= 0x80;

         // version must be 0100xxxx
         bytes[6] &= 0x4F;
         bytes[6] |= 0x40;

         return uuid{std::begin(bytes), std::end(bytes)};
      }

   private:
      std::uniform_int_distribution<uint32_t>  distribution;
      std::shared_ptr<UniformRandomNumberGenerator> generator;
   };

   using uuid_random_generator = basic_uuid_random_generator<std::mt19937>;

   namespace detail
   {
      template <typename TChar>
      constexpr inline unsigned char hex2char(TChar const ch)
      {
         if (ch >= static_cast<TChar>('0') && ch <= static_cast<TChar>('9'))
            return ch - static_cast<TChar>('0');
         if (ch >= static_cast<TChar>('a') && ch <= static_cast<TChar>('f'))
            return 10 + ch - static_cast<TChar>('a');
         if (ch >= static_cast<TChar>('A') && ch <= static_cast<TChar>('F'))
            return 10 + ch - static_cast<TChar>('A');
         return 0;
      }

      template <typename TChar>
      constexpr inline bool is_hex(TChar const ch)
      {
         return
            (ch >= static_cast<TChar>('0') && ch <= static_cast<TChar>('9')) ||
            (ch >= static_cast<TChar>('a') && ch <= static_cast<TChar>('f')) ||
            (ch >= static_cast<TChar>('A') && ch <= static_cast<TChar>('F'));
      }

      template <typename TChar>
      constexpr inline unsigned char hexpair2char(TChar const a, TChar const b)
      {
         return (hex2char(a) << 4) | hex2char(b);
      }
   }

   uuid::uuid(std::string_view str)
   {
      create(str.data(), str.size());
   }

   uuid::uuid(std::wstring_view str)
   {
      create(str.data(), str.size());
   }

   template <typename TChar>
   void uuid::create(TChar const * const str, size_t const size)
   {
      TChar digit = 0;
      bool firstdigit = true;
      size_t index = 0;

      for (size_t i = 0; i < size; ++i)
      {
         if (str[i] == static_cast<TChar>('-')) continue;

         if (index >= 16 || !detail::is_hex(str[i]))
         {
            std::fill(std::begin(data), std::end(data), 0);
            return;
         }

         if (firstdigit)
         {
            digit = str[i];
            firstdigit = false;
         }
         else
         {
            data[index++] = detail::hexpair2char(digit, str[i]);
            firstdigit = true;
         }
      }

      if (index < 16)
      {
         std::fill(std::begin(data), std::end(data), 0);
      }
   }
}

namespace std
{
   template <>
   void swap(uuids::uuid & lhs, uuids::uuid & rhs)
   {
      lhs.swap(rhs);
   }

   template <>
   struct hash<uuids::uuid>
   {
      typedef uuids::uuid argument_type;
      typedef std::size_t result_type;

      result_type operator()(argument_type const &uuid) const
      {
         std::hash<std::string> hasher;
         return static_cast<result_type>(hasher(uuids::to_string(uuid)));
      }
   };
}