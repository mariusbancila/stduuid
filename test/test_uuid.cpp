#include "uuid.h"
#include "catch.hpp"

#include <set>
#include <unordered_set>
#include <vector>

using namespace uuids;

TEST_CASE("Test default constructor", "[ctors]") 
{
   uuid empty;
   REQUIRE(empty.is_nil());
   REQUIRE(empty.size() == 16);
}

TEST_CASE("Test string_view constructor", "[ctors]")
{
   using namespace std::string_literals;

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
      uuid guid(str);
      REQUIRE(uuids::to_string(guid) == str);
   }

   {
      uuid guid("47183823-2574-4bfd-b411-99ed177d3e43");
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
      REQUIRE(uuids::to_wstring(guid) == L"47183823-2574-4bfd-b411-99ed177d3e43");
   }
}

TEST_CASE("Test wstring_view constructor", "[ctors]")
{
   using namespace std::string_literals;

   auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
   uuid guid(str);
   REQUIRE(uuids::to_wstring(guid) == str);
}

TEST_CASE("Test iterators constructor", "[ctors]")
{
   using namespace std::string_literals;

   {
      std::array<uuids::uuid::value_type, 16> arr{ {
            0x47, 0x18, 0x38, 0x23,
            0x25, 0x74,
            0x4b, 0xfd,
            0xb4, 0x11,
            0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 } };

      uuid guid(std::begin(arr), std::end(arr));
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }

   {
      uuids::uuid::value_type arr[16] = {
         0x47, 0x18, 0x38, 0x23,
         0x25, 0x74,
         0x4b, 0xfd,
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 };

      uuid guid(std::begin(arr), std::end(arr));
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }
}

TEST_CASE("Test equality", "[operators]")
{
   uuid empty;
   uuid guid = uuids::uuid_random_generator{}();

   REQUIRE(empty == empty);
   REQUIRE(guid == guid);
   REQUIRE(empty != guid);
}

TEST_CASE("Test comparison", "[operators]")
{
   auto empty = uuid{};
   uuids::uuid_random_generator gen;
   auto id = gen();

   REQUIRE(empty < id);

   std::set<uuids::uuid> ids{
      uuid{},
      gen(),
      gen(),
      gen(),
      gen()
   };

   REQUIRE(ids.size() == 5);
   REQUIRE(ids.find(uuid{}) != ids.end());
}

TEST_CASE("Test hashing", "[ops]")
{
   using namespace std::string_literals;
   auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
   auto guid = uuid{ str };

   auto h1 = std::hash<std::string>{};
   auto h2 = std::hash<uuid>{};
   REQUIRE(h1(str) == h2(guid));

   uuids::uuid_random_generator gen;

   std::unordered_set<uuids::uuid> ids{
      uuid{},
      gen(),
      gen(),
      gen(),
      gen()
   };

   REQUIRE(ids.size() == 5);
   REQUIRE(ids.find(uuid{}) != ids.end());
}

TEST_CASE("Test swap", "[ops]")
{
   uuid empty;
   uuid guid = uuids::uuid_random_generator{}();

   REQUIRE(empty.is_nil());
   REQUIRE(!guid.is_nil());

   std::swap(empty, guid);

   REQUIRE(!empty.is_nil());
   REQUIRE(guid.is_nil());

   empty.swap(guid);

   REQUIRE(empty.is_nil());
   REQUIRE(!guid.is_nil());
}

TEST_CASE("Test string conversion", "[ops]")
{
   uuid empty;
   REQUIRE(uuids::to_string(empty) == "00000000-0000-0000-0000-000000000000");
   REQUIRE(uuids::to_wstring(empty) == L"00000000-0000-0000-0000-000000000000");
}

TEST_CASE("Test iterators", "[iter]")
{
   std::array<uuids::uuid::value_type, 16> arr{ {
         0x47, 0x18, 0x38, 0x23,
         0x25, 0x74,
         0x4b, 0xfd,
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
      } };

   {
      uuid guid;
      REQUIRE(guid.is_nil());

      std::copy(std::cbegin(arr), std::cend(arr), std::begin(guid));
      REQUIRE(!guid.is_nil());
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");

      size_t i = 0;
      for (auto const & b : guid)
      {
         REQUIRE(arr[i++] == b);
      }
   }

   {
      const uuid guid("47183823-2574-4bfd-b411-99ed177d3e43");
      REQUIRE(!guid.is_nil());
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");

      size_t i = 0;
      for (auto const & b : guid)
      {
         REQUIRE(arr[i++] == b);
      }
   }
}

TEST_CASE("Test constexpr", "[const]")
{
   constexpr uuid empty;
   constexpr bool isnil = empty.is_nil();
   constexpr size_t size = empty.size();
   constexpr uuid_variant variant = empty.variant();
   constexpr uuid_version version = empty.version();
}