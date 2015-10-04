// -*-  indent-tabs-mode:nil; c-basic-offset:4; -*-
//------------------------------------------------------------------------------
// Add MC2 annotations to C code.
// Copyright 2015 Patrick Lam <prof.lam@gmail.com>
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal with the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimers.
//
// Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimers in
// the documentation and/or other materials provided with the
// distribution.
//
// Neither the names of the University of Waterloo, nor the names of
// its contributors may be used to endorse or promote products derived
// from this Software without specific prior written permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
// Patrick Lam (prof.lam@gmail.com)
//
// Base code:
// Eli Bendersky (eliben@gmail.com)
//
//------------------------------------------------------------------------------
#include <sstream>
#include <string>
#include <map>
#include <stdbool.h>

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/STLExtras.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

static LangOptions LangOpts;
static llvm::cl::OptionCategory AddMC2AnnotationsCategory("Add MC2 Annotations");

static std::string encode(std::string varName) {
    std::stringstream nn;
    nn << "_m" << varName;
    return nn.str();
}

static int fnCount;
static std::string encodeFn(int num) {
    std::stringstream nn;
    nn << "_fn" << num;
    return nn.str();
};

static int ptrCount;
static std::string encodePtr(int num) {
    std::stringstream nn;
    nn << "_p" << num;
    return nn.str();
};

static int rmwCount;
static std::string encodeRMW(int num) {
    std::stringstream nn;
    nn << "_rmw" << num;
    return nn.str();
};

static int branchCount;
static std::string encodeBranch(int num) {
    std::stringstream nn;
    nn << "_br" << num;
    return nn.str();
};

static int condCount;
static std::string encodeCond(int num) {
    std::stringstream nn;
    nn << "_cond" << num;
    return nn.str();
};

static int rvCount;
static std::string encodeRV(int num) {
    std::stringstream nn;
    nn << "_rv" << num;
    return nn.str();
};

static int funcCount;

struct ProvisionalName {
    int index, length;
    const DeclRefExpr * pname;
    bool enabled;

    ProvisionalName(int index, const DeclRefExpr * pname) : index(index), pname(pname), length(encode(pname->getNameInfo().getName().getAsString()).length()), enabled(true) {}
    ProvisionalName(int index, const DeclRefExpr * pname, int length) : index(index), pname(pname), length(length), enabled(true) {}
};

struct Update {
    SourceLocation loc;
    std::string update;
    std::vector<ProvisionalName *> * pnames;

    Update(SourceLocation loc, std::string update, std::vector<ProvisionalName *> * pnames) : 
        loc(loc), update(update), pnames(pnames) {}

    ~Update() { 
        for (auto pname : *pnames) delete pname;
        delete pnames; 
    }
};

void updateProvisionalName(std::vector<Update *> &DeferredUpdates, const ValueDecl * now_known, std::string mcVar) {
    for (Update * u : DeferredUpdates) {
        for (int i = 0; i < u->pnames->size(); i++) {
            ProvisionalName * v = (*(u->pnames))[i];
            if (!v->enabled) continue;
            if (now_known == v->pname->getDecl()) {
                v->enabled = false;
                std::string oldName = encode(v->pname->getNameInfo().getName().getAsString());

                u->update.replace(v->index, v->length, mcVar);
                for (int j = i+1; j < u->pnames->size(); j++) {
                    ProvisionalName * vv = (*(u->pnames))[j];
                    if (vv->index > v->index)
                        vv->index -= v->length - mcVar.length();
                }
            }
        }
    }
}

static const VarDecl * retrieveSingleDecl(const DeclStmt * s) {
    // XXX iterate through all decls defined in s, not just the first one
    assert(s->isSingleDecl() && isa<VarDecl>(s->getSingleDecl()) && "unsupported form of decl");
    if (s->isSingleDecl() && isa<VarDecl>(s->getSingleDecl())) {
        return cast<VarDecl>(s->getSingleDecl());
    } else return NULL;
}

class FindCallArgVisitor : public RecursiveASTVisitor<FindCallArgVisitor> {
public:
    FindCallArgVisitor() : DE(NULL), UnaryOp(NULL) {}

    bool VisitStmt(Stmt * s) {
        if (!UnaryOp) {
            if (UnaryOperator * uo = dyn_cast<UnaryOperator>(s)) {
                if (uo->getOpcode() == UnaryOperatorKind::UO_AddrOf ||
                    uo->getOpcode() == UnaryOperatorKind::UO_Deref)
                    UnaryOp = uo;
            }
        }

        if (!DE && (DE = dyn_cast<DeclRefExpr>(s)))
            ;
        return true;
    }

    void Clear() {
        UnaryOp = NULL; DE = NULL;
    }

    const UnaryOperator * RetrieveUnaryOp() {
        if (UnaryOp) {
            bool found = false;
            const Stmt * s = UnaryOp;
            while (s != NULL) {
                if (s == DE) {
                    found = true; break;
                }

                if (const UnaryOperator * op = dyn_cast<UnaryOperator>(s))
                    s = op->getSubExpr();
                else if (const CastExpr * op = dyn_cast<CastExpr>(s))
                    s = op->getSubExpr();
                else if (const MemberExpr * op = dyn_cast<MemberExpr>(s))
                    s = op->getBase();
                else
                    s = NULL;
            }
            if (found)
                return UnaryOp;
        }
        return NULL;
    }

    const DeclRefExpr * RetrieveDeclRefExpr() {
        return DE;
    }

private:
    const UnaryOperator * UnaryOp;
    const DeclRefExpr * DE;
};

class FindLocalsVisitor : public RecursiveASTVisitor<FindLocalsVisitor> {
public:
    FindLocalsVisitor() : Vars() {}

    bool VisitDeclRefExpr(DeclRefExpr * de) {
        Vars.push_back(de->getDecl());
        return true;
    }

    void Clear() {
        Vars.clear();
    }

    const TinyPtrVector<const NamedDecl *> RetrieveVars() {
        return Vars;
    }

private:
    TinyPtrVector<const NamedDecl *> Vars;
};


class MallocHandler : public MatchFinder::MatchCallback {
public:
    MallocHandler(std::set<const Expr *> & MallocExprs) :
        MallocExprs(MallocExprs) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        const CallExpr * ce = Result.Nodes.getNodeAs<CallExpr>("callExpr");

        MallocExprs.insert(ce);
    }

    private:
    std::set<const Expr *> &MallocExprs;
};

static void generateMC2Function(Rewriter & Rewrite,
                                const Expr * e, 
                                SourceLocation loc,
                                std::string tmpname, 
                                std::string tmpFn, 
                                const DeclRefExpr * lhs, 
                                std::string lhsName,
                                std::vector<ProvisionalName *> * vars1,
                                std::vector<Update *> & DeferredUpdates) {
    // prettyprint the LHS (&newnode->value)
    // e.g. int * _tmp0 = &newnode->value;
    std::string SStr;
    llvm::raw_string_ostream S(SStr);
    e->printPretty(S, nullptr, Rewrite.getLangOpts());
    const std::string &Str = S.str();

    std::stringstream prel;
    prel << "\nvoid * " << tmpname << " = " << Str << ";\n";

    // MCID _p0 = MC2_function(1, MC2_PTR_LENGTH, _tmp0, _fn0);
    prel << "MCID " << tmpFn << " = MC2_function_id(" << ++funcCount << ", 1, MC2_PTR_LENGTH, " << tmpname << ", ";
    if (lhs) {
        // XXX generate casts when they'd eliminate warnings
        ProvisionalName * v = new ProvisionalName(prel.tellp(), lhs);
        vars1->push_back(v);
    }
    prel << encode(lhsName) << "); ";

    Update * u = new Update(loc, prel.str(), vars1);
    DeferredUpdates.push_back(u);
}

class LoadHandler : public MatchFinder::MatchCallback {
public:
    LoadHandler(Rewriter &Rewrite,
                std::set<const NamedDecl *> & DeclsRead,
                std::set<const VarDecl *> & DeclsNeedingMC, 
                std::map<const NamedDecl *, std::string> &DeclToMCVar,
                std::map<const Expr *, std::string> &ExprToMCVar,
                std::set<const Stmt *> & StmtsHandled,
                std::map<const Expr *, SourceLocation> &Redirector,
                std::vector<Update *> & DeferredUpdates) : 
        Rewrite(Rewrite), DeclsRead(DeclsRead), DeclsNeedingMC(DeclsNeedingMC), DeclToMCVar(DeclToMCVar),
        ExprToMCVar(ExprToMCVar),
        StmtsHandled(StmtsHandled), Redirector(Redirector), DeferredUpdates(DeferredUpdates) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        std::vector<ProvisionalName *> * vars = new std::vector<ProvisionalName *>();
        CallExpr * ce = const_cast<CallExpr *>(Result.Nodes.getNodeAs<CallExpr>("callExpr"));
        const VarDecl * d = Result.Nodes.getNodeAs<VarDecl>("decl");
        const Stmt * s = Result.Nodes.getNodeAs<Stmt>("containingStmt");
        const Expr * lhs = NULL;
        if (s && isa<BinaryOperator>(s)) lhs = cast<BinaryOperator>(s)->getLHS();
        if (!s) s = ce;

        const DeclRefExpr * rhs = NULL;
        MemberExpr * ml = NULL;
        bool isAddrOfR = false, isAddrMemberR = false;

        StmtsHandled.insert(s);
        
        std::string n, n_decl;
        if (lhs) {
            FindCallArgVisitor fcaVisitor;
            fcaVisitor.Clear();
            fcaVisitor.TraverseStmt(ce->getArg(0));
            rhs = cast<DeclRefExpr>(fcaVisitor.RetrieveDeclRefExpr()->IgnoreParens());
            const UnaryOperator * ruop = fcaVisitor.RetrieveUnaryOp();
            isAddrOfR = ruop && ruop->getOpcode() == UnaryOperatorKind::UO_AddrOf;
            isAddrMemberR = ruop && isa<MemberExpr>(ruop->getSubExpr());
            if (isAddrMemberR)
                ml = dyn_cast<MemberExpr>(ruop->getSubExpr());

            FindLocalsVisitor flv;
            flv.Clear();
            flv.TraverseStmt(const_cast<Stmt*>(cast<Stmt>(lhs)));
            for (auto & d : flv.RetrieveVars()) {
                const VarDecl * dd = cast<VarDecl>(d);
                n = dd->getName();
                // XXX todo rhs for non-decl stmts
                if (!isa<ParmVarDecl>(dd))
                    DeclsNeedingMC.insert(dd);
                DeclsRead.insert(d);
                DeclToMCVar[dd] = encode(n);
            }
        } else {
            FindCallArgVisitor fcaVisitor;
            fcaVisitor.Clear();
            fcaVisitor.TraverseStmt(ce->getArg(0));
            rhs = cast<DeclRefExpr>(fcaVisitor.RetrieveDeclRefExpr()->IgnoreParens());
            const UnaryOperator * ruop = fcaVisitor.RetrieveUnaryOp();
            isAddrOfR = ruop && ruop->getOpcode() == UnaryOperatorKind::UO_AddrOf;
            isAddrMemberR = ruop && isa<MemberExpr>(ruop->getSubExpr());
            if (isAddrMemberR)
                ml = dyn_cast<MemberExpr>(ruop->getSubExpr());

            if (d) {
                n = d->getName();
                DeclsNeedingMC.insert(d);
                DeclsRead.insert(d);
                DeclToMCVar[d] = encode(n);
            } else {
                n = ExprToMCVar[ce];
                fcaVisitor.Clear();
                fcaVisitor.TraverseStmt(ce);
                const DeclRefExpr * dd = cast<DeclRefExpr>(fcaVisitor.RetrieveDeclRefExpr()->IgnoreParens());
                updateProvisionalName(DeferredUpdates, dd->getDecl(), encode(n));
                DeclToMCVar[dd->getDecl()] = encode(n);
                n_decl = "MCID ";
            }
        }
        
        std::stringstream nol;

        if (lhs && isa<DeclRefExpr>(lhs)) {
            const DeclRefExpr * ll = cast<DeclRefExpr>(lhs);
            ProvisionalName * v = new ProvisionalName(nol.tellp(), ll);
            vars->push_back(v);
        }

        if (rhs) {
            if (isAddrMemberR) {
                if (!n.empty()) 
                    nol << n_decl << encode(n) << "=";
                nol << "MC2_nextOpLoadOffset(";

                ProvisionalName * v = new ProvisionalName(nol.tellp(), rhs);
                vars->push_back(v);
                nol << encode(rhs->getNameInfo().getName().getAsString());

                nol << ", MC2_OFFSET(";
                nol << ml->getBase()->getType().getAsString();
                nol << ", ";
                nol << ml->getMemberDecl()->getName().str();
                nol << ")";
            } else if (!isAddrOfR) {
                if (!n.empty()) 
                    nol << n_decl << encode(n) << "=";
                nol << "MC2_nextOpLoad(";
                ProvisionalName * v = new ProvisionalName(nol.tellp(), rhs);
                vars->push_back(v);
                nol << encode(rhs->getNameInfo().getName().getAsString());
            } else {
                if (!n.empty()) 
                    nol << n_decl << encode(n) << "=";
                nol << "MC2_nextOpLoad(";
                nol << "MCID_NODEP";
            }
        } else {
            if (!n.empty()) 
                nol << n_decl << encode(n) << "=";
            nol << "MC2_nextOpLoad(";
            nol << "MCID_NODEP";
        }

        if (lhs)
            nol << "), ";
        else
            nol << "); ";
        SourceLocation ss = s->getLocStart();
        // eek gross hack:
        // if the load appears as its own stmt and is the 1st stmt, containingStmt may be the containing CompoundStmt;
        // move over 1 so that we get the right location.
        if (isa<CompoundStmt>(s)) ss = ss.getLocWithOffset(1);
        const Expr * e = dyn_cast<Expr>(s);
        if (e && Redirector.count(e) > 0)
            ss = Redirector[e];
        Update * u = new Update(ss, nol.str(), vars);
        DeferredUpdates.insert(DeferredUpdates.begin(), u);
    }

    private:
    Rewriter &Rewrite;
    std::set<const NamedDecl *> & DeclsRead;
    std::set<const VarDecl *> & DeclsNeedingMC;
    std::map<const Expr *, std::string> &ExprToMCVar;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::set<const Stmt *> &StmtsHandled;
    std::vector<Update *> &DeferredUpdates;
    std::map<const Expr *, SourceLocation> &Redirector;
};

class StoreHandler : public MatchFinder::MatchCallback {
public:
    StoreHandler(Rewriter &Rewrite,
                 std::set<const NamedDecl *> & DeclsRead,
                 std::set<const VarDecl *> &DeclsNeedingMC,
                 std::vector<Update *> & DeferredUpdates) : 
        Rewrite(Rewrite), DeclsRead(DeclsRead), DeclsNeedingMC(DeclsNeedingMC), DeferredUpdates(DeferredUpdates) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        std::vector<ProvisionalName *> * vars = new std::vector<ProvisionalName *>();
        CallExpr * ce = const_cast<CallExpr *>(Result.Nodes.getNodeAs<CallExpr>("callExpr"));

        fcaVisitor.Clear();
        fcaVisitor.TraverseStmt(ce->getArg(0));
        const DeclRefExpr * lhs = fcaVisitor.RetrieveDeclRefExpr();
        const UnaryOperator * luop = fcaVisitor.RetrieveUnaryOp();
    
        std::stringstream nol;

        bool isAddrMemberL;
        bool isAddrOfL;

        if (luop && luop->getOpcode() == UnaryOperatorKind::UO_AddrOf) {
            isAddrMemberL = isa<MemberExpr>(luop->getSubExpr());
            isAddrOfL = !isa<MemberExpr>(luop->getSubExpr());
        }

        if (lhs) {
            if (isAddrOfL) {
                nol << "MC2_nextOpStore(";
                nol << "MCID_NODEP";
            } else {
                if (isAddrMemberL) {
                    MemberExpr * ml = cast<MemberExpr>(luop->getSubExpr());

                    nol << "MC2_nextOpStoreOffset(";

                    ProvisionalName * v = new ProvisionalName(nol.tellp(), lhs);
                    vars->push_back(v);

                    nol << encode(lhs->getNameInfo().getName().getAsString());
                    if (!isa<ParmVarDecl>(lhs->getDecl()))
                        DeclsNeedingMC.insert(cast<VarDecl>(lhs->getDecl()));

                    nol << ", MC2_OFFSET(";
                    nol << ml->getBase()->getType().getAsString();
                    nol << ", ";
                    nol << ml->getMemberDecl()->getName().str();
                    nol << ")";
                } else {
                    nol << "MC2_nextOpStore(";
                    ProvisionalName * v = new ProvisionalName(nol.tellp(), lhs);
                    vars->push_back(v);

                    nol << encode(lhs->getNameInfo().getName().getAsString());
                }
            }
        }
        else {
            nol << "MC2_nextOpStore(";
            nol << "MCID_NODEP";
        }
        
        nol << ", ";

        fcaVisitor.Clear();
        fcaVisitor.TraverseStmt(ce->getArg(1));
        const DeclRefExpr * rhs = fcaVisitor.RetrieveDeclRefExpr();
        const UnaryOperator * ruop = fcaVisitor.RetrieveUnaryOp();

        bool isAddrOfR = ruop && ruop->getOpcode() == UnaryOperatorKind::UO_AddrOf;
        bool isDerefR = ruop && ruop->getOpcode() == UnaryOperatorKind::UO_Deref;

        if (rhs && !isAddrOfR) {
            assert (!isDerefR && "Must use atomic load for dereferences!");
            ProvisionalName * v = new ProvisionalName(nol.tellp(), rhs);
            vars->push_back(v);

            nol << encode(rhs->getNameInfo().getName().getAsString());
            DeclsRead.insert(rhs->getDecl());
        }
        else
            nol << "MCID_NODEP";
        
        nol << ");\n";
        Update * u = new Update(ce->getLocStart(), nol.str(), vars);
        DeferredUpdates.push_back(u);
    }

    private:
    Rewriter &Rewrite;
    FindCallArgVisitor fcaVisitor;
    std::set<const NamedDecl *> & DeclsRead;
    std::set<const VarDecl *> & DeclsNeedingMC;
    std::vector<Update *> &DeferredUpdates;
};

class RMWHandler : public MatchFinder::MatchCallback {
public:
    RMWHandler(Rewriter &rewrite,
                 std::set<const NamedDecl *> & DeclsRead,
                 std::set<const NamedDecl *> & DeclsInCond,
                 std::map<const NamedDecl *, std::string> &DeclToMCVar,
                 std::map<const Expr *, std::string> &ExprToMCVar,
                 std::set<const Stmt *> & StmtsHandled,
                 std::map<const Expr *, SourceLocation> &Redirector,
                 std::vector<Update *> & DeferredUpdates) : 
        rewrite(rewrite), DeclsRead(DeclsRead), DeclsInCond(DeclsInCond), DeclToMCVar(DeclToMCVar), 
        ExprToMCVar(ExprToMCVar),
        StmtsHandled(StmtsHandled), Redirector(Redirector), DeferredUpdates(DeferredUpdates) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        CallExpr * ce = const_cast<CallExpr *>(Result.Nodes.getNodeAs<CallExpr>("callExpr"));
        const Stmt * s = Result.Nodes.getNodeAs<Stmt>("containingStmt");
        std::vector<ProvisionalName *> * vars = new std::vector<ProvisionalName *>();

        std::stringstream nol;
        
        std::string rmwMCVar;
        rmwMCVar = encodeRMW(rmwCount++);

        const VarDecl * rmw_lhs;
        if (s) {
            StmtsHandled.insert(s);
            assert (isa<DeclStmt>(s) || isa<BinaryOperator>(s) && "unknown RMW format: not declrefexpr, not binaryoperator");
            const DeclStmt * ds;
            if ((ds = dyn_cast<DeclStmt>(s))) {
                rmw_lhs = retrieveSingleDecl(ds);
            } else {
                const Expr * e = cast<BinaryOperator>(s)->getLHS();
                assert (isa<DeclRefExpr>(e));
                rmw_lhs = cast<VarDecl>(cast<DeclRefExpr>(e)->getDecl());
            }
            DeclToMCVar[rmw_lhs] = rmwMCVar;
        }

        // retrieve effective LHS of the RMW
        fcaVisitor.Clear();
        fcaVisitor.TraverseStmt(ce->getArg(1));
        const DeclRefExpr * elhs = fcaVisitor.RetrieveDeclRefExpr();
        const UnaryOperator * eluop = fcaVisitor.RetrieveUnaryOp();
        bool isAddrMemberL = false;

        if (eluop && eluop->getOpcode() == UnaryOperatorKind::UO_AddrOf) {
            isAddrMemberL = isa<MemberExpr>(eluop->getSubExpr());
        }

        nol << "MCID " << rmwMCVar;
        if (isAddrMemberL) {
            MemberExpr * ml = cast<MemberExpr>(eluop->getSubExpr());

            nol << " = MC2_nextRMWOffset(";

            ProvisionalName * v = new ProvisionalName(nol.tellp(), elhs);
            vars->push_back(v);

            nol << encode(elhs->getNameInfo().getName().getAsString());

            nol << ", MC2_OFFSET(";
            nol << ml->getBase()->getType().getAsString();
            nol << ", ";
            nol << ml->getMemberDecl()->getName().str();
            nol << ")";
        } else {
            nol << " = MC2_nextRMW(";
            bool isAddrOfL = eluop && eluop->getOpcode() == UnaryOperatorKind::UO_AddrOf;

            if (elhs) {
                if (isAddrOfL)
                    nol << "MCID_NODEP";
                else {
                    ProvisionalName * v = new ProvisionalName(nol.tellp(), elhs);
                    vars->push_back(v);

                    std::string elhsName = encode(elhs->getNameInfo().getName().getAsString());
                    nol << elhsName;
                }
            }
            else
                nol << "MCID_NODEP";
        }
        nol << ", ";

        // handle both RHS ops
        int outputted = 0;
        for (int arg = 2; arg < 4; arg++) {
            fcaVisitor.Clear();
            fcaVisitor.TraverseStmt(ce->getArg(arg));
            const DeclRefExpr * a = fcaVisitor.RetrieveDeclRefExpr();
            const UnaryOperator * op = fcaVisitor.RetrieveUnaryOp();
            
            bool isAddrOfR = op && op->getOpcode() == UnaryOperatorKind::UO_AddrOf;
            bool isDerefR = op && op->getOpcode() == UnaryOperatorKind::UO_Deref;

            if (a && !isAddrOfR) {
                assert (!isDerefR && "Must use atomic load for dereferences!");

                DeclsInCond.insert(a->getDecl());

                if (outputted > 0) nol << ", ";
                outputted++;

                bool alreadyMCVar = false;
                if (DeclToMCVar.find(a->getDecl()) != DeclToMCVar.end()) {
                    alreadyMCVar = true;
                    nol << DeclToMCVar[a->getDecl()];
                }
                else {
                    std::string an = "MCID_NODEP";
                    ProvisionalName * v = new ProvisionalName(nol.tellp(), a, an.length());
                    nol << an;
                    vars->push_back(v);
                }

                DeclsRead.insert(a->getDecl());
            }
            else {
                if (outputted > 0) nol << ", ";
                outputted++;

                nol << "MCID_NODEP";
            }
        }
        nol << ");\n";

        SourceLocation place = s ? s->getLocStart() : ce->getLocStart();
        const Expr * e = s ? dyn_cast<Expr>(s) : ce;
        if (e && Redirector.count(e) > 0)
            place = Redirector[e];
        Update * u = new Update(place, nol.str(), vars);
        DeferredUpdates.insert(DeferredUpdates.begin(), u);
    }
    
    private:
    Rewriter &rewrite;
    FindCallArgVisitor fcaVisitor;
    std::set<const NamedDecl *> &DeclsRead;
    std::set<const NamedDecl *> &DeclsInCond;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::map<const Expr *, std::string> &ExprToMCVar;
    std::set<const Stmt *> &StmtsHandled;
    std::vector<Update *> &DeferredUpdates;
    std::map<const Expr *, SourceLocation> &Redirector;
};

class FindReturnsBreaksVisitor : public RecursiveASTVisitor<FindReturnsBreaksVisitor> {
public:
    FindReturnsBreaksVisitor() : Returns(), Breaks() {}

    bool VisitStmt(Stmt * s) {
        if (isa<ReturnStmt>(s))
            Returns.push_back(cast<ReturnStmt>(s));

        if (isa<BreakStmt>(s))
            Breaks.push_back(cast<BreakStmt>(s));
        return true;
    }

    void Clear() {
        Returns.clear(); Breaks.clear();
    }

    const std::vector<const ReturnStmt *> RetrieveReturns() {
        return Returns;
    }

    const std::vector<const BreakStmt *> RetrieveBreaks() {
        return Breaks;
    }

private:
    std::vector<const ReturnStmt *> Returns;
    std::vector<const BreakStmt *> Breaks;
};

class LoopHandler : public MatchFinder::MatchCallback {
public:
    LoopHandler(Rewriter &rewrite) : rewrite(rewrite) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        const Stmt * s = Result.Nodes.getNodeAs<Stmt>("s");

        rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(s->getLocStart()),
                           "MC2_enterLoop();\n", true, true);

        // annotate all returns with MC2_exitLoop()
        // annotate all breaks that aren't further nested with MC2_exitLoop().
        FindReturnsBreaksVisitor frbv;
        if (isa<ForStmt>(s))
            frbv.TraverseStmt(const_cast<Stmt *>(cast<ForStmt>(s)->getBody()));
        if (isa<WhileStmt>(s))
            frbv.TraverseStmt(const_cast<Stmt *>(cast<WhileStmt>(s)->getBody()));
        if (isa<DoStmt>(s))
            frbv.TraverseStmt(const_cast<Stmt *>(cast<DoStmt>(s)->getBody()));

        for (auto & r : frbv.RetrieveReturns()) {
            rewrite.InsertText(r->getLocStart(), "MC2_exitLoop();\n", true, true);
        }
        
        // need to find all breaks and returns embedded inside the loop

        rewrite.InsertTextAfterToken(rewrite.getSourceMgr().getExpansionLoc(s->getLocEnd().getLocWithOffset(1)),
                                     "\nMC2_exitLoop();\n");
    }

private:
    Rewriter &rewrite;
};

/* Inserts MC2_function for any variables which are subsequently used by the model checker, as long as they depend on MC-visible [currently: read] state. */
class AssignHandler : public MatchFinder::MatchCallback {
public:
    AssignHandler(Rewriter &rewrite, std::set<const NamedDecl *> &DeclsRead,
                  std::set<const NamedDecl *> &DeclsInCond,
                  std::set<const VarDecl *> &DeclsNeedingMC,
                  std::map<const NamedDecl *, std::string> &DeclToMCVar,
                  std::set<const Stmt *> &StmtsHandled,
                  std::set<const Expr *> &MallocExprs,
                  std::vector<Update *> &DeferredUpdates) :
        rewrite(rewrite),
        DeclsRead(DeclsRead),
        DeclsInCond(DeclsInCond),
        DeclsNeedingMC(DeclsNeedingMC),
        DeclToMCVar(DeclToMCVar),
        StmtsHandled(StmtsHandled),
        MallocExprs(MallocExprs),
        DeferredUpdates(DeferredUpdates) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        BinaryOperator * op = const_cast<BinaryOperator *>(Result.Nodes.getNodeAs<BinaryOperator>("op"));
        const Stmt * s = Result.Nodes.getNodeAs<Stmt>("containingStmt");
        FindLocalsVisitor locals, locals_rhs;

        const VarDecl * lhs = NULL;
        const Expr * rhs = NULL;
        const DeclStmt * ds;

        if (s && (ds = dyn_cast<DeclStmt>(s))) {
            // long term goal: refactor the run() method to deal with one assignment at a time
            // for now, if there is only declarations and no rhs's, we'll ignore this stmt
            if (!ds->isSingleDecl()) {
                for (auto & d : ds->decls()) {
                    VarDecl * vd = dyn_cast<VarDecl>(d);
                    if (!d || vd->hasInit())
                        assert(0 && "unsupported form of decl");
                }
                return;
            }

            lhs = retrieveSingleDecl(ds);
        }

        if (StmtsHandled.find(ds) != StmtsHandled.end() || StmtsHandled.find(op) != StmtsHandled.end())
            return;

        if (lhs) {
            if (lhs->hasInit()) {
                rhs = lhs->getInit();
                if (rhs) {
                    rhs = rhs->IgnoreCasts();
                }
            }
            else
                return;
        }
        std::set<std::string> mcState;

        bool lhsUsedInCond;
        bool rhsRead = false;

        bool lhsTooComplicated = false;
        if (op) {
            DeclRefExpr * vd;
            if ((vd = dyn_cast<DeclRefExpr>(op->getLHS())))
                lhs = dyn_cast<VarDecl>(vd->getDecl());
            else {
                // kick the can along...
                lhsTooComplicated = true;
            }

            rhs = op->getRHS();
            if (rhs) 
                rhs = rhs->IgnoreCasts();
        }

        // rhs must be MC-active state, i.e. in declsread
        // lhs must be subsequently used in (1) store/load or (2) branch condition or (3) other functions and (3a) uses values from other functions or (3b) uses values from loads, stores, or phi functions

        if (rhs) {
            locals_rhs.TraverseStmt(const_cast<Stmt *>(cast<Stmt>(rhs)));
            for (auto & nd : locals_rhs.RetrieveVars()) {
                if (DeclsRead.find(nd) != DeclsRead.end())
                    rhsRead = true;
            }
        }

        locals.TraverseStmt(const_cast<Stmt *>(cast<Stmt>(rhs)));

        lhsUsedInCond = DeclsInCond.find(lhs) != DeclsInCond.end();
        if (lhsUsedInCond) {
            for (auto & d : locals.RetrieveVars()) {
                if (DeclToMCVar.count(d) > 0)
                    mcState.insert(DeclToMCVar[d]);
                else if (DeclsRead.find(d) != DeclsRead.end())
                    mcState.insert(encode(d->getName().str()));
            }
        }
        if (rhsRead) {
            for (auto & d : locals_rhs.RetrieveVars()) {
                if (DeclToMCVar.count(d) > 0)
                    mcState.insert(DeclToMCVar[d]);
                else if (DeclsRead.find(d) != DeclsRead.end())
                    mcState.insert(encode(d->getName().str()));
            }
        }
        if (mcState.size() > 0 || MallocExprs.find(rhs) != MallocExprs.end()) {
            if (lhsTooComplicated)
                assert(0 && "couldn't find LHS of = operator");

            std::stringstream nol;
            std::string _lhsStr, lhsStr;
            std::string mcVar = encodeFn(fnCount++);
            if (lhs) {
                lhsStr = lhs->getName().str();
                _lhsStr = encode(lhsStr);
                DeclToMCVar[lhs] = mcVar;
                DeclsNeedingMC.insert(cast<VarDecl>(lhs));
            }
            int function_id = 0;
            if (!(MallocExprs.find(rhs) != MallocExprs.end()))
                function_id = ++funcCount;
            nol << "\n" << mcVar << " = MC2_function_id(" << function_id << ", " << mcState.size();
            if (lhs)
                nol << ", sizeof (" << lhsStr << "), (uint64_t)" << lhsStr;
            else 
                nol << ", MC2_PTR_LENGTH";
            for (auto & d : mcState) {
                nol <<  ", ";
                if (_lhsStr == d)
                    nol << mcVar;
                else
                    nol << d;
            }
            nol << "); ";
            SourceLocation place;
            if (op) {
                place = Lexer::getLocForEndOfToken(op->getLocEnd(), 0, rewrite.getSourceMgr(), rewrite.getLangOpts()).getLocWithOffset(1);
            } else
                place = s->getLocEnd();
            rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(place.getLocWithOffset(1)),
                               nol.str(), true, true);

            updateProvisionalName(DeferredUpdates, lhs, mcVar);
        }
    }

    private:
    Rewriter &rewrite;
    std::set<const NamedDecl *> &DeclsRead, &DeclsInCond;
    std::set<const VarDecl *> &DeclsNeedingMC;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::set<const Stmt *> &StmtsHandled;
    std::set<const Expr *> &MallocExprs;
    std::vector<Update *> &DeferredUpdates;
};

// record vars used in conditions
class BranchConditionRefactoringHandler : public MatchFinder::MatchCallback {
public:
    BranchConditionRefactoringHandler(Rewriter &rewrite,
                                      std::set<const NamedDecl *> & DeclsInCond,
                                      std::map<const NamedDecl *, std::string> &DeclToMCVar,
                                      std::map<const Expr *, std::string> &ExprToMCVar,
                                      std::map<const Expr *, SourceLocation> &Redirector,
                                      std::vector<Update *> &DeferredUpdates) :
        rewrite(rewrite), DeclsInCond(DeclsInCond), DeclToMCVar(DeclToMCVar),
        ExprToMCVar(ExprToMCVar), Redirector(Redirector), DeferredUpdates(DeferredUpdates) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        IfStmt * is = const_cast<IfStmt *>(Result.Nodes.getNodeAs<IfStmt>("if"));
        Expr * cond = is->getCond();

        // refactor out complicated conditions
        FindCallArgVisitor flv;
        flv.TraverseStmt(cond);
        std::string mcVar;

        BinaryOperator * bc = const_cast<BinaryOperator *>(Result.Nodes.getNodeAs<BinaryOperator>("bc"));
        if (bc) {
            std::string condVar = encodeCond(condCount++);
            std::stringstream condVarEncoded;
            condVarEncoded << condVar << "_m";

            // prettyprint the binary op
            // e.g. int _cond0 = x == y;
            std::string SStr;
            llvm::raw_string_ostream S(SStr);
            bc->printPretty(S, nullptr, rewrite.getLangOpts());
            const std::string &Str = S.str();
            
            std::stringstream prel;

            bool is_equality = false;
            // handle equality tests
            if (bc->getOpcode() == BO_EQ) {
                Expr * lhs = bc->getLHS()->IgnoreCasts(), * rhs = bc->getRHS()->IgnoreCasts();
                if (isa<DeclRefExpr>(lhs) && isa<DeclRefExpr>(rhs)) {
                    DeclRefExpr * l = dyn_cast<DeclRefExpr>(lhs), *r = dyn_cast<DeclRefExpr>(rhs);
                    is_equality = true;
                    prel << "\nMCID " << condVarEncoded.str() << ";\n";
                    std::string ld = DeclToMCVar.find(l->getDecl())->second,
                        rd = DeclToMCVar.find(r->getDecl())->second;

                    prel << "\nint " << condVar << " = MC2_equals(" <<
                        ld << ", (uint64_t)" << l->getNameInfo().getName().getAsString() << ", " <<
                        rd << ", (uint64_t)" << r->getNameInfo().getName().getAsString() << ", " <<
                        "&" << condVarEncoded.str() << ");\n";
                }
            }

            if (!is_equality) {
                prel << "\nint " << condVar << " = " << Str << ";";
                prel << "\nMCID " << condVarEncoded.str() << " = MC2_function_id(" << ++funcCount << ", 1, sizeof(" << condVar << "), " << condVar;
                const DeclRefExpr * d = flv.RetrieveDeclRefExpr();
                if (DeclToMCVar.find(d->getDecl()) != DeclToMCVar.end()) {
                    prel << ", " << DeclToMCVar[d->getDecl()];
                }
                prel << ");\n";
            }

            ExprToMCVar[cond] = condVarEncoded.str();
            rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(is->getLocStart()),
                               prel.str(), false, true);

            // rewrite the binary op with the newly-inserted var
            Expr * RO = bc->getRHS(); // used for location only

            int cl = Lexer::MeasureTokenLength(RO->getLocStart(), rewrite.getSourceMgr(), rewrite.getLangOpts());
            SourceRange SR(cond->getLocStart(), rewrite.getSourceMgr().getExpansionLoc(RO->getLocStart()).getLocWithOffset(cl-1));
            rewrite.ReplaceText(SR, condVar);
        } else {
            std::string condVar = encodeCond(condCount++);
            std::stringstream condVarEncoded;
            condVarEncoded << condVar << "_m";

            std::string SStr;
            llvm::raw_string_ostream S(SStr);
            cond->printPretty(S, nullptr, rewrite.getLangOpts());
            const std::string &Str = S.str();

            std::stringstream prel;
            prel << "\nint " << condVar << " = " << Str << ";";
            prel << "\nMCID " << condVarEncoded.str() << " = MC2_function_id(" << ++funcCount << ", 1, sizeof(" << condVar << "), " << condVar;
            std::vector<ProvisionalName *> * vars = new std::vector<ProvisionalName *>();
            const DeclRefExpr * d = flv.RetrieveDeclRefExpr();
            if (isa<VarDecl>(d->getDecl()) && DeclToMCVar.find(d->getDecl()) != DeclToMCVar.end()) {
                prel << ", " << DeclToMCVar[d->getDecl()];
            } else {
                prel << ", ";
                ProvisionalName * v = new ProvisionalName(prel.tellp(), d, 0);
                vars->push_back(v);
            }
            prel << ");\n";

            ExprToMCVar[cond] = condVarEncoded.str();
            // gross hack; should look for any callexprs in cond
            // but right now, if it's a unaryop, just manually traverse
            if (isa<UnaryOperator>(cond)) {
                Expr * e = dyn_cast<UnaryOperator>(cond)->getSubExpr();
                ExprToMCVar[e] = condVarEncoded.str();
            }
            Update * u = new Update(is->getLocStart(), prel.str(), vars);
            DeferredUpdates.push_back(u);

            // rewrite the call op with the newly-inserted var
            SourceRange SR(cond->getLocStart(), cond->getLocEnd());
            Redirector[cond] = is->getLocStart();
            rewrite.ReplaceText(SR, condVar);
        }

        std::deque<const Decl *> q;
        const NamedDecl * d = flv.RetrieveDeclRefExpr()->getDecl();
        q.push_back(d);
        while (!q.empty()) {
            const Decl * d = q.back();
            q.pop_back();
            if (isa<NamedDecl>(d))
                DeclsInCond.insert(cast<NamedDecl>(d));

            const VarDecl * vd;
            if ((vd = dyn_cast<VarDecl>(d))) {
                if (vd->hasInit()) {
                    const Expr * e = vd->getInit();
                    flv.Clear();
                    flv.TraverseStmt(const_cast<Expr *>(e));
                    const NamedDecl * d = flv.RetrieveDeclRefExpr()->getDecl();
                    q.push_back(d);
                }
            }
        }
    }

private:
    Rewriter &rewrite;
    std::set<const NamedDecl *> & DeclsInCond;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::map<const Expr *, std::string> &ExprToMCVar;
    std::map<const Expr *, SourceLocation> &Redirector;
    std::vector<Update *> &DeferredUpdates;
};

class BranchAnnotationHandler : public MatchFinder::MatchCallback {
public:
    BranchAnnotationHandler(Rewriter &rewrite,
                            std::map<const NamedDecl *, std::string> & DeclToMCVar,
                            std::map<const Expr *, std::string> & ExprToMCVar)
        : rewrite(rewrite),
          DeclToMCVar(DeclToMCVar),
          ExprToMCVar(ExprToMCVar){}
    virtual void run(const MatchFinder::MatchResult &Result) {
        IfStmt * is = const_cast<IfStmt *>(Result.Nodes.getNodeAs<IfStmt>("if"));

        // if the branch condition is interesting:
        // (but right now, not too interesting)
        Expr * cond = is->getCond()->IgnoreCasts();

        FindLocalsVisitor flv;
        flv.TraverseStmt(cond);
        if (flv.RetrieveVars().size() == 0) return;

        const NamedDecl * condVar = flv.RetrieveVars()[0];

        std::string mCondVar;
        if (ExprToMCVar.count(cond) > 0)
            mCondVar = ExprToMCVar[cond];
        else if (DeclToMCVar.count(condVar) > 0) 
            mCondVar = DeclToMCVar[condVar];
        else
            mCondVar = encode(condVar->getName());
        std::string brVar = encodeBranch(branchCount++);

        std::stringstream brline;
        brline << "MCID " << brVar << ";\n";
        rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(is->getLocStart()),
                           brline.str(), false, true);

        Stmt * ts = is->getThen(), * es = is->getElse();
        bool tHasChild = hasChild(ts);
        SourceLocation tfl;
        if (tHasChild) {
            if (isa<CompoundStmt>(ts))
                tfl = getFirstChild(ts)->getLocStart();
            else
                tfl = ts->getLocStart();
        } else
            tfl = ts->getLocStart().getLocWithOffset(1);
        SourceLocation tsl = ts->getLocEnd().getLocWithOffset(-1);

        std::stringstream tlineStart, mergeStmt, eline;

        UnaryOperator * uop = dyn_cast<UnaryOperator>(cond);
        tlineStart << brVar << " = MC2_branchUsesID(" << mCondVar << ", " << "1" << ", 2, true);\n";
        eline << brVar << " = MC2_branchUsesID(" << mCondVar << ", " << "0" << ", 2, true);";

        mergeStmt << "\tMC2_merge(" << brVar << ");\n";

        rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(tfl), tlineStart.str(), false, true);

        Stmt * tls = NULL;
        int extra_else_offset = 0;

        if (tHasChild) { tls = getLastChild(ts); }
        if (tls) extra_else_offset = 2; else extra_else_offset = 1;

        if (!tHasChild || (!isa<ReturnStmt>(tls) && !isa<BreakStmt>(tls))) {
            extra_else_offset = 0;
            rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(tsl.getLocWithOffset(1)),
                               mergeStmt.str(), true, true);
        }
        if (tHasChild && !isa<CompoundStmt>(ts)) {
            rewrite.InsertText(rewrite.getSourceMgr().getFileLoc(tls->getLocStart()), "{", false, true);
            SourceLocation tend = Lexer::findLocationAfterToken(tls->getLocEnd(), tok::semi, rewrite.getSourceMgr(), rewrite.getLangOpts(), false);
            rewrite.InsertText(tend, "}", true, true);
        }
        if (tHasChild && isa<CompoundStmt>(ts)) extra_else_offset++;

        if (es) {
            SourceLocation esl = es->getLocEnd().getLocWithOffset(-1);
            bool eHasChild = hasChild(es); 
            Stmt * els = NULL;
            if (eHasChild) els = getLastChild(es); else els = es;
            
            eline << "\n";

            SourceLocation el;
            if (eHasChild) {
                if (isa<CompoundStmt>(es))
                    el = getFirstChild(es)->getLocStart();
                else {
                    el = es->getLocStart();
                }
            } else
                el = es->getLocStart().getLocWithOffset(1);
            rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(el), eline.str(), false, true);

            if (eHasChild && !isa<CompoundStmt>(es)) {
                rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(el), "{", false, true);
                rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(es->getLocEnd().getLocWithOffset(1)), "}", true, true);
            }

            if (!eHasChild || (!isa<ReturnStmt>(els) && !isa<BreakStmt>(els)))
                rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(esl.getLocWithOffset(1)), mergeStmt.str(), true, true);
        }
        else {
            std::stringstream eCompoundLine;
            eCompoundLine << " else { " << eline.str() << mergeStmt.str() << " }";
            SourceLocation tend = Lexer::findLocationAfterToken(ts->getLocEnd(), tok::semi, rewrite.getSourceMgr(), rewrite.getLangOpts(), false);
            if (!tend.isValid())
                tend = Lexer::getLocForEndOfToken(ts->getLocEnd(), 0, rewrite.getSourceMgr(), rewrite.getLangOpts());
            rewrite.InsertText(tend.getLocWithOffset(1), eCompoundLine.str(), false, true);
        }
    }
private:

    bool hasChild(Stmt * s) {
        if (!isa<CompoundStmt>(s)) return true;
        return (!cast<CompoundStmt>(s)->body_empty());
    }

    Stmt * getFirstChild(Stmt * s) {
        assert(isa<CompoundStmt>(s) && "haven't yet added code to rewrite then/elsestmt to CompoundStmt");
        assert(!cast<CompoundStmt>(s)->body_empty());
        return *(cast<CompoundStmt>(s)->body_begin());
    }

    Stmt * getLastChild(Stmt * s) {
        CompoundStmt * cs;
        if ((cs = dyn_cast<CompoundStmt>(s))) {
            assert (!cs->body_empty());
            return cs->body_back();
        }
        return s;
    }

    Rewriter &rewrite;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::map<const Expr *, std::string> &ExprToMCVar;
};

class FunctionCallHandler : public MatchFinder::MatchCallback {
public:
    FunctionCallHandler(Rewriter &rewrite,
                        std::map<const NamedDecl *, std::string> &DeclToMCVar,
                        std::set<const FunctionDecl *> &ThreadMains)
        : rewrite(rewrite), DeclToMCVar(DeclToMCVar), ThreadMains(ThreadMains) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        CallExpr * ce = const_cast<CallExpr *>(Result.Nodes.getNodeAs<CallExpr>("callExpr"));
        Decl * d = ce->getCalleeDecl();
        NamedDecl * nd = dyn_cast<NamedDecl>(d);
        const Stmt * s = Result.Nodes.getNodeAs<Stmt>("containingStmt");
        ASTContext *Context = Result.Context;

        if (nd->getName() == "thrd_create") {
            Expr * callee0 = ce->getArg(1)->IgnoreCasts();
            UnaryOperator * callee1;
            if ((callee1 = dyn_cast<UnaryOperator>(callee0))) {
                if (callee1->getOpcode() == UnaryOperatorKind::UO_AddrOf)
                    callee0 = callee1->getSubExpr();
            }
            DeclRefExpr * callee = dyn_cast<DeclRefExpr>(callee0);
            if (!callee) return;
            FunctionDecl * fd = dyn_cast<FunctionDecl>(callee->getDecl());
            ThreadMains.insert(fd);
            return;
        }

        if (!d->hasBody())
            return;

        if (s && !ce->getCallReturnType(*Context)->isVoidType()) {
            // TODO check that the type is mc-visible also?
            const DeclStmt * ds;
            const VarDecl * lhs = NULL;
            std::string mc_rv = encodeRV(rvCount++);

            std::stringstream brline;
            brline << "MCID " << mc_rv << ";\n";
            rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(s->getLocStart()),
                               brline.str(), false, true);

            std::stringstream nol;
            if (ce->getNumArgs() > 0) nol << ", ";
            nol << "&" << mc_rv;
            rewrite.InsertTextBefore(rewrite.getSourceMgr().getExpansionLoc(ce->getRParenLoc()),
                                     nol.str());

            if (s && (ds = dyn_cast<DeclStmt>(s))) {
                if (!ds->isSingleDecl()) {
                    for (auto & d : ds->decls()) {
                        VarDecl * vd = dyn_cast<VarDecl>(d);
                        if (!d || vd->hasInit())
                            assert(0 && "unsupported form of decl");
                    }
                    return;
                }

                lhs = retrieveSingleDecl(ds);
            }

            DeclToMCVar[lhs] = mc_rv;
        }

        for (const auto & a : ce->arguments()) {
            std::stringstream nol;

            std::string aa = "MCID_NODEP";

            Expr * e = a->IgnoreCasts();
            DeclRefExpr * dr = dyn_cast<DeclRefExpr>(e);
            if (dr) { 
                NamedDecl * d = dr->getDecl();
                if (DeclToMCVar.find(d) != DeclToMCVar.end())
                    aa = DeclToMCVar[d];
            }

            nol << aa << ", ";
            
            if (a->getLocEnd().isValid())
                rewrite.InsertTextBefore(rewrite.getSourceMgr().getExpansionLoc(a->getLocStart()),
                                         nol.str());
        }
    }

private:
    Rewriter &rewrite;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::set<const FunctionDecl *> &ThreadMains;
};

class ReturnHandler : public MatchFinder::MatchCallback {
public:
    ReturnHandler(Rewriter &rewrite,
                  std::map<const NamedDecl *, std::string> &DeclToMCVar,
                  std::set<const FunctionDecl *> &ThreadMains)
        : rewrite(rewrite), DeclToMCVar(DeclToMCVar), ThreadMains(ThreadMains) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        const FunctionDecl * fd = Result.Nodes.getNodeAs<FunctionDecl>("containingFunction");
        ReturnStmt * rs = const_cast<ReturnStmt *>(Result.Nodes.getNodeAs<ReturnStmt>("returnStmt"));
        Expr * rv = const_cast<Expr *>(rs->getRetValue());

        if (!rv) return;        
        if (ThreadMains.find(fd) != ThreadMains.end()) return;
        // not sure why this is explicitly needed, but crashes without it
        if (!fd->getIdentifier() || fd->getName() == "user_main") return;

        FindLocalsVisitor flv;
        flv.TraverseStmt(rv);
        std::string mrv = "MCID_NODEP";

        if (flv.RetrieveVars().size() > 0) {
            const NamedDecl * returnVar = flv.RetrieveVars()[0];
            if (DeclToMCVar.find(returnVar) != DeclToMCVar.end()) {
                mrv = DeclToMCVar[returnVar];
            }
        }
        std::stringstream nol;
        nol << "*retval = " << mrv << ";\n";
        rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(rs->getLocStart()),
                           nol.str(), false, true);
    }

private:
    Rewriter &rewrite;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::set<const FunctionDecl *> &ThreadMains;
};

class VarDeclHandler : public MatchFinder::MatchCallback {
public:
    VarDeclHandler(Rewriter &rewrite,
                   std::map<const NamedDecl *, std::string> &DeclToMCVar,
                   std::set<const VarDecl *> &DeclsNeedingMC)
        : rewrite(rewrite), DeclToMCVar(DeclToMCVar), DeclsNeedingMC(DeclsNeedingMC) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        VarDecl * d = const_cast<VarDecl *>(Result.Nodes.getNodeAs<VarDecl>("d"));
        std::stringstream nol;

        if (DeclsNeedingMC.find(d) == DeclsNeedingMC.end()) return;

        std::string dn;
        if (DeclToMCVar.find(d) != DeclToMCVar.end())
            dn = DeclToMCVar[d];
        else
            dn = encode(d->getName().str());

        nol << "MCID " << dn << "; ";

        if (d->getLocStart().isValid())
            rewrite.InsertTextBefore(rewrite.getSourceMgr().getExpansionLoc(d->getLocStart()),
                                     nol.str());
    }

private:
    Rewriter &rewrite;
    std::map<const NamedDecl *, std::string> &DeclToMCVar;
    std::set<const VarDecl *> &DeclsNeedingMC;
};

class FunctionDeclHandler : public MatchFinder::MatchCallback {
public:
    FunctionDeclHandler(Rewriter &rewrite,
                        std::set<const FunctionDecl *> &ThreadMains)
        : rewrite(rewrite), ThreadMains(ThreadMains) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
        FunctionDecl * fd = const_cast<FunctionDecl *>(Result.Nodes.getNodeAs<FunctionDecl>("fd"));

        if (!fd->getIdentifier()) return;

        if (fd->getName() == "user_main") { ThreadMains.insert(fd); return; }

        if (ThreadMains.find(fd) != ThreadMains.end()) return;

        SourceLocation LastParam = fd->getNameInfo().getLocStart().getLocWithOffset(fd->getName().size()).getLocWithOffset(1);
        for (auto & p : fd->params()) {
            std::stringstream nol;
            nol << "MCID " << encode(p->getName()) << ", ";
            if (p->getLocStart().isValid())
                rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(p->getLocStart()),
                                   nol.str(), false);
            if (p->getLocEnd().isValid())
                LastParam = p->getLocEnd().getLocWithOffset(p->getName().size());
        }

        if (!fd->getReturnType()->isVoidType()) {
            std::stringstream nol;
            if (fd->param_size() > 0) nol << ", ";
            nol << "MCID * retval";
            rewrite.InsertText(rewrite.getSourceMgr().getExpansionLoc(LastParam),
                               nol.str(), false);
        }
    }

private:
    Rewriter &rewrite;
    std::set<const FunctionDecl *> &ThreadMains;
};

class BailHandler : public MatchFinder::MatchCallback {
public:
    BailHandler() {}
    virtual void run(const MatchFinder::MatchResult &Result) {
        assert(0 && "we don't handle goto statements");
    }
};

class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(Rewriter &R) : R(R),
                                 DeclsRead(),
                                 DeclsInCond(),
                                 DeclToMCVar(),
                                 HandlerMalloc(MallocExprs),
                                 HandlerLoad(R, DeclsRead, DeclsNeedingMC, DeclToMCVar, ExprToMCVar, StmtsHandled, Redirector, DeferredUpdates),
                                 HandlerStore(R, DeclsRead, DeclsNeedingMC, DeferredUpdates),
                                 HandlerRMW(R, DeclsRead, DeclsInCond, DeclToMCVar, ExprToMCVar, StmtsHandled, Redirector, DeferredUpdates),
                                 HandlerLoop(R),
                                 HandlerBranchConditionRefactoring(R, DeclsInCond, DeclToMCVar, ExprToMCVar, Redirector, DeferredUpdates),
                                 HandlerAssign(R, DeclsRead, DeclsInCond, DeclsNeedingMC, DeclToMCVar, StmtsHandled, MallocExprs, DeferredUpdates),
                                 HandlerAnnotateBranch(R, DeclToMCVar, ExprToMCVar),
                                 HandlerFunctionDecl(R, ThreadMains),
                                 HandlerFunctionCall(R, DeclToMCVar, ThreadMains),
                                 HandlerReturn(R, DeclToMCVar, ThreadMains),
                                 HandlerVarDecl(R, DeclToMCVar, DeclsNeedingMC),
                                 HandlerBail() {
        MatcherFunctionCall.addMatcher(callExpr(anyOf(hasParent(compoundStmt()),
                                                      hasAncestor(varDecl(hasParent(stmt().bind("containingStmt")))),
                                                      hasAncestor(binaryOperator(hasOperatorName("=")).bind("containingStmt")))).bind("callExpr"),
                                       &HandlerFunctionCall);
        MatcherLoadStore.addMatcher
            (callExpr(callee(functionDecl(anyOf(hasName("malloc"), hasName("calloc"))))).bind("callExpr"),
             &HandlerMalloc);

        MatcherLoadStore.addMatcher
            (callExpr(callee(functionDecl(anyOf(hasName("load_32"), hasName("load_64")))),
                      anyOf(hasAncestor(varDecl(hasParent(stmt().bind("containingStmt"))).bind("decl")),
                            hasAncestor(binaryOperator(hasOperatorName("=")).bind("containingStmt")),
                            hasParent(stmt().bind("containingStmt"))))
             .bind("callExpr"),
             &HandlerLoad);

        MatcherLoadStore.addMatcher(callExpr(callee(functionDecl(anyOf(hasName("store_32"), hasName("store_64"))))).bind("callExpr"),
                                    &HandlerStore);

        MatcherLoadStore.addMatcher
            (callExpr(callee(functionDecl(anyOf(hasName("rmw_32"), hasName("rmw_64")))),
                      anyOf(hasAncestor(varDecl(hasParent(stmt().bind("containingStmt"))).bind("decl")),
                            hasAncestor(binaryOperator(hasOperatorName("="),
                                                       hasLHS(declRefExpr().bind("lhs"))).bind("containingStmt")),
                            anything()))
             .bind("callExpr"),
             &HandlerRMW);

        MatcherLoadStore.addMatcher(ifStmt(hasCondition
                                           (anyOf(binaryOperator().bind("bc"),
                                                  hasDescendant(callExpr(callee(functionDecl(anyOf(hasName("load_32"), hasName("load_64"))))).bind("callExpr")),
                                                  hasDescendant(callExpr(callee(functionDecl(anyOf(hasName("store_32"), hasName("store_64"))))).bind("callExpr")),
                                                  hasDescendant(callExpr(callee(functionDecl(anyOf(hasName("rmw_32"), hasName("rmw_64"))))).bind("callExpr")),
                                                  anything()))).bind("if"),
                                    &HandlerBranchConditionRefactoring);

        MatcherLoadStore.addMatcher(forStmt().bind("s"),
                                    &HandlerLoop);
        MatcherLoadStore.addMatcher(whileStmt().bind("s"),
                                    &HandlerLoop);
        MatcherLoadStore.addMatcher(doStmt().bind("s"),
                                    &HandlerLoop);

        MatcherFunction.addMatcher(binaryOperator(anyOf(hasAncestor(declStmt().bind("containingStmt")),
                                                        hasParent(compoundStmt())),
                                                        hasOperatorName("=")).bind("op"),
                                   &HandlerAssign);
        MatcherFunction.addMatcher(declStmt().bind("containingStmt"), &HandlerAssign);

        MatcherFunction.addMatcher(ifStmt().bind("if"),
                                   &HandlerAnnotateBranch);

        MatcherFunctionDecl.addMatcher(functionDecl().bind("fd"),
                                       &HandlerFunctionDecl);
        MatcherFunctionDecl.addMatcher(varDecl().bind("d"), &HandlerVarDecl);
        MatcherFunctionDecl.addMatcher(returnStmt(hasAncestor(functionDecl().bind("containingFunction"))).bind("returnStmt"),
                                   &HandlerReturn);

        MatcherSanity.addMatcher(gotoStmt(), &HandlerBail);
    }

    // Override the method that gets called for each parsed top-level
    // declaration.
    void HandleTranslationUnit(ASTContext &Context) override {
        LangOpts = Context.getLangOpts();

        MatcherFunctionCall.matchAST(Context);
        MatcherLoadStore.matchAST(Context);
        MatcherFunction.matchAST(Context);
        MatcherFunctionDecl.matchAST(Context);
        MatcherSanity.matchAST(Context);

        for (auto & u : DeferredUpdates) {
            R.InsertText(R.getSourceMgr().getExpansionLoc(u->loc), u->update, true, true);
            delete u;
        }
        DeferredUpdates.clear();
    }

private:
    /* DeclsRead contains all local variables 'x' which:
    * 1) appear in 'x = load_32(...);
    * 2) appear in 'y = store_32(x); */
    std::set<const NamedDecl *> DeclsRead;
    /* DeclsInCond contains all local variables 'x' used in a branch condition or rmw parameter */
    std::set<const NamedDecl *> DeclsInCond;
    std::map<const NamedDecl *, std::string> DeclToMCVar;
    std::map<const Expr *, std::string> ExprToMCVar;
    std::set<const VarDecl *> DeclsNeedingMC;
    std::set<const FunctionDecl *> ThreadMains;
    std::set<const Stmt *> StmtsHandled;
    std::set<const Expr *> MallocExprs;
    std::map<const Expr *, SourceLocation> Redirector;
    std::vector<Update *> DeferredUpdates;

    Rewriter &R;

    MallocHandler HandlerMalloc;
    LoadHandler HandlerLoad;
    StoreHandler HandlerStore;
    RMWHandler HandlerRMW;
    LoopHandler HandlerLoop;
    BranchConditionRefactoringHandler HandlerBranchConditionRefactoring;
    BranchAnnotationHandler HandlerAnnotateBranch;
    AssignHandler HandlerAssign;
    FunctionDeclHandler HandlerFunctionDecl;
    FunctionCallHandler HandlerFunctionCall;
    ReturnHandler HandlerReturn;
    VarDeclHandler HandlerVarDecl;
    BailHandler HandlerBail;
    MatchFinder MatcherLoadStore, MatcherFunction, MatcherFunctionDecl, MatcherFunctionCall, MatcherSanity;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
    MyFrontendAction() {}
    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        llvm::errs() << "** EndSourceFileAction for: "
                     << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";

        // Now emit the rewritten buffer.
        TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef file) override {
        llvm::errs() << "** Creating AST consumer for: " << file << "\n";
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return llvm::make_unique<MyASTConsumer>(TheRewriter);
    }

private:
    Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
    CommonOptionsParser op(argc, argv, AddMC2AnnotationsCategory);
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());
    
    return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
