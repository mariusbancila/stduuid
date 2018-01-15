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

   uuid uuid_default_generator::operator()()
   {
#ifdef _WIN32

      GUID newId;
      ::CoCreateGuid(&newId);

      std::array<uint8_t, 16> bytes =
      {{
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
      }};

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
         }};

      return uuid { std::begin(bytes), std::end(bytes) };

#elif defined(__APPLE__)
      auto newId = CFUUIDCreate(NULL);
      auto bytes = CFUUIDGetUUIDBytes(newId);
      CFRelease(newId);

      std::array<uint8_t, 16> bytes =
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
      return uuid{ std::begin(bytes), std::end(bytes) };
#elif
      return uuid{};
#endif
   }

   basic_uuid_random_generator()
      :generator(new UniformRandomNumberGenerator)
   {
      std::random_device rd;
      auto seed_data = std::array<int, UniformRandomNumberGenerator::state_size> {};
      std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
      std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
      generator->seed(seq);
   }

   explicit basic_uuid_random_generator(UniformRandomNumberGenerator& gen) :
      generator(&gen, []() {}) {}
   explicit basic_uuid_random_generator(UniformRandomNumberGenerator* pGen) :
      generator(&gen, []() {}) {}
}

namespace std
{
   template <>
   void swap(uuids::uuid & lhs, uuids::uuid & rhs)
   {
      lhs.swap(rhs);
   }
}
