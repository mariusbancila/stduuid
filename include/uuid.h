#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <array>
#include <string_view>

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
      future
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
      constexpr explicit uuid() {}
      constexpr explicit uuid(std::array<uint8_t, 16> const & bytes) :data{bytes} {}
      explicit uuid(uint8_t const * const bytes)
      {
         std::copy(bytes, bytes + 16, std::begin(data));
      }

      explicit uuid(std::string const & str);
      explicit uuid(std::wstring const & str);

      constexpr uuid_variant variant() const noexcept
      {
         if ((data[8] & 0x80) == 0x00)
            return uuid_variant::ncs;
         else if ((data[8] & 0xC0) == 0x80)
            return uuid_variant::rfc;
         else if ((data[8] & 0xE0) == 0xC0)
            return uuid_variant::microsoft;
         else
            return uuid_variant::future;
      }

      constexpr uuid_version version() const
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

      constexpr bool is_nil() const noexcept
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

      template<class CharT, 
         class Traits = std::char_traits<CharT>,
         class Alloc = std::allocator<CharT>>
         std::basic_string<CharT, Traits, Alloc> string(Alloc const & a = Alloc()) const
      {
         std::basic_stringstream<CharT, Traits, Alloc>  sstr;
         sstr << *this;
         return sstr.str();
      }

      std::string string() const
      {
         std::stringstream sstr;
         sstr << *this;
         return sstr.str();
      }

      std::wstring wstring() const
      {
         std::wstringstream sstr;
         sstr << *this;
         return sstr.str();
      }

   private:
      std::array<uint8_t, 16> data{ 0 };

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

   uuid make_uuid();
}

namespace std
{
   template <>
   void swap(uuids::uuid & lhs, uuids::uuid & rhs);

   template <>
   struct hash<uuids::uuid>
   {
      typedef uuids::uuid argument_type;
      typedef std::size_t result_type;

      result_type operator()(argument_type const &uuid) const
      {
         std::hash<std::string> hasher;
         return static_cast<result_type>(hasher(uuid.string()));
      }
   };
}