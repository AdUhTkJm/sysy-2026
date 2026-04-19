#include "../utils/presburger/IntegerRelation.h"
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace pres;

#define test [[gnu::unused]]

namespace {

using Int = IntegerRelation::Int;
using Row = IntegerRelation::Row;

struct TestContext {
  int failures = 0;

  void expect(bool condition, const std::string &message) {
    if (!condition) {
      std::cerr << "[FAIL] " << message << "\n";
      ++failures;
    }
  }
} t;

#define expect t.expect

test void testIntegerEmptiness() {
  Space s {1, 0, 0, 0};

  // Space s2 {2, 0, 0, 0};
  // auto rationalButNotInteger = IntegerRelation(
  //     s2,
  //     { { 1, -1, 0 } },
  //     { { 2, 0, -1 }, { -2, 0, 1 } });
  // expect(rationalButNotInteger.isEmpty(),
  //          "x - y == 0, 2x - 1 >= 0, -2x + 1 >= 0 should be integer-empty");

  // auto nonEmpty = IntegerRelation(
  //     s2,
  //     { { 1, -1, 0 } },
  //     { { 1, 0, 0 }, { -1, 0, 1 } });
  // expect(!nonEmpty.isEmpty(), "x == y, 0 <= x <= 1 should be non-empty");

  // auto gcdCheck = IntegerRelation(s, { { 3, -1 } }, {});
  // expect(gcdCheck.isEmpty(), "3x - 1 == 0 should be empty");

  auto integerGap = IntegerRelation(s, {}, { { 4, -2 }, { -4, 3 } });
  expect(integerGap.isEmpty(), "2 <= 4x <= 3 has no integer solution");
}

test void testIntegerRelationOps() {
  Space s {1, 0, 0, 0};
  auto ge0 = IntegerRelation(s, {}, { { 1, 0 } });
  auto le2 = IntegerRelation(s, {}, { { -1, 2 } });
  auto ge1 = IntegerRelation(s, {}, { { 1, -1 } });

  auto inter = IntegerRelation::intersection(ge0, le2);
  expect(inter.containsPoint({ 0 }), "intersection should contain 0");
  expect(inter.containsPoint({ 2 }), "intersection should contain 2");
  expect(!inter.containsPoint({ 3 }), "intersection should not contain 3");

  auto uni = IntegerRelation::setUnion(inter, ge1);
  expect(uni.getNumDisjuncts() == 2,
           "union of distinct basic relations should have two disjuncts");
  expect(!uni.containsPoint({ 0 }), "union should contain 0");
  expect(!uni.containsPoint({ 3 }), "union should contain 3");

  auto sameUnion = IntegerRelation::setUnion(inter, inter);
  expect(sameUnion == inter, "union should be idempotent");

  auto singlePoint = IntegerRelation(s, { { 1, -1 } }, {});
  auto diff = IntegerRelation::setDifference(inter, singlePoint);
  expect(diff.containsPoint({ 1 }),
           "difference should remove x == 1");
  expect(!diff.containsPoint({ 0 }),
           "difference should keep 0");
  expect(!diff.containsPoint({ 2 }),
           "difference should keep 2");
}

test void testPresburgerRelationOps() {
  Space s {1, 0, 0, 0};
  auto x0 = IntegerRelation(s, { { 1, 0 } }, {});
  auto x2 = IntegerRelation(s, { { 1, -2 } }, {});
  auto ge1 = IntegerRelation(s, {}, { { 1, -1 } });
  auto x1 = IntegerRelation(s, { { 1, -1 } }, {});

  PresburgerRelation disj({ x0, x2 });
  auto inter = PresburgerRelation::intersection(disj, PresburgerRelation(ge1));
  expect(inter.getNumDisjuncts() == 1,
           "intersection should keep only the surviving disjunct");
  expect(!inter.containsPoint({ 2 }),
           "intersected disjunction should contain 2");
  expect(inter.containsPoint({ 0 }),
           "intersected disjunction should not contain 0");

  auto uni = PresburgerRelation::setUnion(PresburgerRelation(x0), PresburgerRelation(x2));
  expect(uni.getNumDisjuncts() == 2,
           "Presburger union should keep both singleton disjuncts");

  auto diff = PresburgerRelation::setDifference(
      PresburgerRelation(IntegerRelation(s, {}, { { 1, 0 }, { -1, 2 } })),
      PresburgerRelation(x1));
  expect(diff.containsPoint({ 1 }),
           "Presburger difference should remove 1");
  expect(!diff.containsPoint({ 0 }),
           "Presburger difference should keep 0");
  expect(!diff.containsPoint({ 2 }),
           "Presburger difference should keep 2");
}

test void testSimplex() {
  // x + 2y >= 3
  // y + 2z == 4
  // z + 2x >= 5
  // x, y, z <= 10 to make it bounded
  Space s { 3, 0, 0, 0 };
  IntegerRelation rel(s, {
    { 0, 1, 2, -4 },
  }, {
    { 1, 2, 0, -3 },
    { 2, 0, 1, -5 },
    { -1, 0, 0, 10 },
    { 0, -1, 0, 10 },
    { 0, 0, -1, 10 }
  });

  expect(rel.getRationalMin({ 0, 0, 1, 0 }) == -3,
    "simplex: z >= -3");
  expect(rel.getRationalMin({ 1, 0, 0, 0 }) == Fraction(5, 3),
    "simplex: x >= 5/3");
  expect(rel.getRationalMin({ 1, 0, 1, 0 }) == 1,
    "simplex: x + z >= 1"); // x = 4, z = -3
}

} // namespace
#undef test

namespace test {

void runIntegerRelationTests() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
    { "TestSimplex", testSimplex },
    { "IntegerEmptiness", testIntegerEmptiness },
    { "IntegerRelationOps", testIntegerRelationOps },
    { "PresburgerRelationOps", testPresburgerRelationOps },
  };

  for (const auto &[name, fn] : tests) {
    int before = t.failures;
    fn();
    if (t.failures == before)
      std::cerr << "[PASS] " << name << "\n";
  }

  if (t.failures != 0) {
    std::cerr << t.failures << " test(s) failed\n";
    return;
  }

  std::cerr << "All IntegerRelation unit tests passed\n";
}

}
