#include "uuid.h"

#ifdef _WIN32
#include <objbase.h>
#elif defined(__linux__) || defined(__unix__)
#include <uuid/uuid.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CFUUID.h>
#endif

namespace 
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

namespace uuids
{
   uuid::uuid(std::string const & str)
   {
      create(str.c_str(), str.size());
   }

   uuid::uuid(std::wstring const & str)
   {
      create(str.c_str(), str.size());
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

         if (index >= 16 || !is_hex(str[i]))
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
            data[index++] = hexpair2char(digit, str[i]);
            firstdigit = true;
         }
      }

      if (index < 16)
      {
         std::fill(std::begin(data), std::end(data), 0);
      }
   }

   uuid make_uuid()
   {
#ifdef _WIN32

      GUID newId;
      ::CoCreateGuid(&newId);

      std::array<unsigned char, 16> bytes =
      {{
         (unsigned char)((newId.Data1 >> 24) & 0xFF),
         (unsigned char)((newId.Data1 >> 16) & 0xFF),
         (unsigned char)((newId.Data1 >> 8) & 0xFF),
         (unsigned char)((newId.Data1) & 0xff),

         (unsigned char)((newId.Data2 >> 8) & 0xFF),
         (unsigned char)((newId.Data2) & 0xff),

         (unsigned char)((newId.Data3 >> 8) & 0xFF),
         (unsigned char)((newId.Data3) & 0xFF),

         (unsigned char)newId.Data4[0],
         (unsigned char)newId.Data4[1],
         (unsigned char)newId.Data4[2],
         (unsigned char)newId.Data4[3],
         (unsigned char)newId.Data4[4],
         (unsigned char)newId.Data4[5],
         (unsigned char)newId.Data4[6],
         (unsigned char)newId.Data4[7]
      }};

      return uuid{ bytes };

#elif defined(__linux__) || defined(__unix__)

      uuid_t id;
      uuid_generate(id);
      return uuid{ id };

#elif defined(__APPLE__)
      auto newId = CFUUIDCreate(NULL);
      auto bytes = CFUUIDGetUUIDBytes(newId);
      CFRelease(newId);

      std::array<unsigned char, 16> byteArray =
      {{
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
      }};
      return uuid{ byteArray };
#elif
      return uuid{};
#endif
   }
}

namespace std
{
   template <>
   void swap(uuids::uuid & lhs, uuids::uuid & rhs)
   {
      lhs.swap(rhs);
   }
}
