#include "..\include\uuid.h"
#include <assert.h>
#include <iostream>

int main()
{
   using namespace uuids;

   {
      std::cout << "Test nill" << std::endl;

      uuid empty;
      assert(empty.is_nil());
      assert(empty.size() == 16);
   }

   {
      std::cout << "Test make" << std::endl;

      uuid const guid = uuids::make_uuid();
      assert(!guid.is_nil());
      assert(guid.size() == 16);
      assert(guid.version() == uuids::version_type::random_number_based);
      assert(guid.variant() == uuids::variant_type::rfc);
   }

   {
      std::cout << "Test comparison" << std::endl;

      uuid empty;
      uuid guid = uuids::make_uuid();

      assert(empty == empty);
      assert(guid == guid);
      assert(empty != guid);
   }

   {
      std::cout << "Test swap" << std::endl;

      uuid empty;
      uuid guid = uuids::make_uuid();

      assert(empty.is_nil());
      assert(!guid.is_nil());

      std::swap(empty, guid);

      assert(!empty.is_nil());
      assert(guid.is_nil());

      empty.swap(guid);

      assert(empty.is_nil());
      assert(!guid.is_nil());
   }

   {
      std::cout << "Test string conversion" << std::endl;

      uuid empty;
      assert(empty.string() == "00000000-0000-0000-0000-000000000000");
      assert(empty.wstring() == L"00000000-0000-0000-0000-000000000000");
   }

   {
      std::cout << "Test string constructor" << std::endl;

      using namespace std::string_literals;

      auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
      uuid guid(str);
      assert(guid.string() == str);
   }

   {
      std::cout << "Test wstring constructor" << std::endl;

      using namespace std::string_literals;

      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
      uuid guid(str);
      assert(guid.wstring() == str);
   }

   {
      std::cout << "Test constexpr" << std::endl;

      constexpr uuid empty;
      constexpr bool isnil = empty.is_nil();
      constexpr size_t size = empty.size();
      constexpr uuids::variant_type variant = empty.variant();
      constexpr uuids::version_type version = empty.version();
   }

   std::cout << "ALL PASSED" << std::endl;

   return 0;
}
