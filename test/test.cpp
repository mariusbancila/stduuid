#include "../include/uuid.h"
#include <assert.h>
#include <iostream>
#include <set>
#include <unordered_set>
#include <random>
#include <vector>

int main()
{
   using namespace uuids;

   {
      std::cout << "Test default constructor" << std::endl;

      uuid empty;
      assert(empty.is_nil());
      assert(empty.size() == 16);
   }

   {
      std::cout << "Test string_view constructor" << std::endl;

      using namespace std::string_literals;

      {
         auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
         uuid guid(str);
         assert(uuids::to_string(guid) == str);
      }

      {
         uuid guid("47183823-2574-4bfd-b411-99ed177d3e43");
         assert(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
         assert(uuids::to_wstring(guid) == L"47183823-2574-4bfd-b411-99ed177d3e43");
      }
   }

   {
      std::cout << "Test wstring_view constructor" << std::endl;

      using namespace std::string_literals;

      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
      uuid guid(str);
      assert(uuids::to_wstring(guid) == str);
   }

   {
      std::cout << "Test iterators constructor" << std::endl;

      using namespace std::string_literals;

      {
         std::array<uuids::uuid::value_type, 16> arr{ {
               0x47, 0x18, 0x38, 0x23,
               0x25, 0x74,
               0x4b, 0xfd,
               0xb4, 0x11,
               0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43} };

         uuid guid(std::begin(arr), std::end(arr));
         assert(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
      }

      {
         uuids::uuid::value_type arr[16] = {
            0x47, 0x18, 0x38, 0x23,
            0x25, 0x74,
            0x4b, 0xfd,
            0xb4, 0x11,
            0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 };

         uuid guid(std::begin(arr), std::end(arr));
         assert(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
      }
   }

   {
      std::cout << "Test equality" << std::endl;

      uuid empty;
      uuid guid = uuids::uuid_random_generator{}();

      assert(empty == empty);
      assert(guid == guid);
      assert(empty != guid);
   }

   {
      std::cout << "Test comparison" << std::endl;

      auto empty = uuid{};
      uuids::uuid_random_generator gen;
      auto id = gen();

      assert(empty < id);

      std::set<uuids::uuid> ids{
         uuid{},
         gen(),
         gen(),
         gen(),
         gen()
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

      uuids::uuid_random_generator gen;

      std::unordered_set<uuids::uuid> ids{
         uuid{},
         gen(),
         gen(),
         gen(),
         gen()
      };

      assert(ids.size() == 5);
      assert(ids.find(uuid{}) != ids.end());
   }

   {
      std::cout << "Test swap" << std::endl;

      uuid empty;
      uuid guid = uuids::uuid_random_generator{}();

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
      assert(uuids::to_string(empty) == "00000000-0000-0000-0000-000000000000");
      assert(uuids::to_wstring(empty) == L"00000000-0000-0000-0000-000000000000");
   }   

   {
      std::cout << "Test iterators" << std::endl;

      std::array<uuids::uuid::value_type, 16> arr{{
         0x47, 0x18, 0x38, 0x23,
         0x25, 0x74,
         0x4b, 0xfd,
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
      }};

      {
         uuid guid;
         assert(guid.is_nil());

         std::copy(std::cbegin(arr), std::cend(arr), std::begin(guid));
         assert(!guid.is_nil());
         assert(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");

         size_t i = 0;
         for (auto const & b : guid)
         {
            assert(arr[i++] == b);
         }
      }

      {
         const uuid guid("47183823-2574-4bfd-b411-99ed177d3e43");
         assert(!guid.is_nil());
         assert(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");

         size_t i = 0;
         for (auto const & b : guid)
         {
            assert(arr[i++] == b);
         }
      }
   }

   {
      std::cout << "Test constexpr" << std::endl;

      constexpr uuid empty;
      constexpr bool isnil = empty.is_nil();
      constexpr size_t size = empty.size();
      constexpr uuid_variant variant = empty.variant();
      constexpr uuid_version version = empty.version();
   }

   {
      std::cout << "Test default generator" << std::endl;

      uuid const guid = uuids::uuid_random_generator{}();
      assert(!guid.is_nil());
      assert(guid.size() == 16);
      assert(guid.version() == uuids::uuid_version::random_number_based);
      assert(guid.variant() == uuids::uuid_variant::rfc);
   }

   {
      std::cout << "Test random generator (default ctor)" << std::endl;

      uuids::uuid_random_generator dgen;
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test random generator (conversion ctor w/ ptr)" << std::endl;

      std::random_device rd;
      auto seed_data = std::array<int, std::mt19937::state_size> {};
      std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
      std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
      std::mt19937 generator(seq);

      uuids::uuid_random_generator dgen(&generator);
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test random generator (conversion ctor w/ ptr)" << std::endl;

      std::random_device rd;
      auto seed_data = std::array<int, std::mt19937::state_size> {};
      std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
      std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
      auto generator = std::make_unique<std::mt19937>(seq);

      uuids::uuid_random_generator dgen(generator.get());
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id1.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test random generator (conversion ctor w/ ref)" << std::endl;

      std::random_device rd;
      auto seed_data = std::array<int, std::mt19937::state_size> {};
      std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
      std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
      std::mt19937 generator(seq);

      uuids::uuid_random_generator dgen(generator);
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test basic random generator (default ctor) w/ ranlux48_base" << std::endl;

      uuids::basic_uuid_random_generator<std::ranlux48_base> dgen;
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id1.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test basic random generator (conversion ctor w/ ptr) w/ ranlux48_base" << std::endl;

      std::random_device rd;
      std::ranlux48_base generator(rd());

      uuids::basic_uuid_random_generator<std::ranlux48_base> dgen(&generator);
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test basic random generator (conversion ctor w/ ptr) w/ ranlux48_base" << std::endl;

      std::random_device rd;
      auto generator = std::make_unique<std::ranlux48_base>(rd());

      uuids::basic_uuid_random_generator<std::ranlux48_base> dgen(generator.get());
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test basic random generator (conversion ctor w/ ref) w/ ranlux48_base" << std::endl;

      std::random_device rd;
      std::ranlux48_base generator(rd());

      uuids::basic_uuid_random_generator<std::ranlux48_base> dgen(generator);
      auto id1 = dgen();
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::random_number_based);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen();
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::random_number_based);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
   }

   {
      std::cout << "Test name generator" << std::endl;

      uuids::uuid_name_generator dgen(uuids::uuid{"47183823-2574-4bfd-b411-99ed177d3e43"});
      auto id1 = dgen("john");
      assert(!id1.is_nil());
      assert(id1.size() == 16);
      assert(id1.version() == uuids::uuid_version::name_based_sha1);
      assert(id1.variant() == uuids::uuid_variant::rfc);

      auto id2 = dgen("jane");
      assert(!id2.is_nil());
      assert(id2.size() == 16);
      assert(id2.version() == uuids::uuid_version::name_based_sha1);
      assert(id2.variant() == uuids::uuid_variant::rfc);

      auto id3 = dgen("jane");
      assert(!id3.is_nil());
      assert(id3.size() == 16);
      assert(id3.version() == uuids::uuid_version::name_based_sha1);
      assert(id3.variant() == uuids::uuid_variant::rfc);

      auto id4 = dgen(L"jane");
      assert(!id4.is_nil());
      assert(id4.size() == 16);
      assert(id4.version() == uuids::uuid_version::name_based_sha1);
      assert(id4.variant() == uuids::uuid_variant::rfc);

      assert(id1 != id2);
      assert(id2 == id3);
      assert(id3 != id4);
   }

   std::cout << "ALL PASSED" << std::endl;

   return 0;
}
