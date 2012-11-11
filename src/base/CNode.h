//
//    MINOTAUR -- It's only 1/2 bull
//
//    (C)opyright 2008 - 2012 The MINOTAUR Team.
//


/**
 * \file CNode.h
 * \brief Declare class CNode to represent node of a computational graph of a
 * nonlinear function.
 * \author Ashutosh Mahajan, Argonne National Laboratory
 */

#ifndef MINOTAURCNODE_H
#define MINOTAURCNODE_H

#include "OpCode.h"
#include "Types.h"

namespace Minotaur {

class Variable;
class CNode;

struct CompareCNodes {
  Bool operator()(const CNode* n1, const CNode *n2) const;
};
struct CompareCNodesR {
  Bool operator()(const CNode* n1, const CNode *n2) const;
};
typedef boost::shared_ptr<Variable> VariablePtr;
typedef std::set<CNode *, CompareCNodes> CNodeSet;
typedef std::set<CNode *, CompareCNodesR> CNodeRSet;

class CQIter2 {
public:
  CNode *node;
  CQIter2 *next;
  CQIter2 *prev;
};

class CNode {
public:

  /// Default constructor
  CNode();

  /**
   * Constructor with a specific opcode and children. Children can be
   * NULL, and num_child can be zero.
   */
  CNode(OpCode op, CNode **children, UInt num_child);
  CNode(OpCode op, CNode *lchild, CNode *rchild);
  ~CNode();


  CNode *clone() const;
  void copyParChild(CNode *out, std::map<const CNode*, CNode*> *nmap) const;
  void addPar(CNode *node);
  void eval(const Double *x, Int *error);
  Double eval(Double x, Int *error) const;
  FunctionType findFType();
  void fwdGrad();
  Bool getB() const { return b_; };
  Double getG() const { return g_; };
  Double getH() const {return h_;};
  UInt getId() const {return id_;};
  CNode* getL() const { return l_; };
  Double getLb() const { return lb_; };
  CNode** getListL() const { return child_; };
  CNode** getListR() const { return child_+numChild_; };
  OpCode getOp() const { return op_; };
  CQIter2* getParB() const { return parB_; };
  CNode* getR() const { return r_; };
  Int getTempI() const { return ti_; };
  FunctionType getType() const { return fType_; } ;
  Double getUb() const { return ub_; };
  CNode* getUPar() const { return uPar_; };
  const Variable* getV() const { return v_; };
  Double getVal() const { return val_; };
  void grad(Int *error);
  UInt numChild() const;
  UInt numPar() const { return numPar_; };
  void propHessSpa();
  void propHessSpa2(CNodeRSet *nset);
  void propBounds(Int *error);
  void hess(Int *error);
  void hess2(CNodeRSet *nset, Int *error);
  void setB(Bool b) {b_ = b;};
  void setBounds(Double lb, Double ub) {lb_ = lb; ub_ = ub; };
  void setDouble(Double d) {d_ = d;};
  void setG(Double g) {g_ = g;};
  void setGi(Double gi) {gi_ = gi;};
  void setH(Double h) {h_ = h;};
  void setId(UInt i) {id_ = i;};
  void setInt(Int i) {i_ = i;};
  void setL(CNode *n) {l_ = n;};
  void setOp(OpCode op) {op_ = op;};
  void setR(CNode *n) {r_ = n;};
  void setTempI(Int i) { ti_ = i; };
  void setType(FunctionType t);
  void setVal(Double v);
  void setV(VariablePtr v) {v_ = v.get();};
  void updateBnd(Int *error);

  void write(std::ostream &out) const;
  void writeSubExp(std::ostream &out) const;

protected:
  Bool b_;
  CNode **child_; // array of size numChild_ + 1
  Double d_;
  FunctionType fType_;
  Double g_;
  Double gi_;
  Double h_;
  Int i_;
  UInt id_;
  CNode *l_;
  Double lb_;
  UInt numChild_;
  UInt numPar_;
  OpCode op_;
  CQIter2 *parB_;
  CQIter2 *parE_;
  CNode *r_;
  Int ti_;
  Double ub_;
  CNode *uPar_;
  const Variable* v_;
  Double val_;
  void propBounds_(Double lb, Double ub, Int *error);

};
typedef std::vector<CNode*> CNodeVector;
}


#endif

// Local Variables: 
// mode: c++ 
// eval: (c-set-style "k&r") 
// eval: (c-set-offset 'innamespace 0) 
// eval: (setq c-basic-offset 2) 
// eval: (setq fill-column 78) 
// eval: (auto-fill-mode 1) 
// eval: (setq column-number-mode 1) 
// eval: (setq indent-tabs-mode nil) 
// End: