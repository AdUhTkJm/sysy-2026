#include "../utils/presburger/IntegerRelation.h"
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace pres;

namespace {

using Int = IntegerRelation::Int;
using Row = IntegerRelation::Row;

IntegerRelation pointRelation(Space space, const Row &point) {
  IntegerRelation rel(space);
  for (unsigned i = 0; i < point.size(); ++i) {
    Row row(space.dimension() + 1, 0);
    row[i] = 1;
    row.back() = -point[i];
    rel.addEquality(row);
  }
  rel.normalize();
  return rel;
}

bool containsPoint(const IntegerRelation &rel, const Row &point) {
  return !IntegerRelation::intersection(rel, pointRelation(rel.getSpace(), point)).isEmpty();
}

bool relationEmptyAtAllDisjuncts(const PresburgerRelation &rel,
                                 const Row &point) {
  for (const auto &disjunct : rel.getDisjuncts()) {
    if (containsPoint(disjunct, point))
      return false;
  }
  return true;
}

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

void testIntegerEmptiness() {
  Space s {1, 0, 0, 0};
  auto impossibleParity = IntegerRelation(s, { { 2, -1 } }, {});
  expect(impossibleParity.isEmpty(), "2x - 1 == 0 should be integer-empty");

  Space s2 {2, 0, 0, 0};
  auto rationalButNotInteger = IntegerRelation(
      s2,
      { { 1, -1, 0 } },
      { { 2, 0, -1 }, { -2, 0, 1 } });
  expect(rationalButNotInteger.isEmpty(),
           "x - y == 0, 2x - 1 >= 0, -2x + 1 >= 0 should be integer-empty");

  auto nonEmpty = IntegerRelation(
      s2,
      { { 1, -1, 0 } },
      { { 1, 0, 0 }, { -1, 0, 1 } });
  expect(!nonEmpty.isEmpty(), "x == y, 0 <= x <= 1 should be non-empty");

  auto gcdCheck = IntegerRelation(s, { { 3, -1 } }, {});
  expect(gcdCheck.isEmpty(), "3x - 1 == 0 should be empty");

  auto integerGap = IntegerRelation(s, {}, { { 3, -2 }, { -3, 4 } });
  expect(integerGap.isEmpty(), "2 <= 3x <= 4 has no integer solution");
}

void testIntegerRelationOps() {
  Space s {1, 0, 0, 0};
  auto ge0 = IntegerRelation(s, {}, { { 1, 0 } });
  auto le2 = IntegerRelation(s, {}, { { -1, 2 } });
  auto ge1 = IntegerRelation(s, {}, { { 1, -1 } });

  auto inter = IntegerRelation::intersection(ge0, le2);
  expect(containsPoint(inter, { 0 }), "intersection should contain 0");
  expect(containsPoint(inter, { 2 }), "intersection should contain 2");
  expect(!containsPoint(inter, { 3 }), "intersection should not contain 3");

  auto uni = IntegerRelation::setUnion(inter, ge1);
  expect(uni.getDisjuncts().size() == 2,
           "union of distinct basic relations should have two disjuncts");
  expect(!relationEmptyAtAllDisjuncts(uni, { 0 }), "union should contain 0");
  expect(!relationEmptyAtAllDisjuncts(uni, { 3 }), "union should contain 3");

  auto sameUnion = IntegerRelation::setUnion(inter, inter);
  expect(sameUnion.getDisjuncts().size() == 1,
           "union of identical relations should collapse to one disjunct");

  auto singlePoint = IntegerRelation(s, { { 1, -1 } }, {});
  auto diff = IntegerRelation::setDifference(inter, singlePoint);
  expect(relationEmptyAtAllDisjuncts(diff, { 1 }),
           "difference should remove x == 1");
  expect(!relationEmptyAtAllDisjuncts(diff, { 0 }),
           "difference should keep 0");
  expect(!relationEmptyAtAllDisjuncts(diff, { 2 }),
           "difference should keep 2");
}

void testPresburgerRelationOps() {
  Space s {1, 0, 0, 0};
  auto x0 = IntegerRelation(s, { { 1, 0 } }, {});
  auto x2 = IntegerRelation(s, { { 1, -2 } }, {});
  auto ge1 = IntegerRelation(s, {}, { { 1, -1 } });
  auto x1 = IntegerRelation(s, { { 1, -1 } }, {});

  PresburgerRelation disj({ x0, x2 });
  auto inter = PresburgerRelation::intersection(disj, PresburgerRelation(ge1));
  expect(inter.getDisjuncts().size() == 1,
           "intersection should keep only the surviving disjunct");
  expect(!relationEmptyAtAllDisjuncts(inter, { 2 }),
           "intersected disjunction should contain 2");
  expect(relationEmptyAtAllDisjuncts(inter, { 0 }),
           "intersected disjunction should not contain 0");

  auto uni = PresburgerRelation::setUnion(PresburgerRelation(x0), PresburgerRelation(x2));
  expect(uni.getDisjuncts().size() == 2,
           "Presburger union should keep both singleton disjuncts");

  auto diff = PresburgerRelation::setDifference(
      PresburgerRelation(IntegerRelation(s, {}, { { 1, 0 }, { -1, 2 } })),
      PresburgerRelation(x1));
  expect(relationEmptyAtAllDisjuncts(diff, { 1 }),
           "Presburger difference should remove 1");
  expect(!relationEmptyAtAllDisjuncts(diff, { 0 }),
           "Presburger difference should keep 0");
  expect(!relationEmptyAtAllDisjuncts(diff, { 2 }),
           "Presburger difference should keep 2");
}

} // namespace

namespace test {

void runIntegerRelationTests() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
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
