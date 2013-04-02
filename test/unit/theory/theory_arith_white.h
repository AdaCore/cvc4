/*********************                                                        */
/*! \file theory_arith_white.h
 ** \verbatim
 ** Original author: Tim King
 ** Major contributors: Morgan Deters
 ** Minor contributors (to current version): Dejan Jovanovic
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2013  New York University and The University of Iowa
 ** See the file COPYING in the top-level source directory for licensing
 ** information.\endverbatim
 **
 ** \brief [[ Add one-line brief description here ]]
 **
 ** [[ Add lengthier description here ]]
 ** \todo document this file
 **/


#include <cxxtest/TestSuite.h>

#include "theory/theory.h"
#include "theory/theory_engine.h"
#include "theory/arith/theory_arith.h"
#include "theory/quantifiers_engine.h"
#include "expr/node.h"
#include "expr/node_manager.h"
#include "context/context.h"
#include "util/rational.h"
#include "smt/smt_engine.h"
#include "smt/smt_engine_scope.h"

#include "theory/theory_test_utils.h"

#include <vector>

using namespace CVC4;
using namespace CVC4::theory;
using namespace CVC4::theory::arith;
using namespace CVC4::expr;
using namespace CVC4::context;
using namespace CVC4::kind;
using namespace CVC4::smt;

using namespace std;

class TheoryArithWhite : public CxxTest::TestSuite {

  Context* d_ctxt;
  UserContext* d_uctxt;
  ExprManager* d_em;
  NodeManager* d_nm;
  SmtScope* d_scope;
  SmtEngine* d_smt;

  TestOutputChannel d_outputChannel;
  LogicInfo d_logicInfo;
  Theory::Effort d_level;

  TheoryArith* d_arith;

  TypeNode* d_booleanType;
  TypeNode* d_realType;

  const Rational d_zero;
  const Rational d_one;

  std::set<Node>* preregistered;

  bool debug;

public:

  TheoryArithWhite() : d_level(Theory::EFFORT_FULL), d_zero(0), d_one(1), debug(false) {}

  void fakeTheoryEnginePreprocess(TNode inp){
    Node rewrite = inp; //FIXME this needs to enforce that inp is fully rewritten already!

    if(debug) cout << rewrite << inp << endl;

    std::list<Node> toPreregister;

    toPreregister.push_back(rewrite);
    for(std::list<Node>::iterator i = toPreregister.begin(); i != toPreregister.end(); ++i){
      Node n = *i;
      preregistered->insert(n);

      for(Node::iterator citer = n.begin(); citer != n.end(); ++citer){
        Node c = *citer;
        if(preregistered->find(c) == preregistered->end()){
          toPreregister.push_back(c);
        }
      }
    }
    for(std::list<Node>::reverse_iterator i = toPreregister.rbegin(); i != toPreregister.rend(); ++i){
      Node n = *i;
      if(debug) cout << n.getId() << " "<< n << endl;
      d_arith->preRegisterTerm(n);
    }
  }

  void setUp() {
    d_em = new ExprManager();
    d_nm = NodeManager::fromExprManager(d_em);
    d_smt = new SmtEngine(d_em);
    d_ctxt = d_smt->d_context;
    d_uctxt = d_smt->d_userContext;
    d_scope = new SmtScope(d_smt);
    d_outputChannel.clear();
    d_logicInfo.lock();

    // guard against duplicate statistics assertion errors
    delete d_smt->d_theoryEngine->d_theoryTable[THEORY_ARITH];
    delete d_smt->d_theoryEngine->d_theoryOut[THEORY_ARITH];
    d_smt->d_theoryEngine->d_theoryTable[THEORY_ARITH] = NULL;
    d_smt->d_theoryEngine->d_theoryOut[THEORY_ARITH] = NULL;

    d_arith = new TheoryArith(d_ctxt, d_uctxt, d_outputChannel, Valuation(NULL), d_logicInfo, d_smt->d_theoryEngine->d_quantEngine);

    preregistered = new std::set<Node>();

    d_booleanType = new TypeNode(d_nm->booleanType());
    d_realType = new TypeNode(d_nm->realType());

  }

  void tearDown() {
    delete d_realType;
    delete d_booleanType;

    delete preregistered;

    delete d_arith;
    d_outputChannel.clear();
    delete d_scope;
    delete d_smt;
    delete d_em;
  }

  void testAssert() {
    Node x = d_nm->mkVar(*d_realType);
    Node c = d_nm->mkConst<Rational>(d_zero);

    Node gt = d_nm->mkNode(GT, x, c);
    Node leq = gt.notNode();
    fakeTheoryEnginePreprocess(leq);

    d_arith->assertFact(leq, true);

    d_arith->check(d_level);

    TS_ASSERT_EQUALS(d_outputChannel.getNumCalls(), 0u);
  }

  Node simulateSplit(TNode l, TNode r){
    Node eq = d_nm->mkNode(EQUAL, l, r);
    Node lt = d_nm->mkNode(LT, l, r);
    Node gt = d_nm->mkNode(GT, l, r);

    Node dis = d_nm->mkNode(OR, eq, lt, gt);
    return dis;
  }

  void testTPLt1() {
    Node x = d_nm->mkVar(*d_realType);
    Node c0 = d_nm->mkConst<Rational>(d_zero);
    Node c1 = d_nm->mkConst<Rational>(d_one);

    Node gt0 = d_nm->mkNode(GT, x, c0);
    Node gt1 = d_nm->mkNode(GT, x, c1);
    Node geq1 = d_nm->mkNode(GEQ, x, c1);
    Node leq0 = gt0.notNode();
    Node leq1 = gt1.notNode();
    Node lt1 = geq1.notNode();

    fakeTheoryEnginePreprocess(leq0);
    fakeTheoryEnginePreprocess(leq1);
    fakeTheoryEnginePreprocess(geq1);

    d_arith->presolve();
    d_arith->assertFact(lt1, true);
    d_arith->check(d_level);
    d_arith->propagate(d_level);


    Node gt0Orlt1  = NodeBuilder<2>(OR) << gt0 << lt1;
    Node geq0OrLeq1  = NodeBuilder<2>(OR) << geq1 << leq1;

    cout << d_outputChannel.getIthNode(0) << endl << endl;
    cout << d_outputChannel.getIthNode(1) << endl << endl;
    cout << d_outputChannel.getIthNode(2) << endl << endl;

    TS_ASSERT_EQUALS(d_outputChannel.getNumCalls(), 3u);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(0), LEMMA);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(0), gt0Orlt1);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(1), LEMMA);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(1), geq0OrLeq1);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(2), PROPAGATE);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(2), leq1);
  }


  void testTPLeq0() {
    Node x = d_nm->mkVar(*d_realType);
    Node c0 = d_nm->mkConst<Rational>(d_zero);
    Node c1 = d_nm->mkConst<Rational>(d_one);

    Node gt0 = d_nm->mkNode(GT, x, c0);
    Node gt1 = d_nm->mkNode(GT, x, c1);
    Node geq1 = d_nm->mkNode(GEQ, x, c1);
    Node leq0 = gt0.notNode();
    Node leq1 = gt1.notNode();
    Node lt1 = geq1.notNode();

    fakeTheoryEnginePreprocess(leq0);
    fakeTheoryEnginePreprocess(leq1);
    fakeTheoryEnginePreprocess(geq1);

    d_arith->presolve();
    d_arith->assertFact(leq0, true);
    d_arith->check(d_level);
    d_arith->propagate(d_level);

    Node gt0Orlt1  = NodeBuilder<2>(OR) << gt0 << lt1;
    Node geq0OrLeq1  = NodeBuilder<2>(OR) << geq1 << leq1;

    cout << d_outputChannel.getIthNode(0) << endl;
    cout << d_outputChannel.getIthNode(1) << endl;
    cout << d_outputChannel.getIthNode(2) << endl;
    cout << d_outputChannel.getIthNode(3) << endl;

    TS_ASSERT_EQUALS(d_outputChannel.getNumCalls(), 4u);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(0), LEMMA);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(0), gt0Orlt1);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(1), LEMMA);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(1), geq0OrLeq1);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(2), PROPAGATE);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(2), lt1 );

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(3), PROPAGATE);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(3), leq1);
  }

  void testTPLeq1() {
    Node x = d_nm->mkVar(*d_realType);
    Node c0 = d_nm->mkConst<Rational>(d_zero);
    Node c1 = d_nm->mkConst<Rational>(d_one);

    Node gt0 = d_nm->mkNode(GT, x, c0);
    Node gt1 = d_nm->mkNode(GT, x, c1);
    Node geq1 = d_nm->mkNode(GEQ, x, c1);
    Node leq0 = gt0.notNode();
    Node leq1 = gt1.notNode();
    Node lt1 = geq1.notNode();

    fakeTheoryEnginePreprocess(leq0);
    fakeTheoryEnginePreprocess(leq1);
    fakeTheoryEnginePreprocess(geq1);

    d_arith->presolve();
    d_arith->assertFact(leq1, true);
    d_arith->check(d_level);
    d_arith->propagate(d_level);

    Node gt0Orlt1  = NodeBuilder<2>(OR) << gt0 << lt1;
    Node geq0OrLeq1  = NodeBuilder<2>(OR) << geq1 << leq1;

    cout << d_outputChannel.getIthNode(0) << endl;
    cout << d_outputChannel.getIthNode(1) << endl;

    TS_ASSERT_EQUALS(d_outputChannel.getNumCalls(), 2u);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(0), LEMMA);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(0), gt0Orlt1);

    TS_ASSERT_EQUALS(d_outputChannel.getIthCallType(1), LEMMA);
    TS_ASSERT_EQUALS(d_outputChannel.getIthNode(1), geq0OrLeq1);
  }
};
