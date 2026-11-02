// -----------------------------------------------------------------------------
// stduuid benchmark
//
// Compares the SIMD accelerated implementation in <uuid.h> with the original
// scalar implementation, which is kept verbatim (only the namespace changed)
// in reference/uuid_original.h.  Both run in the same process, in the same
// translation unit, with the same compiler flags, so the numbers are directly
// comparable.
//
// Build with -DUUID_BUILD_BENCH=ON (it defaults to the value of
// UUID_MAIN_PROJECT) and run the resulting bench_stduuid executable.
// -----------------------------------------------------------------------------
#include <uuid.h>

#include "reference/uuid_original.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
   using clock_type = std::chrono::steady_clock;

   constexpr size_t item_count = 4096;

   // A volatile sink keeps the optimizer from deleting the work.  The asm
   // barrier additionally forces the inputs of each operation to be treated as
   // escaping, which stops loop invariant code motion.
   volatile uint64_t g_sink = 0;

   inline void escape(void const* p, size_t n) noexcept
   {
#if defined(__GNUC__) || defined(__clang__)
      __asm__ __volatile__("" : : "r"(p), "r"(n) : "memory");
#else
      (void)p;
      (void)n;
#endif
   }

   inline uint64_t bytes_digest(uuids::uuid const& id) noexcept
   {
      auto bytes = id.as_bytes();
      uint64_t v = 0;
      for (size_t i = 0; i < 16; ++i)
         v = (v << 8) ^ static_cast<unsigned char>(bytes[i]);
      return v;
   }

   inline uint64_t bytes_digest(uuids_reference::uuid const& id) noexcept
   {
      auto bytes = id.as_bytes();
      uint64_t v = 0;
      for (size_t i = 0; i < 16; ++i)
         v = (v << 8) ^ static_cast<unsigned char>(bytes[i]);
      return v;
   }

   double seconds(clock_type::time_point a, clock_type::time_point b) noexcept
   {
      return std::chrono::duration<double>(b - a).count();
   }

   struct row
   {
      std::string name;
      double original_ns;
      double simd_ns;
   };

   // Time one call of f, choosing a repetition count so that a single
   // measurement lasts at least ~120 ms.  f() itself performs `ops` logical
   // operations.  Returns the best (minimum) time per logical operation.
   template <class F>
   double measure(F&& f, size_t ops)
   {
      for (int i = 0; i < 2; ++i)
         f();

      size_t n = 1;
      for (;;)
      {
         auto t0 = clock_type::now();
         for (size_t i = 0; i < n; ++i)
            f();
         auto t1 = clock_type::now();
         double elapsed = seconds(t0, t1);
         if (elapsed >= 0.12 || n >= (size_t(1) << 22))
            break;

         double factor = elapsed > 1e-9 ? 0.12 / elapsed : 2.0;
         n = static_cast<size_t>(static_cast<double>(n) * std::max(2.0, factor * 1.1));
      }

      double best = 1e300;
      for (int r = 0; r < 5; ++r)
      {
         auto t0 = clock_type::now();
         for (size_t i = 0; i < n; ++i)
            f();
         auto t1 = clock_type::now();
         best = std::min(best, seconds(t0, t1));
      }

      return best / static_cast<double>(n * ops) * 1e9;
   }

   template <class FOriginal, class FSimd>
   void compare(std::string name, FOriginal original, FSimd simd, size_t ops,
                std::vector<row>& rows)
   {
      // Interleave the two implementations so that frequency scaling and
      // background load affect both equally.
      for (int i = 0; i < 2; ++i)
      {
         original();
         simd();
      }

      // Calibrate each side separately and use the longer repetition count.
      size_t n_original = 1;
      size_t n_simd = 1;
      auto calibrate = [](auto& f) {
         size_t n = 1;
         for (;;)
         {
            auto t0 = clock_type::now();
            for (size_t i = 0; i < n; ++i)
               f();
            auto t1 = clock_type::now();
            double elapsed = seconds(t0, t1);
            if (elapsed >= 0.05 || n >= (size_t(1) << 21))
               break;
            double factor = elapsed > 1e-9 ? 0.05 / elapsed : 2.0;
            n = static_cast<size_t>(static_cast<double>(n) * std::max(2.0, factor * 1.1));
         }
         return n;
      };

      n_original = calibrate(original);
      n_simd = calibrate(simd);
      size_t const n = std::max(n_original, n_simd);

      double best_original = 1e300;
      double best_simd = 1e300;
      for (int r = 0; r < 5; ++r)
      {
         auto t0 = clock_type::now();
         for (size_t i = 0; i < n; ++i)
            original();
         auto t1 = clock_type::now();
         best_original = std::min(best_original, seconds(t0, t1));

         auto t2 = clock_type::now();
         for (size_t i = 0; i < n; ++i)
            simd();
         auto t3 = clock_type::now();
         best_simd = std::min(best_simd, seconds(t2, t3));
      }

      double const scale = 1.0 / static_cast<double>(n * ops) * 1e9;
      rows.push_back({ std::move(name), best_original * scale, best_simd * scale });
   }

   std::string make_braced(uuids::uuid const& id)
   {
      auto s = uuids::to_string(id);
      return "{" + s + "}";
   }

   std::string make_no_dashes(uuids::uuid const& id)
   {
      auto s = uuids::to_string(id);
      std::string out;
      out.reserve(32);
      for (char c : s)
         if (c != '-')
            out.push_back(c);
      return out;
   }

   std::string make_odd_dashes(uuids::uuid const& id)
   {
      // Dashes in non canonical places: the general scalar parser must still
      // accept this, so it also exercises the fallback path.
      auto s = uuids::to_string(id);
      std::string out;
      out.reserve(40);
      size_t n = 0;
      for (char c : s)
      {
         if (c == '-')
            continue;
         out.push_back(c);
         if (++n == 3 || n == 7 || n == 20)
            out.push_back('-');
      }
      return out;
   }

   bool verify()
   {
      std::mt19937_64 rng(0x5bd1e9955bd1e995ULL);
      std::vector<std::array<uint8_t, 16>> raw(item_count);
      for (auto& bytes : raw)
         for (auto& b : bytes)
            b = static_cast<uint8_t>(rng());

      raw[0] = {};

      uuids::uuid_name_generator name_new(uuids::uuid_namespace_dns);
      uuids_reference::uuid_name_generator name_old(
         uuids_reference::uuid_namespace_dns);

      std::string long_name(2048, 'x');
      for (size_t i = 0; i < long_name.size(); ++i)
         long_name[i] = static_cast<char>('a' + (i % 26));

      for (size_t i = 0; i < raw.size(); ++i)
      {
         uuids::uuid const now(raw[i]);
         uuids_reference::uuid const old(raw[i]);

         std::string const s_new = uuids::to_string(now);
         std::string const s_old = uuids_reference::to_string(old);
         if (s_new != s_old)
         {
            std::cerr << "to_string mismatch at " << i << "\n";
            return false;
         }

         char buffer[36];
         uuids::to_string<char>(now, buffer);
         if (std::string(buffer, 36) != s_old)
         {
            std::cerr << "buffered to_string mismatch at " << i << "\n";
            return false;
         }

         std::string const braced = make_braced(now);
         std::string const no_dashes = make_no_dashes(now);
         std::string const odd_dashes = make_odd_dashes(now);
         char const* forms[] = {
            s_new.c_str(), braced.c_str(), no_dashes.c_str(), odd_dashes.c_str()
         };

         for (char const* form : forms)
         {
            auto parsed_new = uuids::uuid::from_string(form);
            auto parsed_old = uuids_reference::uuid::from_string(form);
            if (!parsed_new || !parsed_old)
            {
               std::cerr << "from_string failed at " << i << ": " << form << "\n";
               return false;
            }
            if (bytes_digest(*parsed_new) != bytes_digest(*parsed_old))
            {
               std::cerr << "from_string mismatch at " << i << ": " << form << "\n";
               return false;
            }

            if (uuids::uuid::is_valid_uuid(form) !=
                uuids_reference::uuid::is_valid_uuid(form))
            {
               std::cerr << "is_valid_uuid mismatch at " << i << ": " << form << "\n";
               return false;
            }
         }

         // Invalid spellings must be rejected by both.
         std::string invalid = s_new;
         invalid[0] = 'g';
         if (uuids::uuid::is_valid_uuid(invalid) !=
             uuids_reference::uuid::is_valid_uuid(invalid))
         {
            std::cerr << "invalid is_valid_uuid mismatch at " << i << "\n";
            return false;
         }
         if (uuids::uuid::from_string(invalid).has_value() !=
             uuids_reference::uuid::from_string(invalid).has_value())
         {
            std::cerr << "invalid from_string mismatch at " << i << "\n";
            return false;
         }

         if (now.is_nil() != old.is_nil())
         {
            std::cerr << "is_nil mismatch at " << i << "\n";
            return false;
         }

         if ((now == uuids::uuid(raw[(i + 1) % raw.size()])) !=
             (old == uuids_reference::uuid(raw[(i + 1) % raw.size()])))
         {
            std::cerr << "operator== mismatch at " << i << "\n";
            return false;
         }
      }

      // SHA-1 path, including a multi block message.
      for (size_t i = 0; i < 64; ++i)
      {
         std::string name = "name-" + std::to_string(i);
         if (bytes_digest(name_new(name)) != bytes_digest(name_old(name)))
         {
            std::cerr << "name generator mismatch at " << i << "\n";
            return false;
         }
      }
      if (bytes_digest(name_new(long_name)) != bytes_digest(name_old(long_name)))
      {
         std::cerr << "name generator mismatch for long name\n";
         return false;
      }

      return true;
   }

   void print_report(std::vector<row> const& rows)
   {
      std::cout << '\n';
      std::cout << std::left << std::setw(40) << "operation"
                << std::right << std::setw(16) << "original ns/op"
                << std::setw(14) << "SIMD ns/op"
                << std::setw(12) << "speedup" << '\n';
      std::cout << std::string(82, '-') << '\n';

      for (auto const& r : rows)
      {
         double const speedup = r.original_ns / r.simd_ns;
         std::cout << std::left << std::setw(40) << r.name
                   << std::right << std::fixed << std::setprecision(2)
                   << std::setw(16) << r.original_ns
                   << std::setw(14) << r.simd_ns
                   << std::setw(11) << speedup << 'x' << '\n';
      }
      std::cout << std::string(82, '-') << '\n';
   }
}

int main(int argc, char** argv)
{
   bool verify_only = false;
   for (int i = 1; i < argc; ++i)
   {
      if (std::strcmp(argv[i], "--verify-only") == 0)
         verify_only = true;
      else
      {
         std::cerr << "usage: " << argv[0] << " [--verify-only]\n";
         return 2;
      }
   }

   if (!verify())
   {
      std::cerr << "verification failed, aborting\n";
      return 1;
   }

   if (verify_only)
   {
      std::cout << "verification passed\n";
      return 0;
   }

   std::mt19937_64 rng(0x123456789abcdef0ULL);
   std::vector<std::array<uint8_t, 16>> raw(item_count);
   for (auto& bytes : raw)
      for (auto& b : bytes)
         b = static_cast<uint8_t>(rng());
   raw[0] = {};

   std::vector<uuids::uuid> ids;
   std::vector<uuids_reference::uuid> old_ids;
   std::vector<std::string> strings;
   std::vector<std::string> braced;
   std::vector<std::string> no_dashes;
   std::vector<std::string> odd_dashes;
   ids.reserve(item_count);
   old_ids.reserve(item_count);
   strings.reserve(item_count);
   braced.reserve(item_count);
   no_dashes.reserve(item_count);
   odd_dashes.reserve(item_count);

   for (auto const& bytes : raw)
   {
      ids.emplace_back(bytes);
      old_ids.emplace_back(bytes);
      strings.push_back(uuids::to_string(ids.back()));
      braced.push_back(make_braced(ids.back()));
      no_dashes.push_back(make_no_dashes(ids.back()));
      odd_dashes.push_back(make_odd_dashes(ids.back()));
   }

   std::vector<std::string> names;
   names.reserve(item_count);
   for (size_t i = 0; i < item_count; ++i)
      names.push_back("host-" + std::to_string(i) + ".example.com");

   std::string const long_name = [] {
      std::string s(2048, 'x');
      for (size_t i = 0; i < s.size(); ++i)
         s[i] = static_cast<char>('a' + (i % 26));
      return s;
   }();

   std::vector<row> rows;

   // -----------------------------------------------------------------------
   // Formatting
   // -----------------------------------------------------------------------
   compare("to_string (std::string)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         std::string s = uuids_reference::to_string(old_ids[i]);
         sum += static_cast<unsigned char>(s[0]);
         escape(s.data(), s.size());
      }
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         std::string s = uuids::to_string(ids[i]);
         sum += static_cast<unsigned char>(s[0]);
         escape(s.data(), s.size());
      }
      g_sink += sum;
   }, item_count, rows);

   compare("to_string (36 byte buffer)", [&] {
      uint64_t sum = 0;
      alignas(16) char buffer[36];
      for (size_t i = 0; i < item_count; ++i)
      {
         uuids_reference::to_string<char>(old_ids[i], buffer);
         sum += static_cast<unsigned char>(buffer[i & 15]);
         escape(buffer, 36);
      }
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      alignas(16) char buffer[36];
      for (size_t i = 0; i < item_count; ++i)
      {
         uuids::to_string<char>(ids[i], buffer);
         sum += static_cast<unsigned char>(buffer[i & 15]);
         escape(buffer, 36);
      }
      g_sink += sum;
   }, item_count, rows);

   // -----------------------------------------------------------------------
   // Parsing
   // -----------------------------------------------------------------------
   compare("from_string (canonical)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids_reference::uuid::from_string(strings[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids::uuid::from_string(strings[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, item_count, rows);

   compare("from_string (braced)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids_reference::uuid::from_string(braced[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids::uuid::from_string(braced[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, item_count, rows);

   compare("from_string (no dashes)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids_reference::uuid::from_string(no_dashes[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids::uuid::from_string(no_dashes[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, item_count, rows);

   compare("from_string (odd dashes, scalar)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids_reference::uuid::from_string(odd_dashes[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
      {
         auto id = uuids::uuid::from_string(odd_dashes[i]);
         sum += id.has_value() ? static_cast<unsigned char>((*id).as_bytes()[0]) : 1;
      }
      g_sink += sum;
   }, item_count, rows);

   compare("is_valid_uuid", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += uuids_reference::uuid::is_valid_uuid(strings[i]) ? 1 : 0;
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += uuids::uuid::is_valid_uuid(strings[i]) ? 1 : 0;
      g_sink += sum;
   }, item_count, rows);

   // -----------------------------------------------------------------------
   // Comparisons / hashing
   // -----------------------------------------------------------------------
   // Random uuids make the scalar implementation exit on the first non-zero
   // byte, which is a hard case for a fixed-width SIMD check.
   compare("is_nil (random, early exit)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += old_ids[i].is_nil() ? 1 : 0;
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += ids[i].is_nil() ? 1 : 0;
      g_sink += sum;
   }, item_count, rows);

   // When every uuid really is nil the scalar loop has to inspect all 16 bytes
   // and the SIMD version wins.
   std::vector<uuids::uuid> nil_ids(item_count);
   std::vector<uuids_reference::uuid> old_nil_ids(item_count);
   compare("is_nil (all nil)", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += old_nil_ids[i].is_nil() ? 1 : 0;
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += nil_ids[i].is_nil() ? 1 : 0;
      g_sink += sum;
   }, item_count, rows);

   compare("operator==", [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += (old_ids[i] == old_ids[(i + 1) % item_count]) ? 1 : 0;
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      for (size_t i = 0; i < item_count; ++i)
         sum += (ids[i] == ids[(i + 1) % item_count]) ? 1 : 0;
      g_sink += sum;
   }, item_count, rows);

   compare("std::hash", [&] {
      uint64_t sum = 0;
      std::hash<uuids_reference::uuid> hasher;
      for (size_t i = 0; i < item_count; ++i)
         sum += hasher(old_ids[i]);
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      std::hash<uuids::uuid> hasher;
      for (size_t i = 0; i < item_count; ++i)
         sum += hasher(ids[i]);
      g_sink += sum;
   }, item_count, rows);

   // -----------------------------------------------------------------------
   // Generators (SHA-1 is the interesting one)
   // -----------------------------------------------------------------------
   compare("name generator (short name)", [&] {
      uint64_t sum = 0;
      uuids_reference::uuid_name_generator gen(uuids_reference::uuid_namespace_dns);
      for (size_t i = 0; i < item_count; ++i)
         sum += bytes_digest(gen(names[i]));
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      uuids::uuid_name_generator gen(uuids::uuid_namespace_dns);
      for (size_t i = 0; i < item_count; ++i)
         sum += bytes_digest(gen(names[i]));
      g_sink += sum;
   }, item_count, rows);

   compare("name generator (2048 byte name)", [&] {
      uint64_t sum = 0;
      uuids_reference::uuid_name_generator gen(uuids_reference::uuid_namespace_dns);
      for (size_t i = 0; i < item_count / 16; ++i)
         sum += bytes_digest(gen(long_name));
      g_sink += sum;
   }, [&] {
      uint64_t sum = 0;
      uuids::uuid_name_generator gen(uuids::uuid_namespace_dns);
      for (size_t i = 0; i < item_count / 16; ++i)
         sum += bytes_digest(gen(long_name));
      g_sink += sum;
   }, item_count / 16, rows);

   print_report(rows);
   std::cout << "\nchecksum: " << g_sink << '\n';

   return 0;
}
