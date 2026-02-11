#include "Attrs.h"
#include "Builder.h"

#include <deque>
#include <unordered_map>
#include <iostream>
#include <cassert>
#include <algorithm>

namespace ir {

Arena Op::arena;
Arena Type::arena;
Arena Value::arena;

Type::TypeCache Type::cache;
std::unordered_map<Value*, Reg> assignment;

const Type *i32 = Type::get(Type::i32, {});
const Type *i64 = Type::get(Type::i64, {});
const Type *f32 = Type::get(Type::f32, {});
const Type *vi4 = Type::get(Type::vi4, {});
const Type *vf4 = Type::get(Type::vf4, {});
const Type *unit = Type::get(Type::unit, {});

void Block::insert(iterator at, Op *op) {
  op->parent = this;
  op->place = ops.insert(at, op);
}

void Block::insertAfter(iterator at, Op *op) {
  op->parent = this;
  if (at == ops.end()) {
    ops.push_back(op);
    op->place = --end();
    return;
  }
  op->place = ops.insert(++at, op);
}

void Block::remove(iterator at) {
  ops.erase(at);
}

Block *Block::nextBlock() const {
  if (this == parent->getLastBlock())
    return nullptr;
  
  auto it = place;
  return *++it;
}

Op::~Op() {
  clearResults();
}

Op *Op::prevOp() const {
  auto it = place;
  if (it == parent->begin())
    return nullptr;
  return *--it;
}

Op *Op::nextOp() const {
  auto it = place;
  if (++it == parent->end())
    return nullptr;
  return *it;
}

Value *Op::pushResult(const Type *t) {
  auto value = new Value(t, this, results.size());
  results.push_back(value);
  return value;
}

void Op::removeResult(int i) {
  results.erase(results.begin() + i);
}

void indent(std::ostream &os, int n) {
  for (int j = 0; j < n; j++)
    os << ' ';
}

Region *Op::appendRegion() {
  auto region = new Region(this);
  regions.push_back(region);
  region->appendBlock();
  return region;
}

void Op::pushOperand(Value *v) {
  v->uses.insert(this);
  operands.push_back(v);
}

Op *Op::getParentOp() const {
  auto bb = parent;
  auto region = bb->parent;
  return region->getParentOp();
}

void Op::moveBefore(Op *op) {
  if (op == this)
    return;

  parent->remove(place);
  parent = op->parent;
  parent->insert(op->place, this);
}

void Op::moveAfter(Op *op) {
  if (op == this)
    return;
  
  parent->remove(place);
  parent = op->parent;
  parent->insertAfter(op->place, this);
}

void Op::moveToEnd(Block *block) {
  parent->remove(place);
  parent = block;
  parent->insert(parent->end(), this);
}

void Op::moveToStart(Block *block) {
  parent->remove(place);
  parent = block;
  parent->insert(parent->begin(), this);
}

void Op::clearOperands() {
  for (auto value : operands)
    value->uses.erase(this);
  operands.clear();
}

void Op::clearResults() {
  for (auto x : results) {
    assert(x->uses.empty());
    delete x;
  }
}

void Op::clearAttributes() {
  attrs.clear();
}

void Op::removeAttribute(const std::string &str) {
  attrs.erase(str);
}

void Op::setAttribute(const std::string &name, Attr *attr) {
  attrs[name] = attr;
}

void Op::removeRegion(Region *region) {
  for (auto it = regions.begin(); it != regions.end(); it++) {
    if (*it == region) {
      regions.erase(it);
      break;
    }
  }
}

void Op::setOperand(int i, Value *v) {
  auto value = operands[i];
  operands[i] = v;
  value->uses.erase(this);
  v->uses.insert(this);
}

void Op::removeOperand(int i) {
  auto value = operands[i];
  operands.erase(operands.begin() + i);
  value->uses.erase(this);
}

void Op::removeOperand(Value *v) {
  for (unsigned i = 0; i < operands.size(); i++) {
    auto value = operands[i];
    if (value == v) {
      removeOperand(i);
      return;
    }
  }
  assert(false);
}

int Op::replaceOperand(Value *before, Value *v) {
  for (unsigned i = 0; i < operands.size(); i++) {
    auto value = operands[i];
    if (value == before) {
      setOperand(i, v);
      return i;
    }
  }
  assert(false);
}

void Op::erase() {
  parent->remove(place);
  clearOperands();

  for (auto region : regions)
    region->erase();
  delete this;
}

Block *Op::createFirstBlock() {
  appendRegion();
  return regions[0]->getFirstBlock();
}

std::vector<const Type*> Op::getResultTypes() const {
  std::vector<const Type*> types;
  types.reserve(results.size());
  for (auto x : results)
    types.push_back(x->type);
  return types;
}

void Value::replaceAllUsesWith(Value *other) {
  for (auto use : uses) {
    for (auto &operand : use->operands) {
      if (operand != this)
        continue;

      operand = other;
      other->uses.insert(use);
      if (auto it = assignment.find(this); it != assignment.end())
        assignment[other] = it->second;
    }
  }
  uses.clear();
}

bool Op::inside(Op *op) const {
  for (const Op *runner = this; !isa<ModuleOp>(runner); runner = runner->getParentOp()) {
    if (runner == op)
      return true;
  }
  return false;
}

bool Op::inside(Block *block) const {
  for (const Op *runner = this; !isa<ModuleOp>(runner); runner = runner->getParentOp()) {
    if (runner->parent == block)
      return true;
  }
  return false;
}

void Block::inlineToEnd(Block *bb) {
  for (auto it = begin(); it != end(); ) {
    auto next = it; ++next;
    (*it)->moveToEnd(bb);
    it = next;
  }
}

void Block::inlineBefore(Op *op) {
  for (auto it = begin(); it != end(); ) {
    auto next = it; ++next;
    (*it)->moveBefore(op);
    it = next;
  }
}

void Block::splitOpsAfter(Block *dest, Op *op) {
  for (auto it = op->place; it != end(); ) {
    auto next = it; ++next;
    // `it` invalidates now.
    (*it)->moveToEnd(dest);
    it = next;
  }
}

void Block::splitOpsBefore(Block *dest, Op *op) {
  for (auto it = begin(); it != op->place; ) {
    auto next = it; ++next;
    // `it` invalidates now.
    (*it)->moveToEnd(dest);
    it = next;
  }
}

void Block::moveBefore(Block *bb) {
  parent->remove(place);
  parent = bb->parent;
  parent->insert(bb->place, this);
}

void Block::moveAfter(Block *bb) {
  parent->remove(place);
  parent = bb->parent;
  parent->insertAfter(bb->place, this);
}

void Block::moveToEnd(Region *region) {
  parent->remove(place);
  parent = region;
  parent->insert(parent->end(), this);
}

void Block::prepareErase() {
  for (auto op : ops)
    op->clearOperands();
}

void Block::erase() {
  auto copy = ops;
  for (auto op : copy)
    op->erase();

  parent->remove(place);
  delete this;
}

bool Block::dominatedBy(const Block *bb) const {
  for (auto p = this; p; p = p->idom) {
    if (p == bb)
      return true;
  }
  return false;
}

Block::~Block() {
  for (auto v : args) {
    assert(!v->uses.size());
    delete v;
  }
}

Value *Block::pushArgument(const Type *type) {
  auto value = new Value(type, this, args.size());
  args.push_back(value);
  return value;
}

Block *Region::insert(Block *at) {
  assert(at->parent == this);

  auto it = bbs.insert(at->place, nullptr);
  *it = new Block(this, it);
  return *it;
}

Block *Region::insertAfter(Block *at) {
  assert(at->parent == this);

  if (at->place == end())
    return appendBlock();

  auto place = at->place;
  ++place;
  auto it = bbs.insert(place, nullptr);
  *it = new Block(this, it);
  return *it;
}

void Region::remove(Block *bb) {
  bbs.erase(bb->place);
}

void Region::remove(iterator at) {
  bbs.erase(at);
}

void Region::insert(iterator at, Block *bb) {
  bb->parent = this;
  bb->place = bbs.insert(at, bb);
}

void Region::insertAfter(iterator at, Block *bb) {
  bb->parent = this;
  if (at == bbs.end()) {
    bbs.push_back(bb);
    bb->place = --end();
    return;
  }
  bb->place = bbs.insert(++at, bb);
}

Block *Region::appendBlock() {
  bbs.push_back(nullptr);
  auto place = --bbs.end();
  *place = new Block(this, place);
  return *place;
}

Region::MoveResult Region::moveTo(Block *bb) {
  Block *prev = bb;
  // Preserve it beforehand; the region will become empty afterwards
  MoveResult result { getFirstBlock(), getLastBlock() };

  for (auto it = begin(); it != end(); ) {
    auto next = it; next++;
    auto current = *it;
    current->moveAfter(prev);
    prev = current;
    it = next;
  }

  return result;
}

void Region::erase() {
  for (auto bb : bbs) {
    for (auto op : bb->getOps()) {
      op->clearOperands();
      for (auto region : op->getRegions())
        region->erase();
    }
  }
  auto copy = bbs;
  for (auto bb : copy)
    bb->erase();
  parent->removeRegion(this);
  delete this;
}

void Region::updatePreds() const {
  for (auto bb : bbs) {
    bb->preds.clear();
    bb->succs.clear();
  }

  for (auto bb : bbs) {
    assert(bb->getNumOps() > 0);
    auto last = bb->getLastOp();
    if (auto target = targetOf(last))
      target->preds.insert(bb);
    
    if (auto other = elseOf(last))
      other->preds.insert(bb);
  }

  for (auto bb : bbs) {
    for (auto pred : bb->preds)
      pred->succs.insert(bb);
  }
}

namespace {

// DFN is the number of each node in DFS order.
using DFN = std::unordered_map<Block*, int>;
using BBMap = std::unordered_map<Block*, Block*>;

using Vertex = std::vector<Block*>;

// Semidominator of `u` is the node `v` with the smallest DFN,
// such that `v` dominates `u` on every path not going through its parent in the DFS tree.
using SDom = BBMap;
using Parent = BBMap;
using UnionFind = BBMap;
// Best ancestor found so far.
using Best = BBMap;

int num = 0;
int pnum = 0;

// Dominators.
DFN dfn;
SDom sdom;
Vertex vertex;
Parent parents;
UnionFind uf;
Best best;
// Post-dominators. Just a copy-paste.
DFN pdfn;
SDom psdom;
Vertex pvertex;
Parent pparents;
UnionFind puf;
Best pbest;


void updateDFN(Block *current) {
  dfn[current] = num++;
  vertex.push_back(current);
  for (auto v : current->succs) {
    if (!dfn.count(v)) {
      parents[v] = current;
      updateDFN(v);
    }
  }
}

void updatePDFN(Block *current) {
  pdfn[current] = pnum++;
  pvertex.push_back(current);
  for (auto v : current->preds) {
    if (!pdfn.count(v)) {
      pparents[v] = current;
      updatePDFN(v);
    }
  }
}

Block* find(Block *v) {
  if (uf[v] != v) {
    Block* u = find(uf[v]);
    if (dfn[sdom[best[uf[v]]]] < dfn[sdom[best[v]]])
      best[v] = best[uf[v]];
    uf[v] = u;
  }
  return uf[v];
}

Block* pfind(Block* v) {
  if (puf[v] != v) {
    Block* u = pfind(puf[v]);
    if (pdfn[psdom[pbest[puf[v]]]] < pdfn[psdom[pbest[v]]])
      pbest[v] = pbest[puf[v]];
    puf[v] = u;
  }
  return puf[v];
}


// Links `w` to `v` (setting the father of `w` to `v`).
void link(Block *v, Block *w) {
  uf[w] = v;
}

void plink(Block *v, Block *w) {
  puf[w] = v;
}

}

// Use the Langauer-Tarjan approach.
// https://www.cs.princeton.edu/courses/archive/fall03/cs528/handouts/a%20fast%20algorithm%20for%20finding.pdf
// Loop unrolling might update dominators very frequently, and it's quite time consuming.
void Region::updateDoms() const {
  updatePreds();
  // Clear existing data.
  for (auto bb : bbs) {
    bb->doms.clear();
    bb->idom = nullptr;
  }

  // Clear global data as well.
  dfn.clear();
  vertex.clear();
  parents.clear();
  sdom.clear();
  uf.clear();
  best.clear();
  num = 1;

  // For each `u` as key, it contains all blocks that it semi-dominates.
  // 'b' for bucket.
  std::map<Block*, std::vector<Block*>> bsdom;

  auto entry = getFirstBlock();
  updateDFN(entry);

  for (auto bb : bbs) {
    sdom[bb] = bb;
    uf[bb] = bb;
    best[bb] = bb;
  }

  // Deal with every block in reverse dfn order.
  for (auto it = vertex.rbegin(); it != vertex.rend(); it++) {
    auto bb = *it;
    for (auto v : bb->preds) {
      // Unreachable. Skip it.
      if (!dfn.count(v))
        continue;
      Block *u;
      if (dfn[v] < dfn[bb])
        u = v;
      else {
        find(v);
        u = best[v];
      }
      if (dfn[sdom[u]] < dfn[sdom[bb]])
        sdom[bb] = sdom[u];
    }

    bsdom[sdom[bb]].push_back(bb);
    link(parents[bb], bb);

    for (auto v : bsdom[parents[bb]]) {
      find(v);
      v->idom = sdom[best[v]] == sdom[v] ? parents[bb] : best[v];
    }
  }

  // Find idom, but ignore the entry block (which has no idom).
  for (unsigned i = 1; i < vertex.size(); ++i) {
    auto bb = vertex[i];
    assert(bb->idom);
    if (bb->idom != sdom[bb])
      bb->idom = bb->idom->idom;
  }
}

void Region::updateDomFront() const {
  updateDoms();
  for (auto bb : bbs)
    bb->domFront.clear();

  // Update dominance frontier.
  // See https://en.wikipedia.org/wiki/Static_single-assignment_form#Computing_minimal_SSA_using_dominance_frontiers
  // For each block, if it has at least 2 preds, then it must be at dominance frontier of all its `preds`,
  // till its `idom`.
  for (auto bb : bbs) {
    if (bb->preds.size() < 2)
      continue;

    for (auto pred : bb->preds) {
      auto runner = pred;
      while (runner != bb->idom) {
        runner->domFront.insert(bb);
        runner = runner->idom;
      }
    }
  }
}

// A dual of updateDoms().
void Region::updatePDoms() const {
  updatePreds();

  std::vector<Block*> exits;
  for (auto bb : bbs) {
    if (isa<ReturnOp>(bb->getLastOp()))
      exits.push_back(bb);
  }

  if (exits.size() != 1) {
    std::cerr << "no single exit for pdom\n";
    assert(false);
  }

  auto exit = exits[0];

  for (auto bb : bbs) {
    bb->pdoms.clear();
    bb->ipdom = nullptr;
  }

  pdfn.clear();
  pvertex.clear();
  pparents.clear();
  psdom.clear();
  puf.clear();
  pbest.clear();
  pnum = 1;

  std::map<Block*, std::vector<Block*>> pbsdom;

  updatePDFN(exit);

  for (auto bb : bbs) {
    psdom[bb] = bb;
    puf[bb] = bb;
    pbest[bb] = bb;
  }

  for (auto it = pvertex.rbegin(); it != pvertex.rend(); ++it) {
    auto bb = *it;
    for (auto v : bb->succs) {
      if (!pdfn.count(v))
        continue;
      Block *u;
      if (pdfn[v] < pdfn[bb])
        u = v;
      else {
        pfind(v);
        u = pbest[v];
      }
      if (pdfn[psdom[u]] < pdfn[psdom[bb]])
        psdom[bb] = psdom[u];
    }

    pbsdom[psdom[bb]].push_back(bb);
    plink(pparents[bb], bb);

    for (auto *v : pbsdom[pparents[bb]]) {
      pfind(v);
      v->ipdom = (psdom[pbest[v]] == psdom[v]) ? pparents[bb] : pbest[v];
    }
  }

  for (size_t i = 1; i < pvertex.size(); ++i) {
    auto bb = pvertex[i];
    assert(bb->ipdom);
    if (bb->ipdom != psdom[bb])
      bb->ipdom = bb->ipdom->ipdom;
  }
}

// See the SSA Book:
//   https://pfalcon.github.io/ssabook/latest/book-full.pdf
// Page 116.
void Region::updateLiveness() const {
  updatePreds();

  // Clear existing values.
  for (auto bb : bbs) {
    bb->liveIn.clear();
    bb->liveOut.clear();
  }

  std::map<Block*, std::set<PhiOp*>> phis;
  std::map<Block*, std::set<Value*>> upwardExposed, defined, phidefs;

  for (auto bb : bbs) {
    for (auto op : bb->getOps()) {
      if (auto phi = dyn_cast<PhiOp>(op)) {
        phis[bb].insert(phi);
        phidefs[bb].insert(phi->getResult());
        continue;
      }

      for (auto result : op->getResults())
        defined[bb].insert(result);

      // A value is upward exposed if it's from some block upwards;
      // i.e. it's used but not defined in this block.
      for (auto value : op->getOperands()) {
        if (!defined[bb].count(value))
          upwardExposed[bb].insert(value);
      }
    }
  }

  std::deque<Block*> worklist;
  
  // Do a dataflow approach. We start with all exit blocks;
  // i.e. those that have no successors.
  std::copy_if(bbs.begin(), bbs.end(), std::back_inserter(worklist), [&](Block *bb) {
    return bb->succs.size() == 0;
  });

  bool changed;
  do {
    changed = false;
    for (auto bb : bbs) {
      auto liveInOld = bb->liveIn;

      // LiveOut(B) = \bigcup_{S\in succ(B)} (LiveIn(S) - PhiDefs(S)) \cup PhiUses(B)
      // Here PhiUses(B) means the set of variables used in Phi nodes of S that come from B.
      std::set<Value*> liveOut;
      for (auto succ : bb->succs) {
        std::set_difference(
          succ->liveIn.begin(), succ->liveIn.end(),
          phidefs[succ].begin(), phidefs[succ].end(),
          std::inserter(liveOut, liveOut.end())
        );
        for (auto phi : phis[succ]) {
          auto &ops = phi->getOperands();
          for (size_t i = 0; i < ops.size(); i++) {
            if (phi->targets[i] == bb)
              liveOut.insert(ops[i]);
          }
        }
      }

      bb->liveOut = liveOut;

      // LiveIn(B) = PhiDefs(B) \cup UpwardExposed(B) \cup (LiveOut(B) - Defs(B))
      bb->liveIn.clear();
      std::set_difference(
        liveOut.begin(), liveOut.end(),
        defined[bb].begin(), defined[bb].end(),
        std::inserter(bb->liveIn, bb->liveIn.end())
      );
      for (auto x : upwardExposed[bb])
        bb->liveIn.insert(x);
      for (auto x : phis[bb])
        bb->liveIn.insert(x->getResult());

      if (liveInOld != bb->liveIn)
        changed = true;
    }
  } while (changed);
}

void Region::convertToPhi() {
  updatePreds();

  Builder builder;
  for (auto bb : bbs) {
    builder.setToStart(bb);
    for (auto [i, arg] : data::enumerate(bb->getArgs())) {
      auto phi = builder.create<PhiOp>(arg->type);
      for (auto p : bb->preds) {
        auto last = p->getLastOp();
        auto index = i + isa<BranchOp>(last);
        phi->addIncoming(last->val(index), p);
      }
      arg->replaceAllUsesWith(phi->ret());
    }
    bb->clearArgs();
  }

  // Remove extra arguments.
  for (auto bb : bbs) {
    auto last = bb->getLastOp();
    if (isa<JumpOp>(last)) {
      last->clearOperands();
      continue;
    }
    if (isa<BranchOp>(last)) {
      auto cond = last->val();
      last->clearOperands();
      last->pushOperand(cond);
    }
  }
}

void Region::convertToBlockArguments() {
  updatePreds();

  Builder builder;
  for (auto bb : bbs) {
    std::vector<PhiOp*> phis;
    for (auto it = bb->begin(); it != bb->end(); ) {
      auto next = it; next++;
      auto phi = dyn_cast<PhiOp>(*it);
      if (!phi)
        break;

      auto arg = bb->pushArgument(phi->ret()->type);
      phi->ret()->replaceAllUsesWith(arg);

      for (auto p : bb->preds) {
        Value *v = phi->incomingFrom(p);
        p->getLastOp()->pushOperand(v);
      }
      phi->erase();
      it = next;
    }
  }
}

#define targetof(Ty) \
  case (int) OpKind::Ty: \
    return cast<Ty>(op)->target; \

Block *targetOf(Op *op) {
  switch (op->id) {
  targetful_op_list(targetof)
  branch_op_list(targetof)
  default:
    return nullptr;
  }
}

#define elseof(Ty) \
  case (int) OpKind::Ty: \
    return cast<Ty>(op)->other; \

Block *elseOf(Op *op) {
  switch (op->id) {
  branch_op_list(elseof)
  default:
    return nullptr;
  }
}

#define settarget(Ty) \
  case (int) OpKind::Ty: \
    cast<Ty>(op)->target = bb;

void setTarget(Op *op, Block *bb) {
  switch (op->id) {
  targetful_op_list(settarget)
  branch_op_list(settarget)
  }
}

#define setelse(Ty) \
  case (int) OpKind::Ty: \
    cast<Ty>(op)->other = bb;

void setElse(Op *op, Block *bb) {
  switch (op->id) {
  branch_op_list(setelse)
  }
}

#define rewire_targetful(Ty) \
  case (int) OpKind::Ty: {\
    auto j = cast<Ty>(op); \
    if (j->target == before) \
      j->target = after; \
    break; \
  }

#define rewire_branch(Ty) \
  case (int) OpKind::Ty: {\
    auto j = cast<Ty>(op); \
    if (j->target == before) \
      j->target = after; \
    if (j->other == before) \
      j->other = after; \
    break; \
  }

void Block::rewire(Block *before, Block *after) {
  // Fix the last operation.
  auto op = getLastOp();
  switch (op->id) {
  targetful_op_list(rewire_targetful)
  branch_op_list(rewire_branch)
  default:
    assert(false && "rewire: `before` is not found");
  }
}

int regbank(const Type *ty) {
  if (ty == f32 || ty == vi4 || ty == vf4)
    return FP;
  return INT;
}

}
