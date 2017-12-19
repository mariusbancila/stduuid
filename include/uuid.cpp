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

         if (index >= 16 || !is_hex(str[i]))
         {
            std::fill(std::begin(data), std::end(data), std::byte{ 0 });
            return;
         }

         if (firstdigit)
         {
            digit = str[i];
            firstdigit = false;
         }
         else
         {
            data[index++] = std::byte{ hexpair2char(digit, str[i]) };
            firstdigit = true;
         }
      }

      if (index < 16)
      {
         std::fill(std::begin(data), std::end(data), std::byte{ 0 });
      }
   }

   uuid make_uuid()
   {
#ifdef _WIN32

      GUID newId;
      ::CoCreateGuid(&newId);

      std::array<std::byte, 16> bytes =
      {{
         std::byte{ (unsigned char)((newId.Data1 >> 24) & 0xFF) },
         std::byte{ (unsigned char)((newId.Data1 >> 16) & 0xFF) },
         std::byte{ (unsigned char)((newId.Data1 >> 8) & 0xFF) },
         std::byte{ (unsigned char)((newId.Data1) & 0xFF) },

         std::byte{ (unsigned char)((newId.Data2 >> 8) & 0xFF) },
         std::byte{ (unsigned char)((newId.Data2) & 0xFF) },

         std::byte{ (unsigned char)((newId.Data3 >> 8) & 0xFF) },
         std::byte{ (unsigned char)((newId.Data3) & 0xFF) },

         std::byte{ newId.Data4[0] },
         std::byte{ newId.Data4[1] },
         std::byte{ newId.Data4[2] },
         std::byte{ newId.Data4[3] },
         std::byte{ newId.Data4[4] },
         std::byte{ newId.Data4[5] },
         std::byte{ newId.Data4[6] },
         std::byte{ newId.Data4[7] }
      }};

      return uuid{ std::begin(bytes), std::end(bytes) };

#elif defined(__linux__) || defined(__unix__)

      uuid_t id;
      uuid_generate(id);

      std::array<std::byte, 16> bytes =
      { {
            std::byte{ id[0] },
            std::byte{ id[1] },
            std::byte{ id[2] },
            std::byte{ id[3] },
            std::byte{ id[4] },
            std::byte{ id[5] },
            std::byte{ id[6] },
            std::byte{ id[7] },
            std::byte{ id[8] },
            std::byte{ id[9] },
            std::byte{ id[10] },
            std::byte{ id[11] },
            std::byte{ id[12] },
            std::byte{ id[13] },
            std::byte{ id[14] },
            std::byte{ id[15] }
         }};

      return uuid { std::begin(bytes), std::end(bytes) };

#elif defined(__APPLE__)
      auto newId = CFUUIDCreate(NULL);
      auto bytes = CFUUIDGetUUIDBytes(newId);
      CFRelease(newId);

      std::array<std::byte, 16> bytes =
      {{
            std::byte{ bytes.byte0 },
            std::byte{ bytes.byte1 },
            std::byte{ bytes.byte2 },
            std::byte{ bytes.byte3 },
            std::byte{ bytes.byte4 },
            std::byte{ bytes.byte5 },
            std::byte{ bytes.byte6 },
            std::byte{ bytes.byte7 },
            std::byte{ bytes.byte8 },
            std::byte{ bytes.byte9 },
            std::byte{ bytes.byte10 },
            std::byte{ bytes.byte11 },
            std::byte{ bytes.byte12 },
            std::byte{ bytes.byte13 },
            std::byte{ bytes.byte14 },
            std::byte{ bytes.byte15 }
      }};
      return uuid{ std::begin(bytes), std::end(bytes) };
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
