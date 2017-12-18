#include "..\include\uuid.h"
#include <assert.h>
#include <iostream>
#include <set>
#include <unordered_set>

int main()
{
   using namespace uuids;

   {
      std::cout << "Test default constructor" << std::endl;

      uuid empty;
      assert(empty.nil());
      assert(empty.size() == 16);
   }

   {
      std::cout << "Test string constructor" << std::endl;

      using namespace std::string_literals;

      {
         auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
         uuid guid(str);
         assert(guid.string() == str);
      }

      {
         uuid guid("47183823-2574-4bfd-b411-99ed177d3e43");
         assert(guid.string() == "47183823-2574-4bfd-b411-99ed177d3e43"s);
      }
   }

   {
      std::cout << "Test wstring constructor" << std::endl;

      using namespace std::string_literals;

      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
      uuid guid(str);
      assert(guid.wstring() == str);
   }

   {
      std::cout << "Test std::array constructor" << std::endl;

      std::array<uint8_t, 16> arr{
         0x47, 0x18, 0x38, 0x23, 
         0x25, 0x74, 
         0x4b, 0xfd, 
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
      };

      using namespace std::string_literals;

      uuid guid(arr);
      assert(guid.string() == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }

   {
      std::cout << "Test array constructor" << std::endl;

      using namespace std::string_literals;

      uint8_t arr[16] = {
         0x47, 0x18, 0x38, 0x23,
         0x25, 0x74,
         0x4b, 0xfd,
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
      };
      uuid guid(arr);
      assert(guid.string() == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }

   {
      std::cout << "Test make" << std::endl;

      uuid const guid = uuids::make_uuid();
      assert(!guid.nil());
      assert(guid.size() == 16);
      assert(guid.version() == uuids::uuid_version::random_number_based);
      assert(guid.variant() == uuids::uuid_variant::rfc);
   }

   {
      std::cout << "Test equality" << std::endl;

      uuid empty;
      uuid guid = uuids::make_uuid();

      assert(empty == empty);
      assert(guid == guid);
      assert(empty != guid);
   }

   {
      std::cout << "Test comparison" << std::endl;

      auto empty = uuid{};
      auto id = make_uuid();

      assert(empty < id);

      std::set<uuids::uuid> ids{
         uuid{},
         uuids::make_uuid(),
         uuids::make_uuid(),
         uuids::make_uuid(),
         uuids::make_uuid()
      };

      assert(ids.size() == 5);
      assert(ids.find(uuid{}) != ids.end());
   }

   {
      std::cout << "Test hashing" << std::endl;

      using namespace std::string_literals;
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
      auto guid = uuid{ str };

      auto h1 = std::hash<std::string>{};
      auto h2 = std::hash<uuid>{};
      assert(h1(str) == h2(guid));

      std::unordered_set<uuids::uuid> ids{
         uuid{},
         uuids::make_uuid(),
         uuids::make_uuid(),
         uuids::make_uuid(),
         uuids::make_uuid()
      };

      assert(ids.size() == 5);
      assert(ids.find(uuid{}) != ids.end());
   }

   {
      std::cout << "Test swap" << std::endl;

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
   }

   {
      std::cout << "Test string conversion" << std::endl;

      uuid empty;
      assert(empty.string() == "00000000-0000-0000-0000-000000000000");
      assert(empty.wstring() == L"00000000-0000-0000-0000-000000000000");
   }   

   {
      std::cout << "Test constexpr" << std::endl;

      constexpr uuid empty;
      constexpr bool isnil = empty.nil();
      constexpr size_t size = empty.size();
      constexpr uuid_variant variant = empty.variant();
      constexpr uuid_version version = empty.version();
   }

   std::cout << "ALL PASSED" << std::endl;

   return 0;
}
